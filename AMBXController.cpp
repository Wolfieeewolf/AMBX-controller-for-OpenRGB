/*---------------------------------------------------------*\
| AMBXController.cpp                                        |
|                                                           |
|   Driver for Philips amBX Gaming lights                   |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "AMBXController.h"
#include "LogManager.h"
#include <cstring>
#include <thread>
#include <chrono>

AMBXController::AMBXController(const char* path)
{
    initialized = false;
    usb_context = nullptr;
    dev_handle = nullptr;
    
    location = "USB amBX: ";
    location += path;
    
    // Initialize libusb in this instance
    if(libusb_init(&usb_context) != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to initialize libusb");
        return;
    }
    
    // Get the device list
    libusb_device** device_list;
    ssize_t device_count = libusb_get_device_list(usb_context, &device_list);
    
    if(device_count < 0)
    {
        LOG_ERROR("Failed to get USB device list");
        return;
    }
    
    // Find our device in the list
    for(ssize_t i = 0; i < device_count; i++)
    {
        libusb_device* device = device_list[i];
        struct libusb_device_descriptor desc;
        
        if(libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
        {
            continue;
        }
        
        if(desc.idVendor == AMBX_VID && desc.idProduct == AMBX_PID)
        {
            // Get bus and address for identifying multiple devices
            uint8_t bus = libusb_get_bus_number(device);
            uint8_t address = libusb_get_device_address(device);
            
            char device_id[32];
            sprintf(device_id, "Bus %d Addr %d", bus, address);
            location = std::string("USB amBX: ") + device_id;
            
            // Try to open this device
            int result = libusb_open(device, &dev_handle);
            
            if(result != LIBUSB_SUCCESS)
            {
                continue;
            }
            
            // Try to detach the kernel driver if attached
            if(libusb_kernel_driver_active(dev_handle, 0))
            {
                if(libusb_detach_kernel_driver(dev_handle, 0) != LIBUSB_SUCCESS)
                {
                    LOG_WARNING("Could not detach kernel driver");
                }
            }
            
            // Set configuration if needed
            libusb_set_auto_detach_kernel_driver(dev_handle, 1);
            
            // Claim the interface
            result = libusb_claim_interface(dev_handle, 0);
            
            if(result != LIBUSB_SUCCESS)
            {
                libusb_close(dev_handle);
                dev_handle = nullptr;
                continue;
            }
            
            // Successfully claimed the interface
            initialized = true;
            break;
        }
    }
    
    libusb_free_device_list(device_list, 1);
    
    if(!initialized)
    {
        LOG_ERROR("Failed to initialize AMBX device");
        return;
    }
    
    // Turn off all lights initially
    SetAllColors(ToRGBColor(0, 0, 0));
}

AMBXController::~AMBXController()
{
    // Turn off all lights before closing
    if(initialized)
    {
        try
        {
            SetAllColors(ToRGBColor(0, 0, 0));
        }
        catch(...) {}
    }
    
    if(dev_handle != nullptr)
    {
        // Release the interface and close the device
        libusb_release_interface(dev_handle, 0);
        libusb_close(dev_handle);
        dev_handle = nullptr;
    }
    
    if(usb_context != nullptr)
    {
        libusb_exit(usb_context);
        usb_context = nullptr;
    }
}

std::string AMBXController::GetDeviceLocation()
{
    return location;
}

std::string AMBXController::GetSerialString()
{
    return serial;
}

bool AMBXController::IsInitialized()
{
    return initialized;
}

void AMBXController::SendPacket(unsigned char* packet, unsigned int size)
{
    if(!initialized || dev_handle == nullptr)
    {
        LOG_ERROR("Device not initialized for AMBX");
        return;
    }
    
    int actual_length = 0;
    int result = libusb_interrupt_transfer(dev_handle, AMBX_ENDPOINT_OUT, packet, size, &actual_length, 100);
    
    if(result != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to send interrupt transfer: %s", libusb_error_name(result));
    }
}

void AMBXController::SetSingleColor(unsigned int light, unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char color_buf[6];
    
    // Set up message packet
    color_buf[0] = AMBX_PACKET_HEADER;
    color_buf[1] = light;
    color_buf[2] = AMBX_SET_COLOR;
    color_buf[3] = red;
    color_buf[4] = green;
    color_buf[5] = blue;

    // Send packet
    SendPacket(color_buf, 6);
    
    // Add a small delay to ensure commands don't flood the device
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

void AMBXController::SetAllColors(RGBColor color)
{
    unsigned int leds[] = 
    { 
        AMBX_LIGHT_LEFT,
        AMBX_LIGHT_RIGHT,
        AMBX_LIGHT_WALL_LEFT,
        AMBX_LIGHT_WALL_CENTER,
        AMBX_LIGHT_WALL_RIGHT
    };
    
    RGBColor colors[] = { color, color, color, color, color };
    
    SetLEDColors(leds, colors, 5);
}

void AMBXController::SetLEDColor(unsigned int led, RGBColor color)
{
    SetSingleColor(led, RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color));
}

void AMBXController::SetLEDColors(unsigned int* leds, RGBColor* colors, unsigned int count)
{
    for(unsigned int i = 0; i < count; i++)
    {
        SetLEDColor(leds[i], colors[i]);
    }
}
