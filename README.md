# dme-cli

A modified version of [dolphin-memory-engine](https://github.com/aldelaro5/dolphin-memory-engine) that adds support to read a u32 value from an address using the CLI and see if it matches a target input value.

## Example usage

### dolphin-emu-nogui
```bash
Usage: ./dolphin-memory-engine [options]
A RAM search made specifically to search, monitor and edit the Dolphin Emulator's emulated memory.

Options:
  -h, --help                                         Displays help on
                                                     commandline options.
  --help-all                                         Displays help, including
                                                     generic Qt options.
  -v, --version                                      Displays version
                                                     information.
  -d, --dolphin-process-name <dolphin_process_name>  Specify custom name for
                                                     the Dolphin Emulator
                                                     process. By default,
                                                     platform-specific names are
                                                     used (e.g. "Dolphin.exe" on
                                                     Windows, or "dolphin-emu"
                                                     on Linux or macOS).
  -a, --address <address>                            Specify custom address to
                                                     read values from.
  -q, --value <value>                                The expected value when
                                                     the address is read.
```
Longer linux processes get truncated, so we must specify the truncated name with `-d`.

```bash
sudo dolphin-memory-engine -d dolphin-emu-nog -a 0x80450580 -q 0

Values don't match! Expected:  0  Got:  1
Exiting!
```
