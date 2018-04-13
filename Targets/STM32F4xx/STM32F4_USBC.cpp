// Copyright Microsoft Corporation
// Copyright Oberon microsystems, Inc
// Copyright GHI Electronics, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>
#include "STM32F4.h"

#define __min(a,b)  (((a) < (b)) ? (a) : (b))

#if defined(__GNUC__)
#define PACKED(x) x __attribute__((packed))
#elif defined(arm) || defined(__arm)
#define PACKED(x) __packed x
#endif

///////////////////////////////////////////////////////////////////////////////////////////
/// USB Debugger driver
///////////////////////////////////////////////////////////////////////////////////////////

// USB 2.0 host requests
#define USB_GET_STATUS           0
#define USB_CLEAR_FEATURE        1
#define USB_SET_FEATURE          3
#define USB_SET_ADDRESS          5
#define USB_GET_DESCRIPTOR       6
#define USB_SET_DESCRIPTOR       7
#define USB_GET_CONFIGURATION    8
#define USB_SET_CONFIGURATION    9

// USB 2.0 defined descriptor types
#define USB_DEVICE_DESCRIPTOR_TYPE        1
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 2
#define USB_STRING_DESCRIPTOR_TYPE        3
#define USB_INTERFACE_DESCRIPTOR_TYPE     4
#define USB_ENDPOINT_DESCRIPTOR_TYPE      5

// USB 2.0 host request type defines
#define USB_SETUP_RECIPIENT(n)          ((n) & 0x0F)
#define USB_SETUP_RECIPIENT_DEVICE             0x00
#define USB_SETUP_RECIPIENT_INTERFACE          0x01
#define USB_SETUP_RECIPIENT_ENDPOINT           0x02

// Local device status defines
#define USB_STATUS_DEVICE_SELF_POWERED   0x0001
#define USB_STATUS_DEVICE_REMOTE_WAKEUP  0x0002

#define USB_STATUS_ENDPOINT_HALT         0x0001

#define USB_FEATURE_ENDPOINT_HALT        0x0000
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP 0x0001

// Local device possible states
#define USB_DEVICE_STATE_DETACHED       0
#define USB_DEVICE_STATE_ATTACHED       1
#define USB_DEVICE_STATE_POWERED        2
#define USB_DEVICE_STATE_DEFAULT        3
#define USB_DEVICE_STATE_ADDRESS        4
#define USB_DEVICE_STATE_CONFIGURED     5
#define USB_DEVICE_STATE_SUSPENDED      6
#define USB_DEVICE_STATE_UNINITIALIZED  0xFF

// Possible responses to host requests
#define USB_STATE_DATA                  0
#define USB_STATE_STALL                 1
#define USB_STATE_DONE                  2
#define USB_STATE_ADDRESS               3
#define USB_STATE_STATUS                4
#define USB_STATE_CONFIGURATION         5
#define USB_STATE_REMOTE_WAKEUP         6


// ATTENTION:
// 2.0 is the lowest version that works with WinUSB on Windows 8!!!
// use older values below if you do not care about that

#define DEVICE_RELEASE_VERSION              0x0200

//string descriptor
#define USB_STRING_DESCRIPTOR_SIZE          32

// index for the strings
#define MANUFACTURER_NAME_INDEX             1
#define PRODUCT_NAME_INDEX                  2
#define SERIAL_NUMBER_INDEX                 0

// Configuration for extended descriptor
#define OS_DESCRIPTOR_EX_VERSION            0x0100

#define USB_DISPLAY_STRING_NUM     4
#define USB_FRIENDLY_STRING_NUM    5

#define OS_DESCRIPTOR_STRING_INDEX        0xEE
#define OS_DESCRIPTOR_STRING_VENDOR_CODE  0xA5

// USB 2.0 response structure lengths
#define USB_DEVICE_DESCRIPTOR_LENGTH             18
#define USB_CONFIGURATION_DESCRIPTOR_LENGTH       9
#define USB_STRING_DESCRIPTOR_HEADER_LENGTH       2


// USB Configuration list structures
#define USB_END_DESCRIPTOR_MARKER           0x00
#define USB_DEVICE_DESCRIPTOR_MARKER        0x01
#define USB_CONFIGURATION_DESCRIPTOR_MARKER 0x02
#define USB_STRING_DESCRIPTOR_MARKER        0x03
#define USB_GENERIC_DESCRIPTOR_MARKER       0xFF

// Configuration Descriptor
#define USB_ATTRIBUTE_REMOTE_WAKEUP    0x20
#define USB_ATTRIBUTE_SELF_POWER       0x40
#define USB_ATTRIBUTE_BASE             0x80

// Endpoint Direction
#define USB_ENDPOINT_DIRECTION_IN 0x80
#define USB_ENDPOINT_DIRECTION_OUT 0x00
#define USB_ENDPOINT_NULL 0xFF

// Endpoint Attribute
#define ENDPOINT_INUSED_MASK        0x01
#define ENDPOINT_DIR_IN_MASK        0x02
#define ENDPOINT_DIR_OUT_MASK       0x04

#define USB_ENDPOINT_ATTRIBUTE_BULK 2
#define USB_MAX_DATA_PACKET_SIZE 64

#define USB_MAX_EP_SIZE            64 // maximum control channel packet size
#define USB_MAX_EP0_SIZE            8 // default control channel packet size
#define USB_FS_MAX_EP_COUNT         4 // OTG FS supports 4 endpoints

// This version of the USB code supports only one language - which
// is not specified by USB configuration records - it is defined here.
// This is the String 0 descriptor.This array includes the String descriptor
// header and exactly one language.

#define USB_LANGUAGE_DESCRIPTOR_SIZE 4

// USB 2.0 request packet from host
PACKED(struct) USB_SETUP_PACKET {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

PACKED(struct) USB_DYNAMIC_CONFIGURATION;

struct USB_PACKET64 {
    uint32_t Size;
    uint8_t  Buffer[USB_MAX_DATA_PACKET_SIZE];
};

struct USB_PIPE_MAP {
    uint8_t RxEP;
    uint8_t TxEP;
};

struct USB_CONTROLLER_STATE;

typedef void(*USB_NEXT_CALLBACK)(USB_CONTROLLER_STATE*);

struct USB_CONTROLLER_STATE {
    bool                                                        Initialized;
    uint8_t                                                     CurrentState;
    uint8_t                                                     ControllerNum;
    uint32_t                                                    Event;

    const USB_DYNAMIC_CONFIGURATION*                            Configuration;

    /* Queues & MaxPacketSize must be initialized by the HAL */
    USB_PACKET64                                   	            *Queues[STM32F4_USB_QUEUE_SIZE];
    uint8_t                                                     CurrentPacketOffset[STM32F4_USB_QUEUE_SIZE];
    uint8_t                                                     MaxPacketSize[STM32F4_USB_QUEUE_SIZE];
    bool                                                        IsTxQueue[STM32F4_USB_QUEUE_SIZE];

    /* Arbitrarily as many pipes as endpoints since that is the maximum number of pipes
       necessary to represent the maximum number of endpoints */
    USB_PIPE_MAP                                                pipes[STM32F4_USB_QUEUE_SIZE];

    /* used for transferring packets between upper & lower */
    uint8_t*                                                    Data;
    uint8_t                                                     DataSize;

    /* USB hardware information */
    uint8_t                                                     Address;
    uint8_t                                                     DeviceState;
    uint8_t                                                     PacketSize;
    uint8_t                                                     ConfigurationNum;
    uint32_t                                                    FirstGetDescriptor;

    /* USB status information, used in
       GET_STATUS, SET_FEATURE, CLEAR_FEATURE */
    uint16_t                                                    DeviceStatus;
    uint16_t*                                                   EndpointStatus;
    uint8_t                                                     EndpointCount;
    uint8_t                                                     EndpointStatusChange;

    /* callback function for getting next packet */
    USB_NEXT_CALLBACK                                           DataCallback;

    /* for helping out upper layer during callbacks */
    uint8_t*                                                    ResidualData;
    uint16_t                                                    ResidualCount;
    uint16_t                                                    Expected;
};

PACKED(struct) TinyCLR_UsbClient_DescriptorHeader {
    uint8_t  marker;
    uint8_t  iValue;
    uint16_t size;
};

PACKED(struct) TinyCLR_UsbClient_GenericDescriptorHeader {
    TinyCLR_UsbClient_DescriptorHeader header;

    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
};

PACKED(struct) TinyCLR_UsbClient_DeviceDescriptor {
    TinyCLR_UsbClient_DescriptorHeader header;

    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

PACKED(struct) TinyCLR_UsbClient_InterfaceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
};

PACKED(struct) TinyCLR_UsbClient_EndpointDescriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
};

PACKED(struct) TinyCLR_UsbClient_ClassDescriptorHeader {
    uint8_t bLength;
    uint8_t bDescriptorType;
};

PACKED(struct) TinyCLR_UsbClient_StringDescriptorHeader {
    TinyCLR_UsbClient_DescriptorHeader header;

    uint8_t bLength;
    uint8_t bDescriptorType;
    wchar_t stringDescriptor[32];
};

PACKED(struct) TinyCLR_UsbClient_ConfigurationDescriptor {
    TinyCLR_UsbClient_DescriptorHeader header;

    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;

    TinyCLR_UsbClient_InterfaceDescriptor   itfc0;
    TinyCLR_UsbClient_EndpointDescriptor    epWrite;
    TinyCLR_UsbClient_EndpointDescriptor    epRead;
};

PACKED(struct) TinyCLR_UsbClient_OsStringDescriptor {
    TinyCLR_UsbClient_DescriptorHeader header;

    uint8_t   bLength;
    uint8_t   bDescriptorType;
    wchar_t signature[7];
    uint8_t   bMS_VendorCode;
    uint8_t   padding;
};

PACKED(struct) TinyCLR_UsbClient_XCompatibleOsId {
    TinyCLR_UsbClient_GenericDescriptorHeader header;

    uint32_t dwLength;
    uint16_t bcdVersion;
    uint16_t wIndex;
    uint8_t  bCount;
    uint8_t  padding1[7];
    uint8_t  bFirstInterfaceNumber;
    uint8_t  reserved;
    uint8_t  compatibleID[8];
    uint8_t  subCompatibleID[8];
    uint8_t  padding2[6];
};

PACKED(struct) TinyCLR_UsbClient_XPropertiesOsWinUsb {
    TinyCLR_UsbClient_GenericDescriptorHeader header;

    uint32_t dwLength;
    uint16_t bcdVersion;
    uint16_t wIndex;
    uint16_t  bCount;

    uint32_t dwSize;
    uint32_t dwPropertyDataType;
    uint16_t wPropertyNameLengh;
    uint8_t  bPropertyName[40];
    uint32_t dwPropertyDataLengh;
    uint8_t  bPropertyData[78];
};

PACKED(struct) USB_DYNAMIC_CONFIGURATION {
    TinyCLR_UsbClient_DeviceDescriptor                  *device;
    TinyCLR_UsbClient_ConfigurationDescriptor           *config;
    TinyCLR_UsbClient_StringDescriptorHeader            *manHeader;
    TinyCLR_UsbClient_StringDescriptorHeader            *prodHeader;
    TinyCLR_UsbClient_StringDescriptorHeader            *displayStringHeader;
    TinyCLR_UsbClient_StringDescriptorHeader            *friendlyStringHeader;
    TinyCLR_UsbClient_OsStringDescriptor                *OS_String;
    TinyCLR_UsbClient_XCompatibleOsId                   *OS_XCompatible_ID;
    TinyCLR_UsbClient_XPropertiesOsWinUsb               *OS_XProperty;
    TinyCLR_UsbClient_DescriptorHeader                  *endList;
};

USB_DYNAMIC_CONFIGURATION UsbDefaultConfiguration;

const TinyCLR_UsbClient_DescriptorHeader * USB_FindRecord(USB_CONTROLLER_STATE* State, uint8_t marker, USB_SETUP_PACKET * iValue);
void STM32F4_UsbClient_ClearEndpoints(int32_t endpoint);
bool STM32F4_UsbClient_RxEnable(USB_CONTROLLER_STATE* State, int endpoint);

TinyCLR_UsbClient_DataReceivedHandler STM32F4_UsbClient_DataReceivedHandler;
TinyCLR_UsbClient_OsExtendedPropertyHandler STM32F4_UsbClient_OsExtendedPropertyHandler;

// usb fifo buffer
static int usb_fifo_buffer_in[STM32F4_USB_QUEUE_SIZE];
static int usb_fifo_buffer_out[STM32F4_USB_QUEUE_SIZE];
static int usb_fifo_buffer_count[STM32F4_USB_QUEUE_SIZE];



static const STM32F4_Gpio_Pin g_STM32F4_Usb_Dm_Pins[] = STM32F4_USB_DM_PINS;
static const STM32F4_Gpio_Pin g_STM32F4_Usb_Dp_Pins[] = STM32F4_USB_DP_PINS;
static const STM32F4_Gpio_Pin g_STM32F4_Usb_Vb_Pins[] = STM32F4_USB_VB_PINS;
static const STM32F4_Gpio_Pin g_STM32F4_Usb_Id_Pins[] = STM32F4_USB_ID_PINS;

static const int TOTAL_USB_CONTROLLERS = SIZEOF_ARRAY(g_STM32F4_Usb_Dm_Pins);

int8_t STM32F4_UsbClient_EndpointMap[] = { ENDPOINT_INUSED_MASK,                          // Endpoint 0
                                                ENDPOINT_DIR_IN_MASK | ENDPOINT_DIR_OUT_MASK,  // Endpoint 1
                                                ENDPOINT_DIR_IN_MASK | ENDPOINT_DIR_OUT_MASK,  // Endpoint 2
                                                ENDPOINT_DIR_IN_MASK | ENDPOINT_DIR_OUT_MASK   // Endpoint 3
};

typedef struct {
    USB_CONTROLLER_STATE state;

    uint8_t     ep0Buffer[USB_MAX_EP_SIZE];
    uint16_t    endpointStatus[STM32F4_USB_QUEUE_SIZE];
    uint16_t    endpointType;
    uint8_t     previousDeviceState;

} STM32F4_UsbClient_State;

/* State variables for the controllers */
static STM32F4_UsbClient_State STM32F4_UsbClient_ControllerState[TOTAL_USB_CONTROLLERS];

uint8_t USB_LanguageDescriptor[USB_LANGUAGE_DESCRIPTOR_SIZE] =
{
    USB_LANGUAGE_DESCRIPTOR_SIZE,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09, 0x04                      // U.S. English
};

// Device descriptor
TinyCLR_UsbClient_DeviceDescriptor deviceDescriptor = {

    {
        USB_DEVICE_DESCRIPTOR_MARKER,
        0,
        sizeof(TinyCLR_UsbClient_DeviceDescriptor)
    },
    USB_DEVICE_DESCRIPTOR_LENGTH,       // Length of device descriptor
    USB_DEVICE_DESCRIPTOR_TYPE,         // USB device descriptor type
    0x0200,                             // USB Version 2.00 (BCD) (2.0 required for Extended ID recognition)
    0,                                  // Device class (none)
    0,                                  // Device subclass (none)
    0,                                  // Device protocol (none)
    USB_MAX_EP0_SIZE,                       // Endpoint 0 size
    USB_DEBUGGER_VENDOR_ID,             // Vendor ID
    USB_DEBUGGER_PRODUCT_ID,            // Product ID
    DEVICE_RELEASE_VERSION,             // Product version 1.00 (BCD)
    MANUFACTURER_NAME_INDEX,            // Manufacturer name string index
    PRODUCT_NAME_INDEX,                 // Product name string index
    0,                                  // Serial number string index (none)
    1                                   // Number of configurations
};

// Configuration descriptor
TinyCLR_UsbClient_ConfigurationDescriptor configDescriptor = {
    {
        USB_CONFIGURATION_DESCRIPTOR_MARKER,
        0,
        sizeof(TinyCLR_UsbClient_ConfigurationDescriptor)
    },
    USB_CONFIGURATION_DESCRIPTOR_LENGTH,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    USB_CONFIGURATION_DESCRIPTOR_LENGTH
        + sizeof(TinyCLR_UsbClient_InterfaceDescriptor)
        + sizeof(TinyCLR_UsbClient_EndpointDescriptor)
        + sizeof(TinyCLR_UsbClient_EndpointDescriptor),
    1,                                                  // Number of interfaces
    1,                                                  // Number of this configuration
    0,                                                  // Config descriptor string index (none)
    (USB_ATTRIBUTE_BASE | USB_ATTRIBUTE_SELF_POWER),    // Config attributes
    50,                                                 // Device max current draw

    // Interface
    sizeof(TinyCLR_UsbClient_InterfaceDescriptor),
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0,                                          // Interface number
    0,                                          // Alternate number (main)
    2,                                          // Number of endpoints
    0xFF,                                       // Interface class (vendor)
    1,                                          // Interface subclass
    1,                                          // Interface protocol
    0 ,                                          // Interface descriptor string index (none)

    // Endpoint
    sizeof(TinyCLR_UsbClient_EndpointDescriptor),
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    USB_ENDPOINT_DIRECTION_IN,
    USB_ENDPOINT_ATTRIBUTE_BULK,
    USB_MAX_EP_SIZE,                                // Endpoint 1 packet size
    0 ,                                          // Endpoint 1 interval

    //Endpoint
    sizeof(TinyCLR_UsbClient_EndpointDescriptor),
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    USB_ENDPOINT_DIRECTION_OUT,
    USB_ENDPOINT_ATTRIBUTE_BULK,
    USB_MAX_EP_SIZE,                                // Endpoint 1 packet size
    0                                           // Endpoint 1 interval
};

// Manufacturer name string descriptor header
TinyCLR_UsbClient_StringDescriptorHeader stringManufacturerDescriptorHeader = {
        {
            USB_STRING_DESCRIPTOR_MARKER,
            MANUFACTURER_NAME_INDEX,
            sizeof(TinyCLR_UsbClient_StringDescriptorHeader)
        },
        USB_STRING_DESCRIPTOR_HEADER_LENGTH + (sizeof(wchar_t) * USB_STRING_DESCRIPTOR_SIZE),
        USB_STRING_DESCRIPTOR_TYPE,
        CONCAT(L, DEVICE_MANUFACTURER)
};

// Product name string descriptor header
TinyCLR_UsbClient_StringDescriptorHeader stringProductNameDescriptorHeader = {
    {
        USB_STRING_DESCRIPTOR_MARKER,
        PRODUCT_NAME_INDEX,
        sizeof(TinyCLR_UsbClient_StringDescriptorHeader)
    },
    USB_STRING_DESCRIPTOR_HEADER_LENGTH + (sizeof(wchar_t) * USB_STRING_DESCRIPTOR_SIZE),
    USB_STRING_DESCRIPTOR_TYPE,
    CONCAT(L, DEVICE_NAME)
};

// String 4 descriptor header (display name)
TinyCLR_UsbClient_StringDescriptorHeader stringDisplayNameDescriptorHeader = {
    {
        USB_STRING_DESCRIPTOR_MARKER,
        USB_DISPLAY_STRING_NUM,
        sizeof(TinyCLR_UsbClient_StringDescriptorHeader)
    },
    USB_STRING_DESCRIPTOR_HEADER_LENGTH + (sizeof(wchar_t) * USB_STRING_DESCRIPTOR_SIZE),
    USB_STRING_DESCRIPTOR_TYPE,
    CONCAT(L, DEVICE_NAME)
};

// String 5 descriptor header (friendly name)
TinyCLR_UsbClient_StringDescriptorHeader stringFriendlyNameDescriptorHeader = {
    {
        USB_STRING_DESCRIPTOR_MARKER,
        USB_FRIENDLY_STRING_NUM,
        sizeof(TinyCLR_UsbClient_StringDescriptorHeader)
    },
    USB_STRING_DESCRIPTOR_HEADER_LENGTH + (sizeof(wchar_t) * USB_STRING_DESCRIPTOR_SIZE),
    USB_STRING_DESCRIPTOR_TYPE,
    CONCAT(L, DEVICE_NAME)
};

// OS Descriptor string for Extended OS Compat ID
TinyCLR_UsbClient_OsStringDescriptor STM32F4_UsbClient_OsStringDescriptor;

// OS Extended Compatible ID for WinUSB
TinyCLR_UsbClient_XCompatibleOsId STM32F4_UsbClient_XCompatibleOsId;

// OS Extended Property
TinyCLR_UsbClient_XPropertiesOsWinUsb STM32F4_UsbClient_XPropertiesOsWinUsb;

// End of configuration marker
const TinyCLR_UsbClient_DescriptorHeader usbDescriptorHeader = {
    USB_END_DESCRIPTOR_MARKER,
    0,
    0
};

void STM32F4_UsbClient_SetEvent(USB_CONTROLLER_STATE *State, uint32_t event) {
    DISABLE_INTERRUPTS_SCOPED(irq);

    uint32_t old_event = State->Event;

    State->Event |= event;

    if (old_event != State->Event) {
        STM32F4_UsbClient_DataReceivedHandler(nullptr);
    }
}

void STM32F4_UsbClient_ClearEvent(USB_CONTROLLER_STATE *State, uint32_t event) {
    DISABLE_INTERRUPTS_SCOPED(irq);

    State->Event &= ~event;
}

void STM32F4_UsbClient_ClearQueues(USB_CONTROLLER_STATE *State, bool ClrRxQueue, bool ClrTxQueue) {
    DISABLE_INTERRUPTS_SCOPED(irq);

    if (ClrRxQueue) {
        for (int endpoint = 0; endpoint < STM32F4_USB_QUEUE_SIZE; endpoint++) {
            if (State->Queues[endpoint] == nullptr || State->IsTxQueue[endpoint])
                continue;

            STM32F4_UsbClient_ClearEndpoints(endpoint);

            /* since this queue is now reset, we have room available for newly arrived packets */
            STM32F4_UsbClient_RxEnable(State, endpoint);
        }
    }

    if (ClrTxQueue) {
        for (int endpoint = 0; endpoint < STM32F4_USB_QUEUE_SIZE; endpoint++) {
            if (State->Queues[endpoint] && State->IsTxQueue[endpoint])
                STM32F4_UsbClient_ClearEndpoints(endpoint);
        }
    }
}

void STM32F4_UsbClient_StateCallback(USB_CONTROLLER_STATE* State) {
    if (State->CurrentState != State->DeviceState) {
        /* whenever we leave the configured state, re-initialize all of the queues */
        //Not necessary, as TxBuffer may hold any data and then send them out when it is configured again.
        // The RxQueue is clear when it is configured.
        if (USB_DEVICE_STATE_CONFIGURED == State->CurrentState) {
            STM32F4_UsbClient_ClearQueues(State, true, true);
        }

        State->CurrentState = State->DeviceState;

        switch (State->DeviceState) {
        case USB_DEVICE_STATE_DETACHED:
            State->ResidualCount = 0;
            State->DataCallback = nullptr;

            break;
        case USB_DEVICE_STATE_CONFIGURED:
            /* whenever we enter the configured state, re-initialize all of the RxQueues */
            /* Txqueue has stored some data to be transmitted */
            STM32F4_UsbClient_ClearQueues(State, true, false);
            break;
        }
    }
}

void STM32F4_UsbClient_DataCallback(USB_CONTROLLER_STATE* State) {
    uint32_t length = __min(State->PacketSize, State->ResidualCount);

    memcpy(State->Data, State->ResidualData, length);

    State->DataSize = length;
    State->ResidualData += length;
    State->ResidualCount -= length;

    if (length == State->PacketSize) {
        State->Expected -= length;
    }
    else {
        State->Expected = 0;
    }

    if (State->Expected) {
        State->DataCallback = STM32F4_UsbClient_DataCallback;
    }
    else {
        State->DataCallback = nullptr;
    }
}

uint8_t STM32F4_UsbClient_HandleGetStatus(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    uint16_t* status;
    uint16_t  zero = 0;

    /* validate setup packet */
    if (Setup->wValue != 0 || Setup->wLength != 2) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState == USB_DEVICE_STATE_DEFAULT) {
        return USB_STATE_STALL;
    }

    switch (USB_SETUP_RECIPIENT(Setup->bmRequestType)) {
    case USB_SETUP_RECIPIENT_DEVICE:
        status = &State->DeviceStatus;
        break;

    case USB_SETUP_RECIPIENT_INTERFACE:
        if (State->DeviceState != USB_DEVICE_STATE_CONFIGURED) {
            return USB_STATE_STALL;
        }

        status = &zero;
        break;

    case USB_SETUP_RECIPIENT_ENDPOINT:
        if (State->DeviceState == USB_DEVICE_STATE_ADDRESS && Setup->wIndex != 0) {
            return USB_STATE_STALL;
        }

        /* bit 0x80 designates direction, which we don't utilize in this calculation */
        Setup->wIndex &= 0x7F;

        if (Setup->wIndex >= State->EndpointCount) {
            return USB_STATE_STALL;
        }

        status = &State->EndpointStatus[Setup->wIndex];
        break;

    default:
        return USB_STATE_STALL;
    }

    /* send requested status to host */
    State->ResidualData = (uint8_t*)status;
    State->ResidualCount = 2;
    State->DataCallback = STM32F4_UsbClient_DataCallback;

    return USB_STATE_DATA;
}

uint8_t STM32F4_UsbClient_HandleClearFeature(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    TinyCLR_UsbClient_ConfigurationDescriptor * Config;
    uint8_t       retState;

    /* validate setup packet */
    if (Setup->wLength != 0) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState != USB_DEVICE_STATE_CONFIGURED) {
        return USB_STATE_STALL;
    }

    switch (USB_SETUP_RECIPIENT(Setup->bmRequestType)) {
    case USB_SETUP_RECIPIENT_DEVICE:
        // only support Remote wakeup
        if (Setup->wValue != USB_FEATURE_DEVICE_REMOTE_WAKEUP)
            return USB_STATE_STALL;

        // Locate the configuration descriptor
        Config = (TinyCLR_UsbClient_ConfigurationDescriptor *)USB_FindRecord(State, USB_CONFIGURATION_DESCRIPTOR_MARKER, Setup);

        if (Config && (Config->bmAttributes & USB_ATTRIBUTE_REMOTE_WAKEUP)) {
            State->DeviceStatus &= ~USB_STATUS_DEVICE_REMOTE_WAKEUP;
            retState = USB_STATE_REMOTE_WAKEUP;
        }
        else {
            return USB_STATE_STALL;
        }
        break;

    case USB_SETUP_RECIPIENT_INTERFACE:
        /* there are no interface features to clear */
        return USB_STATE_STALL;

    case USB_SETUP_RECIPIENT_ENDPOINT:
        if (State->DeviceState == USB_DEVICE_STATE_ADDRESS && Setup->wIndex != 0)
            return USB_STATE_STALL;

        /* bit 0x80 designates direction, which we dont utilize in this calculation */
        Setup->wIndex &= 0x7F;

        if (Setup->wIndex == 0 || Setup->wIndex >= State->EndpointCount)
            return USB_STATE_STALL;

        if (Setup->wValue != USB_FEATURE_ENDPOINT_HALT)
            return USB_STATE_STALL;

        /* clear the halt feature */
        State->EndpointStatus[Setup->wIndex] &= ~USB_STATUS_ENDPOINT_HALT;
        State->EndpointStatusChange = Setup->wIndex;
        retState = USB_STATE_STATUS;
        break;

    default:
        return USB_STATE_STALL;
    }

    /* send zero-length packet to tell host we're done */
    State->ResidualCount = 0;
    State->DataCallback = STM32F4_UsbClient_DataCallback;

    /* notify lower layer of status change */
    return retState;
}

uint8_t STM32F4_UsbClient_HandleSetFeature(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    TinyCLR_UsbClient_ConfigurationDescriptor * Config;
    uint8_t       retState;

    /* validate setup packet */
    if (Setup->wLength != 0) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState == USB_DEVICE_STATE_DEFAULT) {
        return USB_STATE_STALL;
    }

    switch (USB_SETUP_RECIPIENT(Setup->bmRequestType)) {
    case USB_SETUP_RECIPIENT_DEVICE:
        // only support Remote wakeup
        if (Setup->wValue != USB_FEATURE_DEVICE_REMOTE_WAKEUP) {
            return USB_STATE_STALL;
        }

        Config = (TinyCLR_UsbClient_ConfigurationDescriptor *)USB_FindRecord(State, USB_CONFIGURATION_DESCRIPTOR_MARKER, Setup);
        if (Config == nullptr)        // If the configuration record could not be found
            return USB_STATE_STALL; // Something pretty serious is wrong

        if (Config->bmAttributes & USB_ATTRIBUTE_REMOTE_WAKEUP) {
            State->DeviceStatus |= USB_STATUS_DEVICE_REMOTE_WAKEUP;
        }

        retState = USB_STATE_REMOTE_WAKEUP;
        break;

    case USB_SETUP_RECIPIENT_INTERFACE:
        /* there are no interface features to set */
        return USB_STATE_STALL;

    case USB_SETUP_RECIPIENT_ENDPOINT:
        if (State->DeviceState == USB_DEVICE_STATE_ADDRESS && Setup->wIndex != 0) {
            return USB_STATE_STALL;
        }

        /* bit 0x80 designates direction, which we don't utilize in this calculation */
        Setup->wIndex &= 0x7F;

        if (Setup->wIndex == 0 || Setup->wIndex >= State->EndpointCount) {
            return USB_STATE_STALL;
        }

        if (Setup->wValue != USB_FEATURE_ENDPOINT_HALT) {
            return USB_STATE_STALL;
        }

        /* set the halt feature */
        State->EndpointStatus[Setup->wIndex] |= USB_STATUS_ENDPOINT_HALT;
        State->EndpointStatusChange = Setup->wIndex;
        retState = USB_STATE_STATUS;
        break;

    default:
        return USB_STATE_STALL;
    }

    /* send zero-length packet to tell host we're done */
    State->ResidualCount = 0;
    State->DataCallback = STM32F4_UsbClient_DataCallback;

    /* notify lower layer of status change */
    return retState;
}

uint8_t STM32F4_UsbClient_HandleSetAddress(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    /* validate setup packet */
    if (Setup->wValue > 127 || Setup->wIndex != 0 || Setup->wLength != 0) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState >= USB_DEVICE_STATE_CONFIGURED) {
        return USB_STATE_STALL;
    }

    /* set address */
    State->Address = Setup->wValue;

    /* catch state changes */
    if (State->Address == 0) {
        State->DeviceState = USB_DEVICE_STATE_DEFAULT;
    }
    else {
        State->DeviceState = USB_DEVICE_STATE_ADDRESS;
    }

    STM32F4_UsbClient_StateCallback(State);

    /* send zero-length packet to tell host we're done */
    State->ResidualCount = 0;
    State->DataCallback = STM32F4_UsbClient_DataCallback;

    /* notify hardware of address change */
    return USB_STATE_ADDRESS;
}

uint8_t STM32F4_UsbClient_HandleConfigurationRequests(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    const TinyCLR_UsbClient_DescriptorHeader * header;
    uint8_t       type;
    uint8_t       DescriptorIndex;

    /* this request is valid regardless of device state */
    type = ((Setup->wValue & 0xFF00) >> 8);
    DescriptorIndex = (Setup->wValue & 0x00FF);
    State->Expected = Setup->wLength;

    if (State->Expected == 0) {
        // just return an empty Status packet
        State->ResidualCount = 0;
        State->DataCallback = STM32F4_UsbClient_DataCallback;
        return USB_STATE_DATA;
    }

    // The very first GET_DESCRIPTOR command out of reset should always return at most PacketSize bytes.
    // After that, you can return as many as the host has asked.
    if (State->DeviceState <= USB_DEVICE_STATE_DEFAULT) {
        if (State->FirstGetDescriptor) {
            State->FirstGetDescriptor = false;

            State->Expected = __min(State->Expected, State->PacketSize);
        }
    }

    State->ResidualData = nullptr;
    State->ResidualCount = 0;

    if (Setup->bRequest == USB_GET_DESCRIPTOR) {
        switch (type) {
        case USB_DEVICE_DESCRIPTOR_TYPE:
            header = USB_FindRecord(State, USB_DEVICE_DESCRIPTOR_MARKER, Setup);
            if (header) {
                const TinyCLR_UsbClient_DeviceDescriptor * device = (TinyCLR_UsbClient_DeviceDescriptor *)header;
                State->ResidualData = (uint8_t *)&device->bLength;      // Start of the device descriptor
                State->ResidualCount = __min(State->Expected, device->bLength);
            }
            break;

        case USB_CONFIGURATION_DESCRIPTOR_TYPE:
            header = USB_FindRecord(State, USB_CONFIGURATION_DESCRIPTOR_MARKER, Setup);
            if (header) {
                const TinyCLR_UsbClient_ConfigurationDescriptor * Config = (TinyCLR_UsbClient_ConfigurationDescriptor *)header;
                State->ResidualData = (uint8_t *)&Config->bLength;
                State->ResidualCount = __min(State->Expected, Config->wTotalLength);
            }
            break;

        case USB_STRING_DESCRIPTOR_TYPE:
            if (DescriptorIndex == 0)        // If host is requesting the language list
            {
                State->ResidualData = USB_LanguageDescriptor;
                State->ResidualCount = __min(State->Expected, USB_LANGUAGE_DESCRIPTOR_SIZE);
            }
            else if (nullptr != (header = USB_FindRecord(State, USB_STRING_DESCRIPTOR_MARKER, Setup))) {
                const TinyCLR_UsbClient_StringDescriptorHeader * string = (TinyCLR_UsbClient_StringDescriptorHeader *)header;
                State->ResidualData = (uint8_t *)&string->bLength;
                State->ResidualCount = __min(State->Expected, string->bLength);
            }
            break;

        default:
            break;
        }
    }

    // If the request was not recognized, the generic types should be searched
    if (State->ResidualData == nullptr) {
        if (nullptr != (header = USB_FindRecord(State, USB_GENERIC_DESCRIPTOR_MARKER, Setup))) {
            State->ResidualData = (uint8_t *)header;
            State->ResidualData += sizeof(TinyCLR_UsbClient_GenericDescriptorHeader);       // Data is located right after the header
            State->ResidualCount = __min(State->Expected, header->size - sizeof(TinyCLR_UsbClient_GenericDescriptorHeader));
        }
        else
            return USB_STATE_STALL;
    }

    State->DataCallback = STM32F4_UsbClient_DataCallback;

    return USB_STATE_DATA;
}

uint8_t STM32F4_UsbClient_HandleGetConfiguration(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup) {
    /* validate setup packet */
    if (Setup->wValue != 0 || Setup->wIndex != 0 || Setup->wLength != 1) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState == USB_DEVICE_STATE_DEFAULT) {
        return USB_STATE_STALL;
    }

    State->ResidualData = &State->ConfigurationNum;
    State->ResidualCount = 1;
    State->Expected = 1;
    State->DataCallback = STM32F4_UsbClient_DataCallback;

    return USB_STATE_DATA;
}

uint8_t STM32F4_UsbClient_HandleSetConfiguration(USB_CONTROLLER_STATE* State, USB_SETUP_PACKET* Setup, bool DataPhase) {
    /* validate setup packet */
    if (Setup->wIndex != 0 || Setup->wLength != 0) {
        return USB_STATE_STALL;
    }

    /* validate based on device state */
    if (State->DeviceState == USB_DEVICE_STATE_DEFAULT) {
        return USB_STATE_STALL;
    }

    /* we only support one configuration */
    if (Setup->wValue > 1) {
        return USB_STATE_STALL;
    }

    State->ConfigurationNum = Setup->wValue;

    /* catch state changes */
    if (State->ConfigurationNum == 0) {
        State->DeviceState = USB_DEVICE_STATE_ADDRESS;
    }
    else {
        State->DeviceState = USB_DEVICE_STATE_CONFIGURED;
    }

    STM32F4_UsbClient_StateCallback(State);

    if (DataPhase) {
        /* send zero-length packet to tell host we're done */
        State->ResidualCount = 0;
        State->DataCallback = STM32F4_UsbClient_DataCallback;
    }

    return USB_STATE_CONFIGURATION;
}

// Searches through the USB Configuration records for the requested type
// Returns a pointer to the header information if found and nullptr if not
const TinyCLR_UsbClient_DescriptorHeader * USB_FindRecord(USB_CONTROLLER_STATE* State, uint8_t marker, USB_SETUP_PACKET * setup) {
    bool Done = false;

    const TinyCLR_UsbClient_DescriptorHeader * header = (const TinyCLR_UsbClient_DescriptorHeader *)State->Configuration;
    TinyCLR_UsbClient_DescriptorHeader* ptr = (TinyCLR_UsbClient_DescriptorHeader*)(*(uint32_t*)header);
    TinyCLR_UsbClient_ConfigurationDescriptor *config;

    // If there is no configuration for this controller
    if (nullptr == header)
        return header;

    while (!Done) {
        const uint8_t *next = (const uint8_t *)header;
        ptr = (TinyCLR_UsbClient_DescriptorHeader*)(*(uint32_t*)header);
        next += 4;      // Calculate address of next record

        const TinyCLR_UsbClient_GenericDescriptorHeader *generic = (TinyCLR_UsbClient_GenericDescriptorHeader *)ptr;

        switch (ptr->marker) {
        case USB_DEVICE_DESCRIPTOR_MARKER:
            if (ptr->marker == marker)
                Done = true;
            break;

        case USB_CONFIGURATION_DESCRIPTOR_MARKER:
            config = (TinyCLR_UsbClient_ConfigurationDescriptor *)UsbDefaultConfiguration.config;
            if (config->header.marker == marker)
                Done = true;
            break;

        case USB_STRING_DESCRIPTOR_MARKER:
            // If String descriptor then the index is significant
            if ((ptr->marker == marker) && (ptr->iValue == (setup->wValue & 0x00FF)))
                Done = true;
            break;
        case USB_GENERIC_DESCRIPTOR_MARKER:
            if (generic->bmRequestType == setup->bmRequestType &&
                generic->bRequest == setup->bRequest &&
                generic->wValue == setup->wValue &&
                generic->wIndex == setup->wIndex) {
                Done = true;
            }
            break;
        case USB_END_DESCRIPTOR_MARKER:
            Done = true;
            header = nullptr;
            ptr = nullptr;
            break;
        }
        if (!Done)
            header = (const TinyCLR_UsbClient_DescriptorHeader *)next;    // Try next record
    }

    return ptr;
}

uint8_t STM32F4_UsbClient_ControlCallback(USB_CONTROLLER_STATE* State) {
    USB_SETUP_PACKET* Setup;

    if (State->DataSize == 0) {
        return USB_STATE_DONE;
    }

    Setup = (USB_SETUP_PACKET*)State->Data;

    switch (Setup->bRequest) {
    case USB_GET_STATUS:
        return STM32F4_UsbClient_HandleGetStatus(State, Setup);
    case USB_CLEAR_FEATURE:
        return STM32F4_UsbClient_HandleClearFeature(State, Setup);
    case USB_SET_FEATURE:
        return STM32F4_UsbClient_HandleSetFeature(State, Setup);
    case USB_SET_ADDRESS:
        return STM32F4_UsbClient_HandleSetAddress(State, Setup);
    case USB_GET_CONFIGURATION:
        return STM32F4_UsbClient_HandleGetConfiguration(State, Setup);
    case USB_SET_CONFIGURATION:
        return STM32F4_UsbClient_HandleSetConfiguration(State, Setup, true);
    default:
        return STM32F4_UsbClient_HandleConfigurationRequests(State, Setup);
    }

    return USB_STATE_STALL;
}

USB_PACKET64* STM32F4_UsbClient_RxEnqueue(USB_CONTROLLER_STATE* State, int endpoint, bool& disableRx) {
    USB_PACKET64* packet;

    if (usb_fifo_buffer_count[endpoint] == STM32F4_USB_FIFO_BUFFER_SIZE) {
        disableRx = true;

        return nullptr;
    }

    disableRx = false;

    packet = &State->Queues[endpoint][usb_fifo_buffer_in[endpoint]];

    usb_fifo_buffer_in[endpoint]++;
    usb_fifo_buffer_count[endpoint]++;

    if (usb_fifo_buffer_in[endpoint] == STM32F4_USB_FIFO_BUFFER_SIZE)
        usb_fifo_buffer_in[endpoint] = 0;

    STM32F4_UsbClient_SetEvent(State, 1 << endpoint);

    return packet;
}

USB_PACKET64* STM32F4_UsbClient_TxDequeue(USB_CONTROLLER_STATE* State, int endpoint) {
    USB_PACKET64* packet;

    if (usb_fifo_buffer_count[endpoint] == 0) {
        return nullptr;
    }

    packet = &State->Queues[endpoint][usb_fifo_buffer_out[endpoint]];

    usb_fifo_buffer_count[endpoint]--;
    usb_fifo_buffer_out[endpoint]++;

    if (usb_fifo_buffer_out[endpoint] == STM32F4_USB_FIFO_BUFFER_SIZE)
        usb_fifo_buffer_out[endpoint] = 0;

    return packet;
}

void STM32F4_UsbClient_ClearEndpoints(int32_t endpoint) {
    usb_fifo_buffer_in[endpoint] = usb_fifo_buffer_out[endpoint] = usb_fifo_buffer_count[endpoint] = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// TinyCLR USBClient API
///////////////////////////////////////////////////////////////////////////////////////////
bool STM32F4_UsbClient_Initialize(int controller);
bool STM32F4_UsbClient_Uninitialize(int controller);
bool STM32F4_UsbClient_StartOutput(USB_CONTROLLER_STATE* State, int endpoint);

static TinyCLR_UsbClient_Provider usbClientProvider;
static TinyCLR_Api_Info usbClientApi;

const TinyCLR_Api_Info* STM32F4_UsbClient_GetApi() {
    usbClientProvider.Parent = &usbClientApi;
    usbClientProvider.Index = 0;
    usbClientProvider.Acquire = &STM32F4_UsbClient_Acquire;
    usbClientProvider.Release = &STM32F4_UsbClient_Release;
    usbClientProvider.Open = &STM32F4_UsbClient_Open;
    usbClientProvider.Close = &STM32F4_UsbClient_Close;
    usbClientProvider.Write = &STM32F4_UsbClient_Write;
    usbClientProvider.Read = &STM32F4_UsbClient_Read;
    usbClientProvider.Flush = &STM32F4_UsbClient_Flush;
    usbClientProvider.SetDeviceDescriptor = &STM32F4_UsbClient_SetDeviceDescriptor;
    usbClientProvider.SetConfigDescriptor = &STM32F4_UsbClient_SetConfigDescriptor;
    usbClientProvider.SetStringDescriptor = &STM32F4_UsbClient_SetStringDescriptor;
    usbClientProvider.SetDataReceivedHandler = &STM32F4_UsbClient_SetDataReceivedHandler;
    usbClientProvider.SetOsExtendedPropertyHandler = &STM32F4_UsbClient_SetOsExtendedPropertyHandler;

    usbClientApi.Author = "GHI Electronics, LLC";
    usbClientApi.Name = "GHIElectronics.TinyCLR.NativeApis.STM32F4.UsbClientProvider";
    usbClientApi.Type = TinyCLR_Api_Type::UsbClientProvider;
    usbClientApi.Version = 0;
    usbClientApi.Count = 1;
    usbClientApi.Implementation = &usbClientProvider;

    return &usbClientApi;
}

TinyCLR_Result STM32F4_UsbClient_Acquire(const TinyCLR_UsbClient_Provider* self) {
    int32_t controller = self->Index;

    uint8_t *osStringDescriptor = (uint8_t*)&STM32F4_UsbClient_OsStringDescriptor;
    uint8_t *xCompatibleOsId = (uint8_t*)&STM32F4_UsbClient_XCompatibleOsId;
    uint8_t *xPropertiesOsWinUsb = (uint8_t*)&STM32F4_UsbClient_XPropertiesOsWinUsb;

    STM32F4_UsbClient_OsExtendedPropertyHandler(self, osStringDescriptor, xCompatibleOsId, xPropertiesOsWinUsb);

    USB_CONTROLLER_STATE *State = &STM32F4_UsbClient_ControllerState[controller].state;

    DISABLE_INTERRUPTS_SCOPED(irq);

    // Init UsbDefaultConfiguration
    memset(&UsbDefaultConfiguration, 0, sizeof(USB_DYNAMIC_CONFIGURATION));

    configDescriptor.epWrite.bEndpointAddress = USB_ENDPOINT_DIRECTION_IN;
    configDescriptor.epRead.bEndpointAddress = USB_ENDPOINT_DIRECTION_OUT;

    UsbDefaultConfiguration.device = (TinyCLR_UsbClient_DeviceDescriptor*)&deviceDescriptor;
    UsbDefaultConfiguration.config = (TinyCLR_UsbClient_ConfigurationDescriptor*)&configDescriptor;

    UsbDefaultConfiguration.manHeader = (TinyCLR_UsbClient_StringDescriptorHeader*)&stringManufacturerDescriptorHeader;
    UsbDefaultConfiguration.prodHeader = (TinyCLR_UsbClient_StringDescriptorHeader*)&stringProductNameDescriptorHeader;
    UsbDefaultConfiguration.displayStringHeader = (TinyCLR_UsbClient_StringDescriptorHeader*)&stringDisplayNameDescriptorHeader;
    UsbDefaultConfiguration.friendlyStringHeader = (TinyCLR_UsbClient_StringDescriptorHeader*)&stringFriendlyNameDescriptorHeader;

    UsbDefaultConfiguration.OS_String = (TinyCLR_UsbClient_OsStringDescriptor*)&STM32F4_UsbClient_OsStringDescriptor;
    UsbDefaultConfiguration.OS_XCompatible_ID = (TinyCLR_UsbClient_XCompatibleOsId*)&STM32F4_UsbClient_XCompatibleOsId;
    UsbDefaultConfiguration.OS_XProperty = (TinyCLR_UsbClient_XPropertiesOsWinUsb*)&STM32F4_UsbClient_XPropertiesOsWinUsb;

    UsbDefaultConfiguration.endList = (TinyCLR_UsbClient_DescriptorHeader*)&usbDescriptorHeader;

    // Init Usb State
    memset(State, 0, sizeof(USB_CONTROLLER_STATE));

    State->ControllerNum = controller;
    State->Configuration = &UsbDefaultConfiguration;
    State->CurrentState = USB_DEVICE_STATE_UNINITIALIZED;
    State->DeviceStatus = USB_STATUS_DEVICE_SELF_POWERED;
    State->EndpointCount = USB_FS_MAX_EP_COUNT;
    State->PacketSize = USB_MAX_EP0_SIZE;
    State->Initialized = true;

    for (auto i = 0; i < STM32F4_USB_QUEUE_SIZE; i++) {
        State->pipes[i].RxEP = USB_ENDPOINT_NULL;
        State->pipes[i].TxEP = USB_ENDPOINT_NULL;
        State->MaxPacketSize[i] = USB_MAX_EP_SIZE;
    }

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_Release(const TinyCLR_UsbClient_Provider* self) {
    int32_t controller = self->Index;

    USB_CONTROLLER_STATE *State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (State->Initialized) {
        DISABLE_INTERRUPTS_SCOPED(irq);

        STM32F4_UsbClient_Uninitialize(controller);

        State->Initialized = false;

        // for soft reboot allow the USB to be off for at least 100ms
        STM32F4_Time_Delay(nullptr, 100000); // 100ms
    }

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_Open(const TinyCLR_UsbClient_Provider* self, int32_t& pipe, TinyCLR_UsbClient_PipeMode mode) {
    int32_t controller = self->Index;

    USB_CONTROLLER_STATE * State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (!State->Initialized)     // If no such controller exists (or it is not initialized)
        return TinyCLR_Result::NotAvailable;

    int32_t writeEp = USB_ENDPOINT_NULL;
    int32_t readEp = USB_ENDPOINT_NULL;

    if (mode != TinyCLR_UsbClient_PipeMode::InOut)
        return TinyCLR_Result::NotSupported;

    for (auto i = 0; i < SIZEOF_ARRAY(STM32F4_UsbClient_EndpointMap); i++) {
        if ((STM32F4_UsbClient_EndpointMap[i] & ENDPOINT_INUSED_MASK)) // in used
            continue;

        if (writeEp == USB_ENDPOINT_NULL && ((STM32F4_UsbClient_EndpointMap[i] & ENDPOINT_DIR_IN_MASK) == ENDPOINT_DIR_IN_MASK)) {
            writeEp = i;
            STM32F4_UsbClient_EndpointMap[i] |= ENDPOINT_INUSED_MASK;

            continue;
        }

        if (readEp == USB_ENDPOINT_NULL && ((STM32F4_UsbClient_EndpointMap[i] & ENDPOINT_DIR_OUT_MASK) == ENDPOINT_DIR_OUT_MASK)) {
            readEp = i;
            STM32F4_UsbClient_EndpointMap[i] |= ENDPOINT_INUSED_MASK;

            continue;
        }

        if (writeEp != 0 && readEp != 0) {
            break;
        }
    }
    // Check the usbPipe and the two endpoint numbers for validity (both endpoints cannot be zero)
    if ((readEp == USB_ENDPOINT_NULL && writeEp == USB_ENDPOINT_NULL)
        || (readEp != USB_ENDPOINT_NULL && (readEp < 1 || readEp >= STM32F4_USB_QUEUE_SIZE))
        || (writeEp != USB_ENDPOINT_NULL && (writeEp < 1 || writeEp >= STM32F4_USB_QUEUE_SIZE)))
        return TinyCLR_Result::NotAvailable;

    // The specified endpoints must not be in use by another pipe
    for (int i = 0; i < STM32F4_USB_QUEUE_SIZE; i++) {
        if (readEp != USB_ENDPOINT_NULL && (State->pipes[i].RxEP == readEp || State->pipes[i].TxEP == readEp))
            return TinyCLR_Result::NotAvailable;
        if (writeEp != USB_ENDPOINT_NULL && (State->pipes[i].RxEP == writeEp || State->pipes[i].TxEP == writeEp))
            return TinyCLR_Result::NotAvailable;
    }

    for (pipe = 0; pipe < STM32F4_USB_QUEUE_SIZE; pipe++) {
        // The Pipe must be currently closed
        if (State->pipes[pipe].RxEP == USB_ENDPOINT_NULL && State->pipes[pipe].TxEP == USB_ENDPOINT_NULL)
            break;
    }

    if (pipe == STM32F4_USB_QUEUE_SIZE)
        return TinyCLR_Result::NotAvailable;

    // All tests pass, assign the endpoints to the pipe
    State->pipes[pipe].RxEP = readEp;
    State->pipes[pipe].TxEP = writeEp;

    TinyCLR_UsbClient_ConfigurationDescriptor *config = (TinyCLR_UsbClient_ConfigurationDescriptor *)UsbDefaultConfiguration.config;
    TinyCLR_UsbClient_EndpointDescriptor  *ep = (TinyCLR_UsbClient_EndpointDescriptor  *)(((uint8_t *)config) + USB_CONFIGURATION_DESCRIPTOR_LENGTH + sizeof(TinyCLR_UsbClient_DescriptorHeader) + sizeof(TinyCLR_UsbClient_InterfaceDescriptor));

    uint8_t * end = ((uint8_t *)config) + config->header.size;
    uint32_t epType = ((STM32F4_UsbClient_State*)State)->endpointType;

    while (((uint8_t *)ep) != nullptr && ((uint8_t *)ep) < end) {
        if (USB_ENDPOINT_DESCRIPTOR_TYPE != ep->bDescriptorType || sizeof(TinyCLR_UsbClient_EndpointDescriptor) != ep->bLength)
            break;

        auto idx = 0;

        if (ep->bEndpointAddress == USB_ENDPOINT_DIRECTION_IN) {
            ep->bEndpointAddress |= writeEp;
            idx = writeEp;
            State->IsTxQueue[idx] = true;
        }

        else if (ep->bEndpointAddress == USB_ENDPOINT_DIRECTION_OUT) {
            ep->bEndpointAddress |= readEp;
            idx = readEp;
            State->IsTxQueue[idx] = false;
        }

        if (idx > 0) {
            if (apiProvider != nullptr) {
                auto memoryProvider = (const TinyCLR_Memory_Provider*)apiProvider->FindDefault(apiProvider, TinyCLR_Api_Type::MemoryProvider);

                State->Queues[idx] = (USB_PACKET64*)memoryProvider->Allocate(memoryProvider, STM32F4_USB_FIFO_BUFFER_SIZE * sizeof(USB_PACKET64));

                if (State->Queues[idx] == nullptr)
                    return TinyCLR_Result::ArgumentNull;;

                memset(reinterpret_cast<uint8_t*>(State->Queues[idx]), 0x00, STM32F4_USB_FIFO_BUFFER_SIZE * sizeof(USB_PACKET64));
            }

            STM32F4_UsbClient_ClearEndpoints(idx);

            epType |= (ep->bmAttributes & 3) << (idx * 2);
            State->MaxPacketSize[idx] = ep->wMaxPacketSize;
        }

        ep = (TinyCLR_UsbClient_EndpointDescriptor  *)(((uint8_t *)ep) + ep->bLength);
    }

    ((STM32F4_UsbClient_State*)State)->endpointType = epType;

    if (State->CurrentState == USB_DEVICE_STATE_UNINITIALIZED) {
        STM32F4_UsbClient_Initialize(controller);
    }

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_Close(const TinyCLR_UsbClient_Provider* self, int32_t pipe) {
    int32_t controller = self->Index;

    USB_CONTROLLER_STATE * State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (!State->Initialized || pipe >= STM32F4_USB_QUEUE_SIZE)
        return TinyCLR_Result::NotAvailable;

    DISABLE_INTERRUPTS_SCOPED(irq);

    // Close the Rx pipe
    int endpoint = State->pipes[pipe].RxEP;
    if (endpoint != USB_ENDPOINT_NULL && State->Queues[endpoint]) {
        STM32F4_UsbClient_ClearEndpoints(endpoint);
    }

    State->pipes[pipe].RxEP = USB_ENDPOINT_NULL;
    //Free endpoint
    STM32F4_UsbClient_EndpointMap[endpoint] &= ~ENDPOINT_INUSED_MASK;

    // Close the TX pipe
    endpoint = State->pipes[pipe].TxEP;
    if (endpoint != USB_ENDPOINT_NULL && State->Queues[endpoint] != nullptr) {
        STM32F4_UsbClient_ClearEndpoints(endpoint);

        if (apiProvider != nullptr) {
            auto memoryProvider = (const TinyCLR_Memory_Provider*)apiProvider->FindDefault(apiProvider, TinyCLR_Api_Type::MemoryProvider);

            if (State->Queues[endpoint] != nullptr)
                memoryProvider->Free(memoryProvider, State->Queues[endpoint]);

            State->Queues[endpoint] = nullptr;
        }
    }

    State->pipes[pipe].TxEP = USB_ENDPOINT_NULL;

    //Free endpoint
    STM32F4_UsbClient_EndpointMap[endpoint] &= ~ENDPOINT_INUSED_MASK;

    configDescriptor.epWrite.bEndpointAddress = USB_ENDPOINT_DIRECTION_IN;
    configDescriptor.epRead.bEndpointAddress = USB_ENDPOINT_DIRECTION_OUT;

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_Write(const TinyCLR_UsbClient_Provider* self, int32_t pipe, const uint8_t* data, size_t& length) {
    int32_t controller = self->Index;

    USB_CONTROLLER_STATE * State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (pipe >= STM32F4_USB_QUEUE_SIZE
        || data == nullptr
        || State->DeviceState != USB_DEVICE_STATE_CONFIGURED
        || length == 0) {
        return TinyCLR_Result::ArgumentInvalid;
    }

    int endpoint = State->pipes[pipe].TxEP;
    // If no Write side to pipe (or if not yet open)
    if (endpoint == USB_ENDPOINT_NULL || State->Queues[endpoint] == nullptr) {
        return TinyCLR_Result::NotAvailable;
    }

    DISABLE_INTERRUPTS_SCOPED(irq);

    const uint8_t*      ptr = data;
    uint32_t            count = length;
    bool                Done = false;
    uint32_t            WaitLoopCnt = 0;
    int                 totWrite = 0;

    // This loop packetizes the data and sends it out.  All packets sent have
    // the maximum length for the given endpoint except for the last packet which
    // will always have less than the maximum length - even if the packet length
    // must be zero for this to occur.   This is done to comply with standard
    // USB bulk-mode transfers.
    while (!Done) {

        USB_PACKET64* Packet64 = nullptr;

        if (usb_fifo_buffer_count[endpoint] < STM32F4_USB_FIFO_BUFFER_SIZE) {
            Packet64 = &State->Queues[endpoint][usb_fifo_buffer_in[endpoint]];

            usb_fifo_buffer_in[endpoint]++;
            usb_fifo_buffer_count[endpoint]++;

            if (usb_fifo_buffer_in[endpoint] == STM32F4_USB_FIFO_BUFFER_SIZE)
                usb_fifo_buffer_in[endpoint] = 0;
        }

        if (Packet64) {
            uint32_t max_move;

            if (count > State->MaxPacketSize[endpoint])
                max_move = State->MaxPacketSize[endpoint];
            else
                max_move = count;

            if (max_move) {
                memcpy(Packet64->Buffer, ptr, max_move);
            }

            // we are done when we send a non-full length packet
            if (max_move < State->MaxPacketSize[endpoint]) {
                Done = true;
            }

            Packet64->Size = max_move;
            count -= max_move;
            ptr += max_move;

            totWrite += max_move;

            WaitLoopCnt = 0;
        }
        if (Packet64 == nullptr) {
            // a 64-byte USB packet takes less than 50uSec
            // according to the timing calculations of the USB Chief
            // this is way too short to bother with a call
            // to WaitForEventsInternal, so just uSec delay the path
            // here for 50uSec.

            // if in ISR, return

            // if more than 100*50us=5ms,still no packet avaialable, PC side go wrong,stop the loop
            // otherwise it will spin here forever and stopwatch get kick in.
            WaitLoopCnt++;
            if (WaitLoopCnt > 100) {
                // if we were unable to send any data then no one is listening so lets
                if (count == length) {
                    STM32F4_UsbClient_ClearEndpoints(endpoint);
                }

                goto done_write;
            }

            if (irq.IsDisabled()) // @todo - this really needs more checks to be totally valid
            {
                goto done_write;
            }

            if (State->DeviceState != USB_DEVICE_STATE_CONFIGURED) {
                goto done_write;
            }

            STM32F4_UsbClient_StartOutput(State, endpoint);

            irq.Release();

            STM32F4_Time_Delay(nullptr, 50);

            irq.Acquire();
        }
    }

    // here we have a post-condition that IRQs are disabled for all paths through conditional block above
    if (State->DeviceState == USB_DEVICE_STATE_CONFIGURED) {
        STM32F4_UsbClient_StartOutput(State, endpoint);
    }

done_write:
    length = totWrite;

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_Read(const TinyCLR_UsbClient_Provider* self, int32_t pipe, uint8_t* data, size_t& length) {
    int32_t controller = self->Index;

    int endpoint;
    USB_CONTROLLER_STATE * State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (pipe >= STM32F4_USB_QUEUE_SIZE
        || State->DeviceState != USB_DEVICE_STATE_CONFIGURED) {
        return TinyCLR_Result::ArgumentInvalid;
    }

    endpoint = State->pipes[pipe].RxEP;
    // If no Read side to pipe (or if not yet open)
    if (endpoint == USB_ENDPOINT_NULL || State->Queues[endpoint] == nullptr) {
        return TinyCLR_Result::NotAvailable;
    }

    DISABLE_INTERRUPTS_SCOPED(irq);

    USB_PACKET64* Packet64 = nullptr;
    uint8_t*        ptr = (uint8_t*)data;
    uint32_t        count = 0;
    uint32_t        remain = length;

    while (count < length) {
        uint32_t max_move;

        if (usb_fifo_buffer_count[endpoint] > 0) {
            Packet64 = &State->Queues[endpoint][usb_fifo_buffer_out[endpoint]];

            usb_fifo_buffer_count[endpoint]--;
            usb_fifo_buffer_out[endpoint]++;

            if (usb_fifo_buffer_out[endpoint] == STM32F4_USB_FIFO_BUFFER_SIZE) {
                usb_fifo_buffer_out[endpoint] = 0;
            }
        }

        if (!Packet64) {
            STM32F4_UsbClient_ClearEvent(State, 1 << endpoint);
            break;
        }

        max_move = Packet64->Size - State->CurrentPacketOffset[endpoint];
        if (remain < max_move) max_move = remain;

        memcpy(ptr, &Packet64->Buffer[State->CurrentPacketOffset[endpoint]], max_move);

        State->CurrentPacketOffset[endpoint] += max_move;
        ptr += max_move;
        count += max_move;
        remain -= max_move;

        /* if we're done with this packet, move onto the next */
        if (State->CurrentPacketOffset[endpoint] == Packet64->Size) {
            State->CurrentPacketOffset[endpoint] = 0;
            Packet64 = nullptr;

            STM32F4_UsbClient_RxEnable(State, endpoint);
        }
    }

    length = count;

    return TinyCLR_Result::Success;
}

#define USB_FLUSH_RETRY_COUNT 30
TinyCLR_Result STM32F4_UsbClient_Flush(const TinyCLR_UsbClient_Provider* self, int32_t pipe) {
    int32_t controller = self->Index;

    int endpoint;
    int retries = USB_FLUSH_RETRY_COUNT;
    int queueCnt;
    USB_CONTROLLER_STATE * State = &STM32F4_UsbClient_ControllerState[controller].state;

    if (pipe >= STM32F4_USB_QUEUE_SIZE) {
        return TinyCLR_Result::ArgumentInvalid;
    }

    /* not configured, no data can go in or out */
    if (State->DeviceState != USB_DEVICE_STATE_CONFIGURED) {
        return TinyCLR_Result::InvalidOperation;
    }

    endpoint = State->pipes[pipe].TxEP;
    // If no Write side to pipe (or if not yet open)
    if (endpoint == USB_ENDPOINT_NULL || State->Queues[endpoint] == nullptr) {
        return TinyCLR_Result::NotAvailable;
    }

    queueCnt = usb_fifo_buffer_count[endpoint];

    // interrupts were disabled or USB interrupt was disabled for whatever reason, so force the flush
    while (usb_fifo_buffer_count[endpoint] > 0 && retries > 0) {
        STM32F4_UsbClient_StartOutput(State, endpoint);

        STM32F4_Time_Delay(nullptr, queueCnt == usb_fifo_buffer_count[endpoint] ? 100 : 0); // don't call Events_WaitForEventsXXX because it will turn off interrupts

        retries = (queueCnt == usb_fifo_buffer_count[endpoint]) ? retries - 1 : USB_FLUSH_RETRY_COUNT;

        queueCnt = usb_fifo_buffer_count[endpoint];
    }

    if (retries <= 0)
        STM32F4_UsbClient_ClearEndpoints(endpoint);

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_SetDataReceivedHandler(const TinyCLR_UsbClient_Provider* self, TinyCLR_UsbClient_DataReceivedHandler handler) {
    int32_t controller = self->Index;

    STM32F4_UsbClient_DataReceivedHandler = handler;

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_SetOsExtendedPropertyHandler(const TinyCLR_UsbClient_Provider* self, TinyCLR_UsbClient_OsExtendedPropertyHandler handler) {
    int32_t controller = self->Index;

    STM32F4_UsbClient_OsExtendedPropertyHandler = handler;

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_SetDeviceDescriptor(const TinyCLR_UsbClient_Provider* self, const void* descriptor, int32_t length) {
    memcpy(&deviceDescriptor, descriptor, length);

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_SetConfigDescriptor(const TinyCLR_UsbClient_Provider* self, const void* descriptor, int32_t length) {
    memcpy(&configDescriptor, descriptor, length);

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_UsbClient_SetStringDescriptor(const TinyCLR_UsbClient_Provider* self, TinyCLR_UsbClient_StringDescriptorType type, const wchar_t* value) {
    switch (type) {
    case TinyCLR_UsbClient_StringDescriptorType::ManufacturerName:
        memcpy(&stringManufacturerDescriptorHeader.stringDescriptor, value, sizeof(wchar_t) * SIZEOF_ARRAY(stringManufacturerDescriptorHeader.stringDescriptor));
        break;

    case TinyCLR_UsbClient_StringDescriptorType::ProductName:
        memcpy(&stringProductNameDescriptorHeader.stringDescriptor, value, sizeof(wchar_t) * SIZEOF_ARRAY(stringManufacturerDescriptorHeader.stringDescriptor));
        break;

    case TinyCLR_UsbClient_StringDescriptorType::DisplayName:
        memcpy(&stringDisplayNameDescriptorHeader.stringDescriptor, value, sizeof(wchar_t) * SIZEOF_ARRAY(stringManufacturerDescriptorHeader.stringDescriptor));
        break;

    case TinyCLR_UsbClient_StringDescriptorType::FriendlyName:
        memcpy(&stringFriendlyNameDescriptorHeader.stringDescriptor, value, sizeof(wchar_t) * SIZEOF_ARRAY(stringManufacturerDescriptorHeader.stringDescriptor));
        break;
    }

    return TinyCLR_Result::Success;
}

void STM32F4_UsbClient_Reset() {
    for (auto pipe = 0; pipe < STM32F4_USB_QUEUE_SIZE; pipe++) {
        STM32F4_UsbClient_Close(&usbClientProvider, pipe);
    }

    STM32F4_UsbClient_Release(&usbClientProvider);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// STM32F4 USB Hardware driver
///////////////////////////////////////////////////////////////////////////////////////////
#define OTG_FS_BASE           (0x50000000)
#define OTG_FS                ((OTG_TypeDef *) OTG_FS_BASE)

#define OTG_GUSBCFG_PHYSEL    (1<<6)
#define OTG_GUSBCFG_PHYLPCS   (1<<15)
#define OTG_GUSBCFG_FDMOD     (1<<30)

#define OTG_GCCFG_PWRDWN      (1<<16)
#define OTG_GCCFG_VBUSASEN    (1<<18)
#define OTG_GCCFG_VBUSBSEN    (1<<19)
#define OTG_GCCFG_SOFOUTEN    (1<<20)
#define OTG_GCCFG_NOVBUSSENS  (1<<21)

#define OTG_GAHBCFG_GINTMSK   (1<<0)
#define OTG_GAHBCFG_TXFELVL   (1<<7)

#define OTG_GINTSTS_MMIS      (1<<1)
#define OTG_GINTSTS_OTGINT    (1<<2)
#define OTG_GINTSTS_RXFLVL    (1<<4)
#define OTG_GINTSTS_USBSUSP   (1<<11)
#define OTG_GINTSTS_USBRST    (1<<12)
#define OTG_GINTSTS_ENUMDNE   (1<<13)
#define OTG_GINTSTS_IEPINT    (1<<18)
#define OTG_GINTSTS_OEPINT    (1<<19)
#define OTG_GINTSTS_SRQINT    (1<<30)
#define OTG_GINTSTS_WKUPINT   (1U<<31)

#define OTG_GINTMSK_MMISM     (1<<1)
#define OTG_GINTMSK_OTGINT    (1<<2)
#define OTG_GINTMSK_RXFLVLM   (1<<4)
#define OTG_GINTMSK_USBSUSPM  (1<<11)
#define OTG_GINTMSK_USBRST    (1<<12)
#define OTG_GINTMSK_ENUMDNEM  (1<<13)
#define OTG_GINTMSK_IEPINT    (1<<18)
#define OTG_GINTMSK_OEPINT    (1<<19)
#define OTG_GINTMSK_SRQIM     (1<<30)
#define OTG_GINTMSK_WUIM      (1U<<31)

#define OTG_GRSTCTL_CSRST     (1<<0)
#define OTG_GRSTCTL_RXFFLSH   (1<<4)
#define OTG_GRSTCTL_TXFFLSH   (1<<5)
#define OTG_GRSTCTL_TXFNUM    (0x1F<<6)
#define OTG_GRSTCTL_TXF_ALL   (0x10<<6)

#define OTG_DCFG_DSPD         (0x3<<0)
#define OTG_DCFG_DAD          (0x7F<<4)

#define OTG_DCTL_RWUSIG       (1<<0)
#define OTG_DCTL_SDIS         (1<<1)
#define OTG_DCTL_POPRGDNE     (1<<11)

#define OTG_GRXSTSP_EPNUM     (0x0F<<0)
#define OTG_GRXSTSP_CHNUM     (0x0F<<0)
#define OTG_GRXSTSP_BCNT      (0x7FF<<4)
#define OTG_GRXSTSP_DPID      (0x03<<15)
#define OTG_GRXSTSP_PKTSTS    (0x0F<<17)
#define OTG_GRXSTSP_PKTSTS_GN (0x01<<17) // global OUT NAK
#define OTG_GRXSTSP_PKTSTS_PR (0x02<<17) // packet received
#define OTG_GRXSTSP_PKTSTS_DC (0x03<<17) // data transaction completed
#define OTG_GRXSTSP_PKTSTS_SC (0x04<<17) // setup stage completed
#define OTG_GRXSTSP_PKTSTS_TE (0x05<<17) // toggle error
#define OTG_GRXSTSP_PKTSTS_SR (0x06<<17) // setup data received
#define OTG_GRXSTSP_PKTSTS_CH (0x07<<17) // channel haltet
#define OTG_GRXSTSP_FRMNUM    (0x0F<<21)

#define OTG_DIEPMSK_XFRCM     (1<<0) // transfer completed
#define OTG_DIEPMSK_TOM       (1<<3) // timeout

#define OTG_DOEPMSK_XFRCM     (1<<0) // transfer completed
#define OTG_DOEPMSK_STUPM     (1<<3) // setup phase done

#define OTG_DIEPINT_XFRC      (1<<0) // transfer completed
#define OTG_DIEPINT_TOC       (1<<3) // timeout

#define OTG_DOEPINT_XFRC      (1<<0) // transfer completed
#define OTG_DOEPINT_STUP      (1<<3) // setup phase done

#define OTG_DIEPCTL_USBAEP    (1<<15)
#define OTG_DIEPCTL_STALL     (1<<21)
#define OTG_DIEPCTL_CNAK      (1<<26)
#define OTG_DIEPCTL_SNAK      (1<<27)
#define OTG_DIEPCTL_SD0PID    (1<<28)
#define OTG_DIEPCTL_EPDIS     (1<<30)
#define OTG_DIEPCTL_EPENA     (1U<<31)

#define OTG_DOEPCTL_USBAEP    (1<<15)
#define OTG_DOEPCTL_STALL     (1<<21)
#define OTG_DOEPCTL_CNAK      (1<<26)
#define OTG_DOEPCTL_SNAK      (1<<27)
#define OTG_DOEPCTL_SD0PID    (1<<28)
#define OTG_DOEPCTL_EPDIS     (1<<30)
#define OTG_DOEPCTL_EPENA     (1U<<31)

#define OTG_DIEPTSIZ_PKTCNT   (3<<19)
#define OTG_DIEPTSIZ_PKTCNT_1 (1<<19)

#define OTG_DOEPTSIZ_PKTCNT   (3<<19)
#define OTG_DOEPTSIZ_PKTCNT_1 (1<<19)
#define OTG_DOEPTSIZ_STUPCNT  (3<<29)

#define STM32F4_USB_FS_USE_ID_PIN 0
#define STM32F4_USB_FS_USE_VB_PIN 0

// use OTG Full Speed
#define STM32F4_USB_FS_ID 0

#define STM32F4_USB_USE_ID_PIN(c) STM32F4_USB_FS_USE_ID_PIN
#define STM32F4_USB_USE_VB_PIN(c) STM32F4_USB_FS_USE_VB_PIN

#define USB_MAX_BUFFERS (USB_FS_MAX_EP_COUNT - 1)

// FIFO sizes (in 32 bit words)
#define USB_RXFIFO_SIZE  64
#define USB_TX0FIFO_SIZE 64
#define USB_TXnFIFO_SIZE 64

// PHY turnaround time
// (4 AHB clocks + 1 Phy clock in Phy clocks)
#define STM32F4_USB_TRDT ((4 * 48000000 - 1) / STM32F4_AHB_CLOCK_HZ + 2)

#define USB_OTG_NUM_FIFOS                8
#define USB_OTG_NUM_CHANNELS            16

typedef struct {
    // global registers
    __IO uint32_t GOTGCTL;
    __IO uint32_t GOTGINT;
    __IO uint32_t GAHBCFG;
    __IO uint32_t GUSBCFG;
    __IO uint32_t GRSTCTL;
    __IO uint32_t GINTSTS;
    __IO uint32_t GINTMSK;
    __IO uint32_t GRXSTSR;
    __IO uint32_t GRXSTSP;
    __IO uint32_t GRXFSIZ;
    union {
        __IO uint32_t HNPTXFSIZ;
        __IO uint32_t DIEPTXF0;
    };
    __IO uint32_t HNPTXSTS;
    uint32_t Res1[2];
    __IO uint32_t GCCFG;
    __IO uint32_t CID;
    uint32_t Res2[48];
    union {
        __IO uint32_t HPTXFSIZ;
        __IO uint32_t DIEPTXF[USB_OTG_NUM_FIFOS];
    };
    uint32_t Res3[184];
    // host mode registers
    __IO uint32_t HCFG;
    __IO uint32_t HFIR;
    __IO uint32_t HFNUM;
    uint32_t Res4;
    __IO uint32_t HPTXSTS;
    __IO uint32_t HAINT;
    __IO uint32_t HAINTMSK;
    uint32_t Res5[9];
    __IO uint32_t HPRT;
    uint32_t Res6[47];
    struct {
        __IO uint32_t CHAR;
        uint32_t Res7;
        __IO uint32_t INT;
        __IO uint32_t INTMSK;
        __IO uint32_t TSIZ;
        uint32_t Res8[3];
    } HC[USB_OTG_NUM_CHANNELS];
    uint32_t Res9[64];
    // device mode registers
    __IO uint32_t DCFG;
    __IO uint32_t DCTL;
    __IO uint32_t DSTS;
    uint32_t Res10;
    __IO uint32_t DIEPMSK;
    __IO uint32_t DOEPMSK;
    __IO uint32_t DAINT;
    __IO uint32_t DAINTMSK;
    uint32_t Res11[2];
    __IO uint32_t DVBUSDIS;
    __IO uint32_t DVBUSPULSE;
    uint32_t Res12;
    __IO uint32_t DIEPEMPMSK;
    uint32_t Res13[50];
    struct {
        __IO uint32_t CTL;
        uint32_t Res14;
        __IO uint32_t INT;
        uint32_t Res15;
        __IO uint32_t TSIZ;
        uint32_t Res16;
        __IO uint32_t TXFSTS;
        uint32_t Res17;
    } DIEP[USB_OTG_NUM_CHANNELS];
    struct {
        __IO uint32_t CTL;
        uint32_t Res18;
        __IO uint32_t INT;
        uint32_t Res19;
        __IO uint32_t TSIZ;
        uint32_t Res20[3];
    } DOEP[USB_OTG_NUM_CHANNELS];
    uint32_t Res21[64];
    // power and clock gating
    __IO uint32_t PCGCCTL;
    uint32_t Res22[127];
    // FIFO regions
    __IO uint32_t DFIFO[USB_OTG_NUM_FIFOS][1024];
}
OTG_TypeDef;

void STM32F4_UsbClient_ProtectPins(int controller, bool On);
void STM32F4_UsbClient_Interrupt(void* param);

bool STM32F4_UsbClient_Initialize(int controller) {
    if ((uint32_t)controller >= TOTAL_USB_CONTROLLERS)
        return false;

    auto& dp = g_STM32F4_Usb_Dp_Pins[controller];
    auto& dm = g_STM32F4_Usb_Dm_Pins[controller];
    auto& id = g_STM32F4_Usb_Id_Pins[controller];

    if (!STM32F4_GpioInternal_OpenPin(dp.number) || !STM32F4_GpioInternal_OpenPin(dm.number))
        return false;

    if (STM32F4_USB_USE_ID_PIN(controller) && !STM32F4_GpioInternal_OpenPin(id.number))
        return false;

    // enable USB clock
    // FS on AHB2
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

    USB_CONTROLLER_STATE *State = &STM32F4_UsbClient_ControllerState[controller].state;

    OTG_TypeDef* OTG = OTG_FS;

    DISABLE_INTERRUPTS_SCOPED(irq);

    // Detach usb port for a while to enforce re-initialization
    OTG->DCTL = OTG_DCTL_SDIS; // soft disconnect

    OTG->GAHBCFG = OTG_GAHBCFG_TXFELVL;     // int on TxFifo completely empty, int off
    OTG->GUSBCFG = OTG_GUSBCFG_FDMOD        // force device mode
        | STM32F4_USB_TRDT << 10   // turnaround time
        | OTG_GUSBCFG_PHYSEL;      // internal PHY

    OTG->GCCFG = OTG_GCCFG_VBUSBSEN       // B device Vbus sensing
        | OTG_GCCFG_PWRDWN;        // transceiver enabled

    OTG->DCFG |= OTG_DCFG_DSPD;           // device speed = HS

    if (STM32F4_USB_USE_VB_PIN(controller) == 0) { // no Vbus pin
        OTG->GCCFG |= OTG_GCCFG_NOVBUSSENS; // disable vbus sense
    }

    STM32F4_Time_Delay(nullptr, 1000); // asure host recognizes reattach

    // setup hardware
    STM32F4_UsbClient_ProtectPins(controller, true);

    STM32F4_InterruptInternal_Activate(OTG_FS_IRQn, (uint32_t*)&STM32F4_UsbClient_Interrupt, 0);
    STM32F4_InterruptInternal_Activate(OTG_FS_WKUP_IRQn, (uint32_t*)&STM32F4_UsbClient_Interrupt, 0);

    // allow interrupts
    OTG->GINTSTS = 0xFFFFFFFF;           // clear all interrupts
    OTG->GINTMSK = OTG_GINTMSK_USBRST;   // enable reset only
    OTG->DIEPEMPMSK = 0;                 // disable Tx FIFO empty interrupts
    OTG->GAHBCFG |= OTG_GAHBCFG_GINTMSK; // gloabl interrupt enable

#if defined(STM32F413xx)
    OTG->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN | USB_OTG_GOTGCTL_BVALOVAL;
#endif

    return true;
}

bool STM32F4_UsbClient_Uninitialize(int controller) {
    STM32F4_InterruptInternal_Deactivate(OTG_FS_WKUP_IRQn);
    STM32F4_InterruptInternal_Deactivate(OTG_FS_IRQn);

    STM32F4_UsbClient_ProtectPins(controller, false);

    RCC->AHB2ENR &= ~RCC_AHB2ENR_OTGFSEN;

    USB_CONTROLLER_STATE *State = &STM32F4_UsbClient_ControllerState[controller].state;

    State->CurrentState = USB_DEVICE_STATE_UNINITIALIZED;

    return true;
}

void STM32F4_UsbClient_ResetEvent(OTG_TypeDef* OTG, USB_CONTROLLER_STATE* State) {
    // reset interrupts and FIFOs
    OTG->GINTSTS = 0xFFFFFFFF; // clear global interrupts
    OTG->GRXFSIZ = USB_RXFIFO_SIZE; // Rx Fifo
    OTG->DIEPTXF0 = (USB_TX0FIFO_SIZE << 16) | USB_RXFIFO_SIZE; // Tx Fifo 0
    uint32_t addr = USB_RXFIFO_SIZE + USB_TX0FIFO_SIZE;
    for (int i = 0; i < State->EndpointCount; i++) {
        OTG->DIEPTXF[i] = (USB_TXnFIFO_SIZE << 16) | addr; // Tx Fifo i
        addr += USB_TXnFIFO_SIZE;
        OTG->DIEP[i].INT = 0xFF; // clear endpoint interrupts
        OTG->DOEP[i].INT = 0xFF;
        OTG->DIEP[i].CTL = OTG_DIEPCTL_EPDIS; // deactivate endpoint
        OTG->DOEP[i].CTL = OTG_DOEPCTL_EPDIS;
    }

    // flush FIFOs
    OTG->GRSTCTL = OTG_GRSTCTL_RXFFLSH | OTG_GRSTCTL_TXFFLSH | OTG_GRSTCTL_TXF_ALL;

    // configure control endpoint
    OTG->DIEP[0].CTL = OTG_DIEPCTL_USBAEP; // Tx FIFO num = 0, max packet size = 64
    OTG->DOEP[0].CTL = OTG_DOEPCTL_USBAEP;
    OTG->DIEP[0].TSIZ = 0;
    OTG->DOEP[0].TSIZ = OTG_DOEPTSIZ_STUPCNT; // up to 3 setup packets

    // configure data endpoints
    uint32_t intMask = 0x00010001; // ep0 interrupts;
    uint32_t eptype = ((STM32F4_UsbClient_State*)State)->endpointType >> 2; // endpoint types (2 bits / endpoint)
    uint32_t i = 1, bit = 2;
    while (eptype) {
        uint32_t type = eptype & 3;
        if (type != 0) { // data endpoint
            uint32_t ctrl = OTG_DIEPCTL_SD0PID | OTG_DIEPCTL_USBAEP;
            ctrl |= type << 18; // endpoint type
            ctrl |= State->MaxPacketSize[i]; // packet size
            if (State->IsTxQueue[i]) { // Tx (in) endpoint
                ctrl |= OTG_DIEPCTL_SNAK; // disable tx endpoint
                ctrl |= i << 22; // Tx FIFO number
                OTG->DIEP[i].CTL = ctrl; // configure in endpoint
                intMask |= bit; // enable in interrupt
            }
            else { // Rx (out) endpoint
                // Rx endpoints must be enabled here
                // Enabling after Set_Configuration does not work correctly
                OTG->DOEP[i].TSIZ = OTG_DOEPTSIZ_PKTCNT_1 | State->MaxPacketSize[i];
                ctrl |= OTG_DOEPCTL_EPENA | OTG_DOEPCTL_CNAK; // enable rx endpoint
                OTG->DOEP[i].CTL = ctrl; // configure out endpoint
                intMask |= bit << 16; // enable out interrupt
            }
        }
        i++;
        eptype >>= 2;
        bit <<= 1;
    }

    // enable interrupts
    OTG->DIEPMSK = OTG_DIEPMSK_XFRCM; // transfer complete
    OTG->DOEPMSK = OTG_DOEPMSK_XFRCM | OTG_DOEPMSK_STUPM; // setup stage done
    OTG->DAINTMSK = intMask;   // enable ep interrupts
    OTG->GINTMSK = OTG_GINTMSK_OEPINT | OTG_GINTMSK_IEPINT | OTG_GINTMSK_RXFLVLM
        | OTG_GINTMSK_USBRST | OTG_GINTMSK_USBSUSPM | OTG_GINTMSK_WUIM;

    OTG->DCFG &= ~OTG_DCFG_DAD; // reset device address

    /* clear all flags */
    STM32F4_UsbClient_ClearEvent(State, 0xFFFFFFFF); // clear all events on all endpoints

    State->FirstGetDescriptor = true;

    State->DeviceState = USB_DEVICE_STATE_DEFAULT;
    State->Address = 0;
    STM32F4_UsbClient_StateCallback(State);
}

void STM32F4_UsbClient_EndpointRxInterrupt(OTG_TypeDef* OTG, USB_CONTROLLER_STATE* State, uint32_t ep, uint32_t count) {
    uint32_t* pd;

    bool disableRx = false;

    if (ep == 0) { // control endpoint
        pd = (uint32_t*)((STM32F4_UsbClient_State*)State)->ep0Buffer;
        State->Data = (uint8_t*)pd;
        State->DataSize = count;
    }
    else { // data endpoint
        USB_PACKET64* Packet64 = STM32F4_UsbClient_RxEnqueue(State, ep, disableRx);

        if (disableRx) return;

        pd = (uint32_t*)Packet64->Buffer;
        Packet64->Size = count;
    }

    // read data
    uint32_t volatile* ps = OTG->DFIFO[ep];
    for (int c = count; c > 0; c -= 4) {
        *pd++ = *ps;
    }
}

void STM32F4_UsbClient_EndpointInInterrupt(OTG_TypeDef* OTG, USB_CONTROLLER_STATE* State, uint32_t ep) {
    uint32_t bits = OTG->DIEP[ep].INT;
    if (bits & OTG_DIEPINT_XFRC) { // transfer completed
        OTG->DIEP[ep].INT = OTG_DIEPINT_XFRC; // clear interrupt
    }

    if (!(OTG->DIEP[ep].CTL & OTG_DIEPCTL_EPENA)) { // Tx idle
        uint32_t* ps = 0;
        uint32_t count;

        if (ep == 0) { // control endpoint
            if (State->DataCallback) { // data to send
                State->DataCallback(State);  // this call can't fail
                ps = (uint32_t*)State->Data;
                count = State->DataSize;
            }
        }
        else if (State->Queues[ep] != 0 && State->IsTxQueue[ep]) { // Tx data endpoint

            USB_PACKET64* Packet64 = STM32F4_UsbClient_TxDequeue(State, ep);

            if (Packet64) {  // data to send
                ps = (uint32_t*)Packet64->Buffer;
                count = Packet64->Size;
            }
        }

        if (ps) { // data to send
            // enable endpoint
            OTG->DIEP[ep].TSIZ = OTG_DIEPTSIZ_PKTCNT_1 | count;
            OTG->DIEP[ep].CTL |= OTG_DIEPCTL_EPENA | OTG_DIEPCTL_CNAK;

            // write data
            uint32_t volatile* pd = OTG->DFIFO[ep];
            for (int c = count; c > 0; c -= 4) {
                *pd = *ps++;
            }
        }
        else { // no data
            // disable endpoint
            OTG->DIEP[ep].CTL |= OTG_DIEPCTL_SNAK;
        }
    }
}

void STM32F4_UsbClient_HandleSetup(OTG_TypeDef* OTG, USB_CONTROLLER_STATE* State) {
    /* send last setup packet to the upper layer */
    uint8_t result = STM32F4_UsbClient_ControlCallback(State);

    switch (result) {

    case USB_STATE_ADDRESS:
        /* upper layer needs us to change the address */
        OTG->DCFG |= State->Address << 4; // set device address
        break;

    case USB_STATE_DONE:
        State->DataCallback = 0;
        break;

    case USB_STATE_STALL:
        // setup packet failed to process successfully
        // set stall condition on the control endpoint
        OTG->DIEP[0].CTL |= OTG_DIEPCTL_STALL;
        OTG->DOEP[0].CTL |= OTG_DOEPCTL_STALL;

        // ********** skip rest of function **********
        return;
    }

    // check ep0 for replies
    STM32F4_UsbClient_EndpointInInterrupt(OTG, State, 0);

    // check all Tx endpoints after configuration setup
    if (result == USB_STATE_CONFIGURATION) {
        for (int ep = 1; ep < State->EndpointCount; ep++) {
            if (State->Queues[ep] && State->IsTxQueue[ep]) {
                STM32F4_UsbClient_EndpointInInterrupt(OTG, State, ep);
            }
        }
    }
}

void STM32F4_UsbClient_EndpointOutInterrupt(OTG_TypeDef* OTG, USB_CONTROLLER_STATE* State, uint32_t ep) {
    uint32_t bits = OTG->DOEP[ep].INT;
    if (bits & OTG_DOEPINT_XFRC) { // transfer completed
        OTG->DOEP[ep].INT = OTG_DOEPINT_XFRC; // clear interrupt
    }

    if (bits & OTG_DOEPINT_STUP) { // setup phase done
        OTG->DOEP[ep].INT = OTG_DOEPINT_STUP; // clear interrupt
    }

    if (ep == 0) { // control endpoint
        // enable endpoint
        OTG->DOEP[0].TSIZ = OTG_DOEPTSIZ_STUPCNT | OTG_DOEPTSIZ_PKTCNT_1 | State->PacketSize;
        OTG->DOEP[0].CTL |= OTG_DOEPCTL_EPENA | OTG_DOEPCTL_CNAK;
        // Handle Setup data in upper layer
        STM32F4_UsbClient_HandleSetup(OTG, State);
    }
    else if (usb_fifo_buffer_count[ep] < STM32F4_USB_FIFO_BUFFER_SIZE) {
        // enable endpoint
        OTG->DOEP[ep].TSIZ = OTG_DOEPTSIZ_PKTCNT_1 | State->MaxPacketSize[ep];
        OTG->DOEP[ep].CTL |= OTG_DOEPCTL_EPENA | OTG_DOEPCTL_CNAK;
    }
    else {
        // disable endpoint
        OTG->DOEP[ep].CTL |= OTG_DOEPCTL_SNAK;
    }
}

void STM32F4_UsbClient_Interrupt(void* param) {
    INTERRUPT_STARTED_SCOPED(isr);

    DISABLE_INTERRUPTS_SCOPED(irq);

    OTG_TypeDef* OTG = OTG_FS;
    USB_CONTROLLER_STATE* State = &STM32F4_UsbClient_ControllerState[STM32F4_USB_FS_ID].state;

    uint32_t intPend = OTG->GINTSTS; // get pending bits

    while (intPend & OTG_GINTSTS_RXFLVL) { // RxFifo non empty
        uint32_t status = OTG->GRXSTSP; // read and pop status word from fifo
        int ep = status & OTG_GRXSTSP_EPNUM;
        int count = (status & OTG_GRXSTSP_BCNT) >> 4;
        status &= OTG_GRXSTSP_PKTSTS;
        if (status == OTG_GRXSTSP_PKTSTS_PR // data received
            || status == OTG_GRXSTSP_PKTSTS_SR // setup received
            ) {
            STM32F4_UsbClient_EndpointRxInterrupt(OTG, State, ep, count);
        }
        else {
            // others: nothing to do
        }
        intPend = OTG->GINTSTS; // update pending bits
    }

    if (intPend & OTG_GINTSTS_IEPINT) { // IN endpoint
        uint32_t bits = OTG->DAINT & 0xFFFF; // pending IN endpoints
        int ep = 0;
        while (bits) {
            if (bits & 1)
                STM32F4_UsbClient_EndpointInInterrupt(OTG, State, ep);
            ep++;
            bits >>= 1;
        }
    }

    if (intPend & OTG_GINTSTS_OEPINT) { // OUT endpoint
        uint32_t bits = OTG->DAINT >> 16; // pending OUT endpoints
        int ep = 0;
        while (bits) {
            if (bits & 1)
                STM32F4_UsbClient_EndpointOutInterrupt(OTG, State, ep);

            ep++;
            bits >>= 1;
        }
    }

    if (intPend & OTG_GINTSTS_USBRST) { // reset
        STM32F4_UsbClient_ResetEvent(OTG, State);
        OTG->GINTSTS = OTG_GINTSTS_USBRST; // clear interrupt
    }
    else {
        if (intPend & OTG_GINTSTS_USBSUSP) { // suspend
            ((STM32F4_UsbClient_State*)State)->previousDeviceState = State->DeviceState;

            State->DeviceState = USB_DEVICE_STATE_SUSPENDED;

            STM32F4_UsbClient_StateCallback(State);

            OTG->GINTSTS = OTG_GINTSTS_USBSUSP; // clear interrupt
        }

        if (intPend & OTG_GINTSTS_WKUPINT) { // wakeup
            OTG->DCTL &= ~OTG_DCTL_RWUSIG; // remove remote wakeup signaling

            State->DeviceState = ((STM32F4_UsbClient_State*)State)->previousDeviceState;

            STM32F4_UsbClient_StateCallback(State);

            OTG->GINTSTS = OTG_GINTSTS_WKUPINT; // clear interrupt
        }
    }
}

bool STM32F4_UsbClient_StartOutput(USB_CONTROLLER_STATE* State, int ep) {
    if (State == 0 || ep >= State->EndpointCount)
        return false;

    OTG_TypeDef* OTG = OTG_FS;

    DISABLE_INTERRUPTS_SCOPED(irq);

    // If endpoint is not an output
    if (State->Queues[ep] == 0 || !State->IsTxQueue[ep])
        return false;

    /* if the halt feature for this endpoint is set, then just clear all the characters */
    if (State->EndpointStatus[ep] & USB_STATUS_ENDPOINT_HALT) {
        STM32F4_UsbClient_ClearEndpoints(ep);

        return true;
    }

    if (irq.IsDisabled()) { // check all endpoints for pending actions
        STM32F4_UsbClient_Interrupt((void *)OTG);
    }
    // write first packet if not done yet
    STM32F4_UsbClient_EndpointInInterrupt(OTG, State, ep);

    return true;
}

bool STM32F4_UsbClient_RxEnable(USB_CONTROLLER_STATE* State, int ep) {
    // If this is not a legal Rx queue
    if (State == 0 || State->Queues[ep] == 0 || State->IsTxQueue[ep])
        return false;

    OTG_TypeDef* OTG = OTG_FS;

    DISABLE_INTERRUPTS_SCOPED(irq);

    // enable Rx
    if (!(OTG->DOEP[ep].CTL & OTG_DOEPCTL_EPENA)) {
        OTG->DOEP[ep].TSIZ = OTG_DOEPTSIZ_PKTCNT_1 | State->MaxPacketSize[ep];
        OTG->DOEP[ep].CTL |= OTG_DOEPCTL_EPENA | OTG_DOEPCTL_CNAK; // enable endpoint
    }

    return true;
}

void STM32F4_UsbClient_ProtectPins(int controller, bool On) {
    USB_CONTROLLER_STATE *State = &STM32F4_UsbClient_ControllerState[controller].state;

    OTG_TypeDef* OTG = OTG_FS;

    DISABLE_INTERRUPTS_SCOPED(irq);

    auto& dp = g_STM32F4_Usb_Dp_Pins[controller];
    auto& dm = g_STM32F4_Usb_Dm_Pins[controller];
    auto& id = g_STM32F4_Usb_Id_Pins[controller];

    if (On) {

        STM32F4_GpioInternal_ConfigurePin(dp.number, STM32F4_Gpio_PortMode::AlternateFunction, STM32F4_Gpio_OutputType::PushPull, STM32F4_Gpio_OutputSpeed::VeryHigh, STM32F4_Gpio_PullDirection::None, dp.alternateFunction);
        STM32F4_GpioInternal_ConfigurePin(dm.number, STM32F4_Gpio_PortMode::AlternateFunction, STM32F4_Gpio_OutputType::PushPull, STM32F4_Gpio_OutputSpeed::VeryHigh, STM32F4_Gpio_PullDirection::None, dm.alternateFunction);

        if (STM32F4_USB_USE_ID_PIN(controller)) {
            STM32F4_GpioInternal_ConfigurePin(id.number, STM32F4_Gpio_PortMode::AlternateFunction, STM32F4_Gpio_OutputType::PushPull, STM32F4_Gpio_OutputSpeed::VeryHigh, STM32F4_Gpio_PullDirection::None, id.alternateFunction);
        }

        // attach usb port
        OTG->DCTL &= ~OTG_DCTL_SDIS; // remove soft disconnect
    }
    else {
        // detach usb port
        OTG->DCTL |= OTG_DCTL_SDIS; // soft disconnect

        // clear USB Txbuffer
        for (int ep = 1; ep < State->EndpointCount; ep++) {
            if (State->Queues[ep] && State->IsTxQueue[ep]) {
                STM32F4_UsbClient_ClearEndpoints(ep);
            }
        }

        STM32F4_GpioInternal_ClosePin(dp.number);
        STM32F4_GpioInternal_ClosePin(dm.number);

        if (STM32F4_USB_USE_ID_PIN(controller))
            STM32F4_GpioInternal_ClosePin(id.number);
    }

    State->DeviceState = On ? USB_DEVICE_STATE_ATTACHED : USB_DEVICE_STATE_DETACHED;

    STM32F4_UsbClient_StateCallback(State);
}
