#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include "GUI/MainWindow.h"
#include "GUI/Settings/SConfig.h"

#include "DolphinProcess/DolphinAccessor.h"

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  QApplication::setApplicationName("Dolphin Memory Engine");
  QApplication::setApplicationVersion("1.2.2");

  SConfig config;  // Initialize global settings object

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QObject::tr("A RAM search made specifically to search, monitor and edit "
                  "the Dolphin Emulator's emulated memory."));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption dolphinProcessNameOption(
      QStringList() << "d" << "dolphin-process-name",
      QObject::tr("Specify custom name for the Dolphin Emulator process. By default, "
                  "platform-specific names are used (e.g. \"Dolphin.exe\" on Windows, or "
                  "\"dolphin-emu\" on Linux or macOS)."),
      "dolphin_process_name");
  parser.addOption(dolphinProcessNameOption);

  const QCommandLineOption dolphinAddressOption(
      QStringList() << "a" << "address",
      QObject::tr("Specify custom address to read values from."),"address");
  parser.addOption(dolphinAddressOption);

  parser.process(app);

  const QString dolphinProcessName{parser.value(dolphinProcessNameOption)};
  const QString dolphinAddressName{parser.value(dolphinAddressOption)};
  if (!dolphinProcessName.isEmpty())
  {
    qputenv("DME_DOLPHIN_PROCESS_NAME", dolphinProcessName.toStdString().c_str());
  }

  if (!dolphinAddressName.isEmpty()) {
    DolphinComm::DolphinAccessor::hook();
    if (DolphinComm::DolphinAccessor::getStatus() != DolphinComm::DolphinAccessor::DolphinStatus::hooked) {
      qDebug() << "Failed to hook to Dolphin.";
      return 1;
    }

    u32 crashAddress = static_cast<u32>(std::stoul(dolphinAddressName.toStdString(), nullptr, 16));

    u32 offset = crashAddress - 0x80000000;
    u32 crashFlag;
    char crashFlagBuffer[sizeof(u32)];

    DolphinComm::DolphinAccessor::readFromRAM(offset, crashFlagBuffer, 4, true);
    std::memcpy(&crashFlag, crashFlagBuffer, sizeof(u32));

    qDebug() << "Crash flag: " << crashFlag;

    if (crashFlag == 1) {
      // if this is 1 then myExceptionCallback ran
      qDebug() << "Game crashed!";
      return 1;
    }
    return 0;
  } else {

    MainWindow window;

    if (!config.ownsSettingsFile())
    {
      QMessageBox box(
          QMessageBox::Warning, QObject::tr("Another instance is already running"),
          QObject::tr(
              "Changes made to settings will not be preserved in this session. This includes changes "
              "to the watch list, which will need to be saved manually into a file."),
          QMessageBox::Ok);
      box.setWindowIcon(window.windowIcon());
      box.exec();
    }

    window.show();
    return QApplication::exec();
  }
}
