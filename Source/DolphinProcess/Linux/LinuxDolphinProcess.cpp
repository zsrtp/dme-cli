#ifdef __linux__

#include "LinuxDolphinProcess.h"
#include "../../Common/CommonUtils.h"
#include "../../Common/MemoryCommon.h"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/uio.h>
#include <vector>

namespace DolphinComm
{
bool LinuxDolphinProcess::obtainEmuRAMInformations()
{
  std::ifstream theMapsFile("/proc/" + std::to_string(m_PID) + "/maps");
  std::string line;
  bool MEM1Found = false;
  while (getline(theMapsFile, line))
  {
    std::vector<std::string> lineData;
    std::string token;
    std::stringstream ss(line);
    while (getline(ss, token, ' '))
    {
      if (!token.empty())
        lineData.push_back(token);
    }

    if (lineData.size() < 3)
      continue;

    bool foundDevShmDolphin = false;
    for (const std::string& str : lineData)
    {
      if (str.substr(0, 19) == "/dev/shm/dolphinmem" || str.substr(0, 20) == "/dev/shm/dolphin-emu")
      {
        foundDevShmDolphin = true;
        break;
      }
    }

    if (!foundDevShmDolphin)
      continue;

    std::string offsetStr("0x" + lineData[2]);
    const u32 offset{static_cast<u32>(std::stoul(offsetStr, nullptr, 16))};
    if (offset != 0 && offset != Common::GetMEM1Size() + 0x40000)
      continue;

    u64 firstAddress = 0;
    u64 SecondAddress = 0;
    const std::size_t indexDash{lineData[0].find('-')};
    std::string firstAddressStr("0x" + lineData[0].substr(0, indexDash));
    std::string secondAddressStr("0x" + lineData[0].substr(indexDash + 1));

    firstAddress = std::stoul(firstAddressStr, nullptr, 16);
    SecondAddress = std::stoul(secondAddressStr, nullptr, 16);

    if (SecondAddress - firstAddress == Common::GetMEM2Size() &&
        offset == Common::GetMEM1Size() + 0x40000)
    {
      m_MEM2AddressStart = firstAddress;
      m_MEM2Present = true;
      if (MEM1Found)
        break;
    }

    if (SecondAddress - firstAddress == Common::GetMEM1Size())
    {
      if (offset == 0x0)
      {
        m_emuRAMAddressStart = firstAddress;
        MEM1Found = true;
      }
      else if (offset == Common::GetMEM1Size() + 0x40000)
      {
        m_emuARAMAdressStart = firstAddress;
        m_ARAMAccessible = true;
      }
    }
  }

  // On Wii, there is no concept of speedhack so act as if we couldn't find it
  if (m_MEM2Present)
  {
    m_emuARAMAdressStart = 0;
    m_ARAMAccessible = false;
  }

  if (m_emuRAMAddressStart != 0)
    return true;

  // Here, Dolphin appears to be running, but the emulator isn't started
  return false;
}

bool LinuxDolphinProcess::findPID()
{
  DIR* directoryPointer = opendir("/proc/");
  if (directoryPointer == nullptr)
    return false;

  static const char* const s_dolphinProcessName{std::getenv("DME_DOLPHIN_PROCESS_NAME")};

  m_PID = -1;
  struct dirent* directoryEntry = nullptr;
  while (m_PID == -1 && (directoryEntry = readdir(directoryPointer)))
  {
    const char* const name{static_cast<const char*>(directoryEntry->d_name)};
    std::istringstream conversionStream(name);
    int aPID = 0;
    if (!(conversionStream >> aPID))
      continue;
    std::ifstream aCmdLineFile;
    std::string line;
    aCmdLineFile.open("/proc/" + std::string(name) + "/comm");
    getline(aCmdLineFile, line);

    const bool match{s_dolphinProcessName ? line == s_dolphinProcessName :
                                            (line == "dolphin-emu" || line == "dolphin-emu-qt2" ||
                                             line == "dolphin-emu-wx")};
    if (match)
      m_PID = aPID;

    aCmdLineFile.close();
  }
  closedir(directoryPointer);

  const bool running{m_PID != -1};
  return running;
}

bool LinuxDolphinProcess::readFromRAM(const u32 offset, char* buffer, const size_t size,
                                      const bool withBSwap)
{
  u64 RAMAddress = 0;
  if (m_ARAMAccessible)
  {
    if (offset >= Common::ARAM_FAKESIZE) {
      RAMAddress = m_emuRAMAddressStart + offset - Common::ARAM_FAKESIZE;
    } else {
      RAMAddress = m_emuRAMAddressStart + offset;
    }
      
  }
  else if (offset >= (Common::MEM2_START - Common::MEM1_START))
  {
    RAMAddress = m_MEM2AddressStart + offset - (Common::MEM2_START - Common::MEM1_START);
  }
  else
  {
    RAMAddress = m_emuRAMAddressStart + offset;
  }

  iovec local{};
  local.iov_base = buffer;
  local.iov_len = size;

  iovec remote{};
  remote.iov_base = reinterpret_cast<void*>(RAMAddress);
  remote.iov_len = size;

  const ssize_t nread{process_vm_readv(m_PID, &local, 1, &remote, 1, 0)};
  if (nread == -1)
  {
    // A more specific error type should be available in `errno` (if ever interested).
    return false;
  }

  if (static_cast<size_t>(nread) != size)
    return false;

  if (withBSwap)
  {
    switch (size)
    {
    case 2:
    {
      u16 halfword = 0;
      std::memcpy(&halfword, buffer, sizeof(u16));
      halfword = Common::bSwap16(halfword);
      std::memcpy(buffer, &halfword, sizeof(u16));
      break;
    }
    case 4:
    {
      u32 word = 0;
      std::memcpy(&word, buffer, sizeof(u32));
      word = Common::bSwap32(word);
      std::memcpy(buffer, &word, sizeof(u32));
      break;
    }
    case 8:
    {
      u64 doubleword = 0;
      std::memcpy(&doubleword, buffer, sizeof(u64));
      doubleword = Common::bSwap64(doubleword);
      std::memcpy(buffer, &doubleword, sizeof(u64));
      break;
    }
    default:
      break;
    }
  }

  return true;
}

bool LinuxDolphinProcess::writeToRAM(const u32 offset, const char* buffer, const size_t size,
                                     const bool withBSwap)
{
  u64 RAMAddress = 0;
  if (m_ARAMAccessible)
  {
    if (offset >= Common::ARAM_FAKESIZE)
      RAMAddress = m_emuRAMAddressStart + offset - Common::ARAM_FAKESIZE;
    else
      RAMAddress = m_emuARAMAdressStart + offset;
  }
  else if (offset >= (Common::MEM2_START - Common::MEM1_START))
  {
    RAMAddress = m_MEM2AddressStart + offset - (Common::MEM2_START - Common::MEM1_START);
  }
  else
  {
    RAMAddress = m_emuRAMAddressStart + offset;
  }

  char* bufferCopy = new char[size];
  std::memcpy(bufferCopy, buffer, size);

  iovec local{};
  local.iov_base = bufferCopy;
  local.iov_len = size;

  iovec remote{};
  remote.iov_base = reinterpret_cast<void*>(RAMAddress);
  remote.iov_len = size;

  if (withBSwap)
  {
    switch (size)
    {
    case 2:
    {
      u16 halfword = 0;
      std::memcpy(&halfword, bufferCopy, sizeof(u16));
      halfword = Common::bSwap16(halfword);
      std::memcpy(bufferCopy, &halfword, sizeof(u16));
      break;
    }
    case 4:
    {
      u32 word = 0;
      std::memcpy(&word, bufferCopy, sizeof(u32));
      word = Common::bSwap32(word);
      std::memcpy(bufferCopy, &word, sizeof(u32));
      break;
    }
    case 8:
    {
      u64 doubleword = 0;
      std::memcpy(&doubleword, bufferCopy, sizeof(u64));
      doubleword = Common::bSwap64(doubleword);
      std::memcpy(bufferCopy, &doubleword, sizeof(u64));
      break;
    }
    default:
      break;
    }
  }

  const ssize_t nwrote{process_vm_writev(m_PID, &local, 1, &remote, 1, 0)};
  delete[] bufferCopy;

  if (nwrote == -1)
  {
    // A more specific error type should be available in `errno` (if ever interested).
    return false;
  }

  return static_cast<size_t>(nwrote) == size;
}
}  // namespace DolphinComm
#endif
