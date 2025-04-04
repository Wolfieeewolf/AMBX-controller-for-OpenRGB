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
                
                if(result == LIBUSB_ERROR_ACCESS || result == LIBUSB_ERROR_BUSY)
                {
                    LOG_WARNING("AMBX device appears to be in use by another driver (possibly Jungo/WinDriver)");
                    LOG_WARNING("Please use Device Manager to update the driver for this device to WinUSB");
                }
                continue;
            }
            
            // Let libusb handle kernel driver detachment automatically
            libusb_set_auto_detach_kernel_driver(dev_handle, 1);
            
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
            
            // Successfully opened the device (but haven't claimed interface yet)
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
        catch(const std::exception& e)
        {
            LOG_WARNING("Exception while turning off lights: %s", e.what());
        }
        catch(...) 
        {
            LOG_WARNING("Unknown exception while turning off lights");
        }
    }
    
    if(dev_handle != nullptr)
    {
        // Release the interface and close the device
        if(interface_claimed)
        {
            libusb_release_interface(dev_handle, 0);
            interface_claimed = false;
        }
        
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
| Function: ClaimInterface                                   |
|                                                           |
| Description: Claims the USB interface for exclusive access |
|                                                           |
| Parameters: None                                           |
|                                                           |
| Returns: True if interface was claimed successfully,       |
|          False otherwise                                   |
\*---------------------------------------------------------*/
bool AMBXController::ClaimInterface()
{
    if(!initialized || dev_handle == nullptr)
    {
        return false;
    }
    
    // Interface is already claimed
    if(interface_claimed)
    {
        return true;
    }
    
    // Try to claim interface with multiple attempts
    for(int attempt = 0; attempt < 3; attempt++)
    {
        int result = libusb_claim_interface(dev_handle, 0);
        
        if(result == LIBUSB_SUCCESS)
        {
            interface_claimed = true;
            return true;
        }
        else if(result == LIBUSB_ERROR_BUSY) 
        {
            // Interface is likely claimed by another process or driver
            LOG_WARNING("Interface is busy - attempt %d/3", attempt + 1);
        }
        
        // Brief delay before retry
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    LOG_ERROR("Failed to claim interface for AMBX device after multiple attempts");
    LOG_ERROR("This may be due to the original Jungo/WinDriver drivers still being active");
    LOG_ERROR("To fix this issue, please install the WinUSB driver for this device using Zadig");
    return false;
}

/*---------------------------------------------------------*\
| Function: ReleaseInterface                                |
|                                                           |
| Description: Releases the USB interface                    |
|                                                           |
| Parameters: None                                           |
|                                                           |
| Returns: None                                              |
\*---------------------------------------------------------*/
void AMBXController::ReleaseInterface()
{
    if(!initialized || dev_handle == nullptr || !interface_claimed)
    {
        return;
    }
    
    libusb_release_interface(dev_handle, 0);
    interface_claimed = false;
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
    
    // Try to claim the interface
    bool claimed = ClaimInterface();
    
    if(!claimed)
    {
        LOG_ERROR("Failed to claim interface for AMBX before sending packet");
        return;
    }
    
    int actual_length = 0;
    int result = LIBUSB_ERROR_OTHER; // Initialize with error
    
    // Try multiple times in case of transient errors
    for(int attempt = 0; attempt < 3; attempt++)
    {
        result = libusb_interrupt_transfer(dev_handle, AMBX_ENDPOINT_OUT, packet, size, &actual_length, 100);
        
        if(result == LIBUSB_SUCCESS && actual_length == static_cast<int>(size))
        {
            break; // Success, exit the retry loop
        }
        
        // Brief delay before retry with increasing backoff
        std::this_thread::sleep_for(std::chrono::milliseconds(10 * (attempt + 1)));
    }
    
    if(result != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to send interrupt transfer: %s", libusb_error_name(result));
    }
    else if(actual_length != static_cast<int>(size))
    {
        LOG_ERROR("Failed to send complete packet: %d/%d bytes sent", actual_length, size);
    }
    
    // Release the interface after use to allow other software to use the device
    ReleaseInterface();
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
    // Use the more efficient multi-light protocol for better performance
    // Process in batches of 5 lights (max that can fit in one packet)
    const unsigned int BATCH_SIZE = 5;
    
    for(unsigned int i = 0; i < count; i += BATCH_SIZE)
    {
        unsigned int batch_count = ((i + BATCH_SIZE) <= count) ? BATCH_SIZE : (count - i);
        SetMultipleColors(&leds[i], &colors[i], batch_count);
    }
}

/*---------------------------------------------------------*\
| Function: SetMultipleColors                               |
|                                                           |
| Description: Sets multiple lights to different colors     |
|              in a single USB transaction                  |
|                                                           |
| Parameters:                                               |
|   lights  - Array of light IDs                            |
|   colors  - Array of RGB color values                     |
|   count   - Number of lights to set                       |
|                                                           |
| Returns: None                                             |
\*---------------------------------------------------------*/
void AMBXController::SetMultipleColors(unsigned int* lights, RGBColor* colors, unsigned int count)
{
    if(count == 0 || count > 5)
    {
        return;
    }
    
    // For a single light, use the simpler single light method
    if(count == 1)
    {
        SetLEDColor(lights[0], colors[0]);
        return;
    }
    
    // Calculate the size of the packet: 1 byte header + (count * 5 bytes per light)
    unsigned int packet_size = 1 + (count * 5);
    
    // Allocate memory for the packet dynamically
    unsigned char* packet = new unsigned char[packet_size];
    
    // Different multi-command headers seen in traces
    static const unsigned char multi_headers[] = { 0xA4, 0xC4, 0xE4, 0x04, 0x24, 0x44, 0x64, 0x84 };
    static unsigned int header_index = 0;
    
    // Set packet header (rotating through the observed values)
    packet[0] = multi_headers[header_index];
    header_index = (header_index + 1) % (sizeof(multi_headers) / sizeof(multi_headers[0]));
    
    // Build the rest of the packet with each light's data
    for(unsigned int i = 0; i < count; i++)
    {
        unsigned int offset = 1 + (i * 5);
        
        packet[offset + 0] = lights[i];               // Light ID
        packet[offset + 1] = AMBX_SET_COLOR;          // Command (0x03)
        packet[offset + 2] = RGBGetRValue(colors[i]); // Red
        packet[offset + 3] = RGBGetGValue(colors[i]); // Green
        packet[offset + 4] = RGBGetBValue(colors[i]); // Blue
    }
    
    // Send the packet
    SendPacket(packet, packet_size);
    
    // Free the dynamically allocated memory
    delete[] packet;
    
    // Brief delay to ensure the device processes the command
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
