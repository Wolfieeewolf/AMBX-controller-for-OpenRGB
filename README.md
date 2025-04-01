# OpenRGB AMBX Controller

An OpenRGB controller implementation for Philips amBX Gaming lights.

## Overview

This controller enables OpenRGB to control Philips amBX gaming peripherals, including the left/right lights and the wallwasher with its three lighting zones. The implementation supports multiple amBX units connected simultaneously.

## Features

- Full control of all 5 lights in the amBX system
- Direct per-LED color control
- Zone-based control (Side Lights and Wallwasher zones)
- Support for multiple amBX units connected simultaneously

## Requirements

- OpenRGB (latest version recommended)
- libusb (included with OpenRGB)
- **WinUSB driver** installed via Zadig (Windows only)

### Installing WinUSB Driver (Windows only)

The amBX device requires WinUSB drivers to allow libusb to communicate with it:

1. Download Zadig from [zadig.akeo.ie](https://zadig.akeo.ie/)
2. Connect your amBX device
3. Run Zadig
4. Select "Options" → "List All Devices"
5. Find your Philips amBX device in the dropdown
6. Select "WinUSB" as the driver
7. Click "Install Driver" or "Replace Driver"

## Installation

1. Copy the AMBXController folder to the `Controllers` directory in your OpenRGB source code
2. Build OpenRGB with your preferred method
3. Launch OpenRGB

## Usage Notes

- The device must be connected before starting OpenRGB for reliable detection
- If you connect the device after OpenRGB is running, restart OpenRGB to detect it
- Multiple amBX units will appear as "Philips amBX", "Philips amBX 2", etc.

## Troubleshooting

- If the device isn't detected, ensure the WinUSB driver is properly installed
- If OpenRGB crashes, make sure no other software is controlling the amBX device
- If you encounter USB communication errors, try a different USB port

## License

GPL-2.0-only (same as OpenRGB)
