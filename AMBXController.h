/*---------------------------------------------------------*\
| AMBXController.h                                          |
|                                                           |
|   Driver for Philips amBX Gaming lights                   |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
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
\*-----------------------------------------------------*/
#define AMBX_VID                            0x0471
#define AMBX_PID                            0x083F

/*-----------------------------------------------------*\
| AMBX Endpoints                                        |
\*-----------------------------------------------------*/
#define AMBX_ENDPOINT_IN                    0x81
#define AMBX_ENDPOINT_OUT                   0x02
#define AMBX_ENDPOINT_PNP                   0x83

/*-----------------------------------------------------*\
| AMBX Commands                                         |
\*-----------------------------------------------------*/
#define AMBX_PACKET_HEADER                  0xA1
#define AMBX_SET_COLOR                      0x03
#define AMBX_SET_COLOR_SEQUENCE             0x72

/*-----------------------------------------------------*\
| AMBX Lights                                           |
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

private:
    libusb_context*          usb_context;
    libusb_device_handle*    dev_handle;
    std::string              location;
    std::string              serial;
    bool                     initialized;
    
    void                    SendPacket(unsigned char* packet, unsigned int size);
};
