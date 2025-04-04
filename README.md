# AMBX Controller for OpenRGB

This repository contains a driver for the Philips amBX Gaming lights system for use with [OpenRGB](https://openrgb.org).

## Overview

The Philips amBX Gaming lights system is an ambient lighting system that includes:
- Left and right satellite lights
- A wallwasher with three lighting zones (left, center, and right)

This driver allows OpenRGB to control these lights, providing a modern interface for hardware that might otherwise be unusable on modern systems.

## Features

- Supports all five lighting zones of the amBX system
- Compatible with both Philips and maybe MadCatz amBX hardware. MadCatz still needs testing.
- Uses standard libusb drivers instead of proprietary Jungo drivers
- Optimized protocol for efficient lighting updates

## Usage

To use this driver with OpenRGB:
1. Make sure your amBX device is connected via USB
2. Install the WinUSB driver for the device using Zadig (https://zadig.akeo.ie/)
3. Place these files in the `Controllers/AMBXController` directory of your OpenRGB installation
4. Compile OpenRGB
5. Launch OpenRGB to detect and control your amBX lights

**Note:** Installing the WinUSB driver will make the original amBX software non-functional. You'll need to use OpenRGB for controlling the lights after this change.

## Troubleshooting

If OpenRGB fails to detect your amBX device:
- Ensure the device is properly connected
- Check that the WinUSB driver is correctly installed
- Verify that no other software is currently using the device

## License

This project is licensed under GPL-2.0 as part of the OpenRGB project.
