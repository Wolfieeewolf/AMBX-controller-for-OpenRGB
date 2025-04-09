/*---------------------------------------------------------*\
| AMBXControllerDetect.cpp                                  |
|                                                           |
|   Detector for Philips amBX Gaming lights                 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Detector.h"
#include "LogManager.h"
#include "AMBXController.h"
#include "RGBController_AMBX.h"
#include "ResourceManager.h"

#ifdef _WIN32
#include "dependencies/libusb-1.0.27/include/libusb.h"
#else
#include <libusb.h>
#endif

/*-----------------------------------------------------*\
| Philips amBX VID/PID                                  |
\*-----------------------------------------------------*/
#define AMBX_VID                               0x0471
#define AMBX_PID                               0x083F

/******************************************************************************************\
*                                                                                          *
*   DetectAMBXControllers                                                                  *
*                                                                                          *
*       Detect Philips amBX Gaming devices                                                 *
*                                                                                          *
\******************************************************************************************/

void DetectAMBXControllers()
{
    LOG_INFO("Detecting Philips amBX devices...");
    
    /*-------------------------------------*\
    | Initialize libusb                     |
    \*-------------------------------------*/
    libusb_context* context = nullptr;
    
    // Initialize libusb for this detection
    int init_result = libusb_init(&context);
    if(init_result != LIBUSB_SUCCESS)
    {
        LOG_ERROR("Failed to initialize libusb: %s", libusb_error_name(init_result));
        return;
    }
    
    // Get device list
    libusb_device** device_list;
    ssize_t device_count = libusb_get_device_list(context, &device_list);
    
    if(device_count < 0)
    {
        LOG_ERROR("Failed to get USB device list: %s", libusb_error_name(static_cast<int>(device_count)));
        libusb_exit(context);
        return;
    }
    
    int detected_devices = 0;
    
    // Enumerate devices to find AMBX
    for(ssize_t i = 0; i < device_count; i++)
    {
        libusb_device* device = device_list[i];
        struct libusb_device_descriptor descriptor;
        
        if(libusb_get_device_descriptor(device, &descriptor) != LIBUSB_SUCCESS)
        {
            continue;
        }
        
        if(descriptor.idVendor == AMBX_VID && descriptor.idProduct == AMBX_PID)
        {
            // Get device path
            uint8_t bus = libusb_get_bus_number(device);
            uint8_t address = libusb_get_device_address(device);
            
            char device_path[64];
            sprintf(device_path, "%d-%d", bus, address);
            
            LOG_INFO("Found amBX device at bus %d, address %d", bus, address);
            
            // Create controller for this device
            try
            {
                AMBXController* controller = new AMBXController(device_path);
                
                // Only register controller if it initialized successfully
                if(controller->IsInitialized())
                {
                    RGBController_AMBX* rgb_controller = new RGBController_AMBX(controller);
                    ResourceManager::get()->RegisterRGBController(rgb_controller);
                    detected_devices++;
                    
                    LOG_INFO("Successfully added amBX device at %s", device_path);
                }
                else
                {
                    LOG_WARNING("Found amBX device at %s but initialization failed", device_path);
                    delete controller;
                }
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("Exception creating AMBX controller at %s: %s", device_path, e.what());
            }
        }
    }
    
    if(detected_devices == 0)
    {
        // Check if device exists but can't be accessed
        for(ssize_t i = 0; i < device_count; i++)
        {
            libusb_device* device = device_list[i];
            struct libusb_device_descriptor descriptor;
            
            if(libusb_get_device_descriptor(device, &descriptor) != LIBUSB_SUCCESS)
            {
                continue;
            }
            
            if(descriptor.idVendor == AMBX_VID && descriptor.idProduct == AMBX_PID)
            {
                LOG_WARNING("AMBX device found but couldn't be accessed - check permissions");
                LOG_WARNING("On Windows, please install WinUSB driver using Zadig tool");
                LOG_WARNING("On Linux, ensure udev rules are properly installed");
                break;
            }
        }
    }
    
    libusb_free_device_list(device_list, 1);
    libusb_exit(context);
    
    LOG_INFO("AMBX detection completed. Found %d devices.", detected_devices);
}

REGISTER_DETECTOR("Philips amBX", DetectAMBXControllers);
