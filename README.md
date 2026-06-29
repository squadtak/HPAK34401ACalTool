# HP/Agilent/Keysight 34401A EEPROM backup tool

Calibration EEPROM backup tool for HPAK 34401A

connect via RS-232. 9600 8N "2"

```
USAGE: HPAK34401ACalTool [mode] [-cp|--com-port [COM|com<decimal>]] [-fp|--file-path "<path>"]

mode:
 -h, --help             display this help and exit.
 -dm, --dump-mode       Dump Calibration from 34401A to computer.
                        If the --file-path option is not specified, the default output path is used.
                        ("currentDirectory\caldump_xx-xx-xx.bin")
 -fm, --flash-mode      Flashing Calibration from computer to 34401A. (not implemented)

If no mode is specified, the default is dump mode.
If no port is specified, it searches the 34401A among the available ports.

Example:
HPAK34401ACalTool
HPAK34401ACalTool --dump-mode --com-port COM3
HPAK34401ACalTool -dm -cp COM3 -fp "<filepath>"
HPAK34401ACalTool -fm -cp COM3 -fp "<filepath>"
```

command references:

https://www.eevblog.com/forum/testgear/hp-agilent-34401a-hidden-menu/msg4436983/#msg4436983

## Dependencies
libserialport by sigrokproject https://github.com/sigrokproject/libserialport

libhp34401A https://github.com/squadtak/libhp34401A

libfile https://github.com/squadtak/libfile

## To-Do
flashing eeprom

supporting GPIB (visa, ni488.h, ++protocol? hmm...)
