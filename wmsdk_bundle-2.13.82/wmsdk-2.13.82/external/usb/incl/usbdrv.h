/*
 *
 * ============================================================================
 * Copyright (c) 2008-2010   Marvell International, Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */
/**
 *
 * \brief
 *
 */

#ifndef USBDRV_H
#define USBDRV_H

#include "usb_list.h"
#include "devapi.h"
#include "agLinkedList.h"
#include "wm_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USB 1.1 Setup Packet */
#ifdef CPU_LITTLE_ENDIAN
	typedef struct setup_struct_s {
		uint8_t REQUESTTYPE;
		uint8_t REQUEST;
		uint16_t VALUE;
		uint16_t INDEX;
		uint16_t LENGTH;
	} SETUP_STRUCT_t, *SETUP_STRUCT_PTR_t;
#else
	typedef struct setup_struct_s {
		uint16_t VALUE;
		uint8_t REQUEST;
		uint8_t REQUESTTYPE;
		uint16_t LENGTH;
		uint16_t INDEX;
	} SETUP_STRUCT_t, *SETUP_STRUCT_PTR_t;
#endif

//This is used for holding the hw details of a given interface.
//
	typedef struct USB_INTERFACE_DETAILS_s {
		ATLISTENTRY linkedList;	///< The linked list entry, must be first in this structure
		uint8_t OutNum;	///< The number of out endpoints.
		uint8_t InNum;	///< The number of in endpoints.
		uint8_t IntNum;	///< if this is zero, we don't have an interrupt endpoint.
		uint8_t EPType; ///<  Type of endpoint in the interface.  Only one type in one interface.
		uint8_t SizeOfInterfaceDesc;	///< Length of interface + endpoint descriptors.
		usb_intf_id_t interfaceID;	///< Interface ID
		void *InterfacePointer;	///< pointer to the interface descriptor.
		uint32_t ZeroTerminate;	///< how does the interface handle zero len packets.
		os_semaphore_t interfaceSemaphore;	///< The open semaphore.
		usb_intf_callback_t InterfaceCallback;	///< pointer to the interface callback
	} USB_INTERFACE_DETAILS_t;

// Device Callback enum
	typedef enum {
		USB_CLASS_REQUEST_CALLBACK,
		USB_VENDOR_REQUEST_CALLBACK,
		USB_RESET_CALLBACK,
		USB_SET_ADDRESS_CALLBACK,
		USB_DEVICE_DETACH_CALLBACK,
	} USB_CALLBACK_STATE_t;

#ifndef MIN
#define MIN(a, b) ( (a) > (b) ? (b) : (a) )
#endif

#ifndef MAX
#define MAX(a, b) ( (a) > (b) ? (a) : (b) )
#endif

/* Descriptor types, in hi-byte of wValue of struct _usb_setup */
#define USB_DEVICE_DESCRIPTOR       1   /* id for the device descriptor */
#define USB_CONFIG_DESCRIPTOR       2   /* id for the config descriptor */
#define USB_STRING_DESCRIPTOR       3   /* id for the usb string descriptor */
#define USB_INTERFACE_DESCRIPTOR    4   /* id for the interface descriptor */
#define USB_ENDPOINT_DESCRIPTOR     5   /* id for the endpoint descriptor */
#define USB_DEVICE_QUAL             6   /* id for the device qual descriptor */
#define USB_OS_CONFIG_DESCRIPTOR    7   /* id for the other config descriptor */

/* We specify 8 which gives a polling interval of .128
* secs for high speed and 1.02 seconds for low speed. */
#define USB_INT_EP_POLLING_INTERVAL  8  /* Interrupt endpoint polling interval. */

/*
typedef enum {
    e_CONTROL = 0,
    e_BULK = 2,
    e_INTERRUPT = 3
} EP_TYPE;
*/

#pragma pack(1)

/**
 * \brief structure for configuration descriptor
 */
typedef struct USB_CONFIG_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumberInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t MaxPower;
} USB_CONFIG_t;

/**
 * \brief structure for association descriptor
 */
typedef struct USB_ASSOCIATION_s {
    uint8_t bLength;    /* size of descriptor */
    uint8_t bDescriptorType;    /* this is 0x0B for association descriptor */
    uint8_t bFirstInterface;    /* interface number of the first interface */
    uint8_t bInterfaceCount;    /* number of contiguous interface */
    uint8_t bFunctionClass; /* class code */
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t biFunction;
} USB_ASSOCIATION_t;

/**
 * \brief structure for interface descriptor
 */
typedef struct USB_INTERFACE_s {
    uint8_t bLength;    /* size of descriptor */
    uint8_t bDescriptorType;    /* this is 2 for config descriptor */
    uint8_t bInterfaceNumber;   /* Number identifying this interface */
    uint8_t bAlternateSetting;  /* Value used to select an alternate setting */
    uint8_t bNumEndpoints;  /* number of endpoints */
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubclass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface; /* index of string to interface description */
} USB_INTERFACE_t;

/**
 * \brief structure for endpoint
 */
typedef struct USB_ENDPOINT_s {
    uint8_t bLength;    /* size of descriptor */
    uint8_t bDescriptorType;    /* this is 2 for config descriptor */
    uint8_t bEndpointAddress;   /* endpoint # and direction */
    uint8_t bmAttributes;   /* transfer type supported */
    uint16_t wMaxPacketSize;    /* max packet size supported */
    uint8_t bInterval;  /* polling interval in milliseconds */
} USB_ENDPOINT_t;

/**
 * \brief Structure for configuration of the out endpoints.
 */
typedef struct EPOUTDATA_s {
    uint8_t *bulk_out_buffer;
    uint32_t bulk_out_bytes_received;
    uint32_t bulk_out_bytes_received_index;
    uint32_t EventMask;
    void *ConHandle;
    os_mutex_t lock;
    uint32_t NotifyReceivedData;
    uint32_t NotifyReset;
} EPOUTDATA_t;
#define USB_READ_EVENT_MASK     0x0000FFFF
#define USB_CANCEL_EVENT_MASK   0xFFFF0000
#define USB_READ_EVENT_BITS        0x10001

/**
 * \brief Structure for an entry in the ep write list.
 */
typedef struct WRITE_MEMBER_s {
    char *StartBuffer;  /* The initial write buffer sent in. */
    char *CurrentBuffer;    /* The current buffer where to start the xfer. */
    int Length;     /* The remaining amount of data to send */
    uint32_t EndTime;   /* The timeout value for this write. When the timer function */
    /* timer_get_time_usec hits this value the write will be chucked. */
} WRITE_MEMBER_t;

/**
 * \brief Structure for the in endpoint structures.
 */
typedef struct EPINDATA_s {
    uint8_t Init;       /* 0 if empty, non 0 is being used. */
    uint8_t Active;     /* 0 if a write is not active, 1 if a write is active */
    LINK_HEADER WriteList;  /* the pending write list. */
    os_mutex_t lock;
    uint32_t hwZLP;     /* what the hw zlp is set for */
    uint8_t ZLP;        /* 0 if ZLP Disabled, 1 if ZLP Enabled for this endpoint */
} EPINDATA_t;

#define EP_IN_MAX_XFER_SIZE         0x4000  /* the max size of any xfer in the in direction */
#define INT_EP_PACKET_SIZE          8   /* Interrupt endpoint packet size */
#define CONTROL_MAX_PACKET_SIZE     64  /* control endpoint max packet size */
#define BULK_FS_MAX_PACKET_SIZE     64  /* usb1.1 max packet size */
#define BULK_HS_MAX_PACKET_SIZE     512 /* usb 2.0 max bulk packet size */
#define MAX_USB_RECV_PACKET         512 /* usb 2.0 max packet size. */
#define DEVICE_DESCRIPTOR_SIZE      18
#define NUM_BULKOUT_EP NUM_OUT_EPS
#define NUM_BULKIN_EP  NUM_IN_EPS
#define EVENT_DEVICE_INT 0x1
#define EVENT_WRITE_TO   0x2
#define BULK_BUF_XFER_SIZE      512

#define wmlog_e(_mod_name_, _fmt_, ...) \
	wmprintf("[%s]%s"_fmt_"\n\r", _mod_name_, " Error: ", ##__VA_ARGS__)

#define usb_e(...)	wmlog_e("usb", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
