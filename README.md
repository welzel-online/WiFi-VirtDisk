## WiFi-VirtDisk ##

The WiFi-VirtDisk is a network-based storage solution that replaces or supplements the classic SD card approach. The hardware consists of an ESP8266 (D1 mini), which is connected to the SPI port (SD card).

Unlike SD card support, the WiFi-VirtDisk relies on its own SPI protocol, which is fully implemented in the ESP8266. This communicates directly with the IOS of the Z80-MBC2, which I have also adapted. There, the user can flexibly choose whether they want to work with a local SD card or via the WiFi-VirtDisk.
The ESP8266 then communicates via WLAN to a server programme that runs on both Windows 10/11 and Linux.

Technical overview:
- Own SPI protocol instead of SD emulation
- Switchable in the IOS between SD card and WiFi-VirtDisk
- WLAN connection via ESP8266 to a self-developed server programme
- Virtual disk images are stored centrally on the server and appear in the CP/M system like classic drives
- A virtual disk image is generated from a directory on the server.
- Extensibility: the proprietary protocol leaves room for future features

https://www.welzel-online.ch
