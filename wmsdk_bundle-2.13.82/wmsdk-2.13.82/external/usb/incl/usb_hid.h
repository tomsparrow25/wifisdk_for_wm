/*
 *
 * ============================================================================
 * Portions Copyright (c) 2008-2014 Marvell International Ltd. All Rights Reserved.
 *
 *                         Marvell Confidential
 * ============================================================================
 */

#ifndef __USB_HID_H
#define __USB_HID_H

#ifdef __cplusplus
extern "C" {
#endif


#define WBVAL(x) (x & 0xFF),((x >> 8) & 0xFF)

/* HID class specification version */
#define HID_V1_10                               0x0110

/* HID device class code */
#define HID_DEVICE_CLASS                        0x00

/* HID device class subclass codes */
#define HID_DEVICE_SUBCLASS                     0x00


/* HID device class control protocol codes */
#define HID_PROTOCOL_COMMON_AT_COMMANDS         0x00

/* HID interface class code */
#define HID_INTERFACE_CLASS                     0x03

/* HID interface class subclass codes */
#define HID_DEVICE_SUBCLASS_NONEBOOT            0x00
#define HID_DEVICE_SUBCLASS_BOOT                0x01


/* HID interface class control protocol codes */
#define HID_PROTOCOL_NONE                       0x00
#define HID_PROTOCOL_MOUSE                      0x02
  
/* Data interface class code */
#define HID_DATA_INTERFACE_CLASS                0x0A

/* HID descriptor type */
#define HID_DESCRIPTOR_TYPE                     0x21
#define HID_REPORT_DESCRIPTOR_TYPE              0x22 

/*mouse REPORT DESCRIPTOR */



/* HID class-specific request codes */
#define HID_GET_REPORT_REQUEST                  0x01
#define HID_GET_IDLE_REQUEST                    0x02
#define HID_GET_PROTOCOL_REQUEST                0x03
#define HID_SET_REPORT_REQUEST                  0x09
#define HID_SET_IDLE_REQUEST                    0x0A
#define HID_SET_PROTOCOL                        0x0B


  
/* HID descriptor */
#pragma pack(1)
typedef struct _HID_DESCRIPTOR {
  
  uint8_t bLength;                             /* size of this descriptor in bytes */
  uint8_t bDescriptorType;                     /* HID descriptor type */
  uint16_t bcdHID;                             /* USB HID specification release version */
  uint8_t bCountrycode;                        /* Hardware target country */
  uint8_t bNumDescriptorType;                  /* Number of HID class descriptors to follow */
  uint8_t bReportDescriptorType;               /* Report descriptor type */
  uint16_t wDescriptorLength;                  /* Total length of Report decriptor*/
 }HID_DESCRIPTOR;

error_type_t usb_hiddevice_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_CDC_H */

