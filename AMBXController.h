/*---------------------------------------------------------*\
| AMBXController.h                                          |
|                                                           |
|   Driver for Philips amBX Gaming lights                   |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
|                                                           |
|   Protocol notes:                                         |
|   The amBX uses a simple USB protocol for light control   |
|   Packets are sent via interrupt transfer to endpoint 0x02|
|   All light commands use the following format:            |
|     Byte 0: Header (0xA1)                                 |
|     Byte 1: Light ID (see AMBX_LIGHT_* enum below)        |
|     Byte 2: Command (0x03 for SET_COLOR)                  |
|     Bytes 3-5: RGB value (Red, Green, Blue)               |
|                                                           |
|   The amBX system has 5 lighting zones:                   |
|   - Left satellite light                                  |
|   - Right satellite light                                 |
|   - Wallwasher left                                       |
|   - Wallwasher center                                     |
|   - Wallwasher right                                      |
|                                                           |
|   The system can be controlled in two ways:               |
|   1. Setting individual lights with their IDs             |
|   2. Setting all lights simultaneously with a sequence    |
|                                                           |
|   Compatible with both original Philips and MadCatz amBX  |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include "dependencies/libusb-1.0.27/include/libusb.h"
#else
#include <libusb.h>
#endif

/*-----------------------------------------------------*\
| AMBX VID/PID                                          |
|                                                       |
| The same VID/PID is used for both Philips and MadCatz |
| versions of the amBX system                           |
\*-----------------------------------------------------*/
#define AMBX_VID                            0x0471
#define AMBX_PID                            0x083F

/*-----------------------------------------------------*\
| AMBX Endpoints                                        |
|                                                       |
| The device uses interrupt transfers for communication |
| 0x02 is the OUT endpoint for sending commands        |
| 0x81 is the IN endpoint for receiving data           |
| 0x83 is used for PnP events                          |
\*-----------------------------------------------------*/
#define AMBX_ENDPOINT_IN                    0x81
#define AMBX_ENDPOINT_OUT                   0x02
#define AMBX_ENDPOINT_PNP                   0x83

/*-----------------------------------------------------*\
| AMBX Commands                                         |
|                                                       |
| 0xA1 - Packet header for all commands                |
| 0x03 - Set color command (followed by RGB values)    |
| 0x72 - Set timed color sequence (for animations)     |
\*-----------------------------------------------------*/
#define AMBX_PACKET_HEADER                  0xA1
#define AMBX_SET_COLOR                      0x03
#define AMBX_SET_COLOR_SEQUENCE             0x72

/*-----------------------------------------------------*\
| AMBX Lights                                           |
|                                                       |
| IDs for each of the 5 light zones:                   |
| 0x0B - Left satellite light                          |
| 0x1B - Right satellite light                         |
| 0x2B - Left section of wallwasher                    |
| 0x3B - Center section of wallwasher                  |
| 0x4B - Right section of wallwasher                   |
| 0xFF - Special value to address all lights at once   |
\*-----------------------------------------------------*/
enum
{
    AMBX_LIGHT_LEFT         = 0x0B,
    AMBX_LIGHT_RIGHT        = 0x1B,
    AMBX_LIGHT_WALL_LEFT    = 0x2B,
    AMBX_LIGHT_WALL_CENTER  = 0x3B,
    AMBX_LIGHT_WALL_RIGHT   = 0x4B,
    AMBX_LIGHT_ALL          = 0xFF
};

class AMBXController
{
public:
    AMBXController(const char* path);
    ~AMBXController();

    std::string     GetDeviceLocation();
    std::string     GetSerialString();
    
    bool            IsInitialized();
    void            SetSingleColor(unsigned int light, unsigned char red, unsigned char green, unsigned char blue);
    void            SetAllColors(RGBColor color);
    void            SetLEDColor(unsigned int led, RGBColor color);
    void            SetLEDColors(unsigned int* leds, RGBColor* colors, unsigned int count);
    void            SetMultipleColors(unsigned int* lights, RGBColor* colors, unsigned int count);

private:
    libusb_context*          usb_context;
    libusb_device_handle*    dev_handle;
    std::string              location;
    std::string              serial;
    bool                     initialized;
    bool                     interface_claimed;
    
    void                    SendPacket(unsigned char* packet, unsigned int size);
    bool                    ClaimInterface();
    void                    ReleaseInterface();
};
