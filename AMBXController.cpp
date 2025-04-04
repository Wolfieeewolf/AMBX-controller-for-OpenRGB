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
    interface_claimed = false;
    usb_context = nullptr;
    dev_handle = nullptr;
    
    location = "USB amBX: ";
    location += path;
    
    // Initialize libusb in this instance
    int libusb_result = libusb_init(&usb_context);
    if(libusb_result != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to initialize libusb: %s", libusb_error_name(libusb_result));
        return;
    }
    
    // Get the device list
    libusb_device** device_list;
    ssize_t device_count = libusb_get_device_list(usb_context, &device_list);
    
    if(device_count < 0)
    {
        LOG_ERROR("Failed to get USB device list: %s", libusb_error_name(static_cast<int>(device_count)));
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
                LOG_WARNING("Failed to open AMBX device: %s", libusb_error_name(result));
                continue;
            }
            
            // Try to detach the kernel driver if attached
            if(libusb_kernel_driver_active(dev_handle, 0))
            {
                libusb_detach_kernel_driver(dev_handle, 0);
            }
            
            // Set auto-detach for Windows compatibility
            libusb_set_auto_detach_kernel_driver(dev_handle, 1);
            
            // Claim the interface - IMPORTANT: keep it claimed until destruction
            result = libusb_claim_interface(dev_handle, 0);
            
            if(result != LIBUSB_SUCCESS)
            {
                LOG_ERROR("Failed to claim interface: %s", libusb_error_name(result));
                libusb_close(dev_handle);
                dev_handle = nullptr;
                continue;
            }
            
            interface_claimed = true;
            
            // Get string descriptor for serial number if available
            if(desc.iSerialNumber != 0)
            {
                unsigned char serial_str[256];
                int serial_result = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber, 
                                                                       serial_str, sizeof(serial_str));
                if(serial_result > 0)
                {
                    serial = std::string(reinterpret_cast<char*>(serial_str), serial_result);
                }
            }
            
            // Successfully opened and claimed the device
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
        // Release the interface if claimed
        if(interface_claimed)
        {
            libusb_release_interface(dev_handle, 0);
            interface_claimed = false;
        }
        
        // Close the device
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





/*---------------------------------------------------------*\
| Function: SendPacket                                       |
|                                                           |
| Description: Sends a packet to the AMBX device            |
|                                                           |  
| Parameters:                                               |
|   packet - Byte array containing the packet data          |
|   size   - Size of the packet in bytes                    |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
void AMBXController::SendPacket(unsigned char* packet, unsigned int size)
{
    if(!initialized || dev_handle == nullptr)
    {
        LOG_ERROR("Device not initialized for AMBX");
        return;
    }
    
    if(!interface_claimed)
    {
        LOG_ERROR("USB interface not claimed for AMBX");
        return;
    }
    
    int actual_length = 0;
    int result = libusb_interrupt_transfer(dev_handle, AMBX_ENDPOINT_OUT, packet, size, &actual_length, 100);
    
    if(result != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to send interrupt transfer: %s", libusb_error_name(result));
    }
}

/*---------------------------------------------------------*\
| Function: SetSingleColor                                   |
|                                                           |
| Description: Sets a single light to the specified RGB     |
|              color value                                  |
|                                                           |
| Parameters:                                               |
|   light - The ID of the light to set                      |
|   red   - Red component (0-255)                           |
|   green - Green component (0-255)                         |
|   blue  - Blue component (0-255)                          |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
void AMBXController::SetSingleColor(unsigned int light, unsigned char red, unsigned char green, unsigned char blue)
{
    // Validate light ID
    if(light != AMBX_LIGHT_LEFT && 
       light != AMBX_LIGHT_RIGHT && 
       light != AMBX_LIGHT_WALL_LEFT && 
       light != AMBX_LIGHT_WALL_CENTER && 
       light != AMBX_LIGHT_WALL_RIGHT && 
       light != AMBX_LIGHT_ALL)
    {
        LOG_ERROR("Invalid AMBX light ID: 0x%02X", light);
        return;
    }
    
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

/*---------------------------------------------------------*\
| Function: SetAllColors                                     |
|                                                           |
| Description: Sets all lights to the same color             |
|                                                           |
| Parameters:                                               |
|   color - RGB color value to set for all lights           |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
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

/*---------------------------------------------------------*\
| Function: SetLEDColor                                      |
|                                                           |
| Description: Sets a specific LED to a color                |
|                                                           |
| Parameters:                                               |
|   led   - The ID of the LED to set                        |
|   color - RGB color value                                 |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
void AMBXController::SetLEDColor(unsigned int led, RGBColor color)
{
    if(!initialized)
    {
        LOG_ERROR("Cannot set LED color - AMBX device not initialized");
        return;
    }
    
    LOG_DEBUG("Setting LED 0x%02X to RGB: %d,%d,%d", 
             led, 
             RGBGetRValue(color), 
             RGBGetGValue(color), 
             RGBGetBValue(color));
             
    SetSingleColor(led, RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color));
}

/*---------------------------------------------------------*\
| Function: SetLEDColors                                     |
|                                                           |
| Description: Sets multiple LEDs to different colors        |
|                                                           |
| Parameters:                                               |
|   leds   - Array of LED IDs                               |
|   colors - Array of RGB color values                      |
|   count  - Number of LEDs to set                          |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
void AMBXController::SetLEDColors(unsigned int* leds, RGBColor* colors, unsigned int count)
{
    // Simple approach: just send individual commands for each light
    for(unsigned int i = 0; i < count; i++)
    {
        SetLEDColor(leds[i], colors[i]);
        
        // Small delay between commands
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}


