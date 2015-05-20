#ifndef USB_API_H
#define USB_API_H

/*
 * ============================================================================
 * Copyright (c) 2008-2014   Marvell International, Ltd. All Rights Reserved
 *
 *                         Marvell Confidential
 * ============================================================================
 */

/**
 * \brief API for use of the usb devices.
 * To initialize the interface do the following.
 *
 * 1.  Call usb_dev_init to set up the driver. This should only
 *     be called one time.
 * 2.  Call usb_dev_set_config
 * 3.  Call usb_dev_intf_add for each interface you wish to
 *     define. The control interface does not get a call to
 *     init. It is done by default. The order of these calls is
 *     the order that the interfaces are reported to the host.
 * 4.  Call usb_dev_start to allow the usb to enumerate and run.
 *
 * Note that the string pointers that are passed are not copied
 * by the driver. You will have to keep the strings around. If
 * you do an open on interface 0 you open the control endpoint.
 *
 * To reconfigure the interfaces:
 *
 * 1.  Call usb_dev_stop
 * 2.  Call usb_dev_get_config. Optional.
 * 3.  Call usb_dev_set_config. This is
 *     only required if the configuration structure
 *     (usb_sys_config_t) is changed.
 * 4.  Call usb_dev_intf_remove_all to remove all of the
 *     interfaces.
 * 5.  Call usb_dev_intf_add for each interface
 *     you wish to define.
 * 6.  Call usb_dev_start to allow the usb to enumerate and run.
 *
 * To re-enumerate on the USB bus:
 *
 * 1.  Call usb_dev_stop
 * 2.  Call usb_dev_start
 *
 * To change the USB configuration (VID, PID, Language,
 * Manufacturer string, Product String, Formatter Serial number,
 * and USB speed):
 *
 * 1.  Call usb_dev_stop
 * 2.  Call usb_dev_get_config
 * 2.  Call usb_dev_set_config with the updated configuration
 *     structure.
 * 3.  Call usb_dev_start
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

#include <usb_port.h>

#define USB_DEVICE_ZERO_TERMINATE           (0x0)	/* Zero terminate the endpoint */
#define USB_DEVICE_DONT_ZERO_TERMINATE      (0x1)	/* Don't zero terminate the endpoints.*/

#define USB_DEVICE_DONT_ZLP                 (0x0)	/* Disable ZLP */
#define USB_DEVICE_BULK_IN_ZLP              (0x1)	/* Bulk In ZLP */
#define USB_DEVICE_INT_IN_ZLP               (0x2)	/* Interrupt In ZLP */

#define CONIO_TIMEOUT               (-1)
#define CONIO_CONERR                (-2)

/* Unique Interface ID
 expand to 32 bits since this is used to check for a valid handle. */
	typedef enum {
		USB_CDC_INTERFACE,
		USB_HID_INTERFACE,
		USB_MSC_INTERFACE,
		USB_HTTP_INTERFACE,
		LAST_ONE = 0xffffffff
	} usb_intf_id_t;

/* XXX This was defined in usb_device_api.h */
typedef enum {
    e_CONTROL = 0,
    e_BULK = 2,
    e_INTERRUPT = 3
} EP_TYPE;


/**
 * \brief USB2 config structure for init of the system.
 */
	typedef struct {
		uint16_t USBVendorID;	/* The usb vendor id for the system */
		uint16_t USBProductID;	/* The usb product id for the system. */
		uint16_t USBLang;	/* The usb lang id for the system. */
		char *USBMfgStr;	/* pointer to the usb mfg string. */
		char *USBProdStr;	/* Pointer to the usb product string. */
		char *USBFormatterSerNum;	/* Pointer to the usb formatter serial number. */
		bool USBForceFullSpeed;	/*  USB Force Full Speed flag */
	} usb_sys_config_t;

/**
 * \brief This is the prototype for the USB interface call back function.  The
 * driver will utilize the callback to handle USB class and vendor specific
 * requests received on the control endpoint.
 */
	typedef bool(*usb_intf_callback_t) (void *usb_handle, void *setup_pkt,
					    uint8_t state);

/**
 * \brief This structure specifies the interface configuration.
 */

	/* haitao 03/15/12 */
	typedef enum {
		e_CDC,
		e_HID,
		e_MassStorage,	// Doesn't use CM
		e_HTTP,		// Doesn't use CM
	} Channel_type_t;

	typedef struct {
		uint8_t NumOuts;	/* The number of out endpoints. Only supports 0 or 1 */
		uint8_t NumIns;	/* The number of in endpoints. Only supports 0 or 1 */
		uint8_t NumInts;	/* The number of interrupt endpoints. Only supports 0 or 1. */
		EP_TYPE   EPType;         ///< Type of endpoints. /* bob 08/27/12 */
		uint8_t ClassCode;	/* The usb defined class code for this interface */
		uint8_t SubClass;	/* The usb defined sub class code for this interface */
		uint8_t Protocol;	/* The usb defined protocol for this interface. */
		Channel_type_t Function;	/* The type of interface */
		uint8_t *ClassDesc;	/* Interface class description */
		uint32_t ClassDescSize;	/* Interface class description size */
		uint8_t *InterfAssocDes;	/* Standard interface association descriptor */
		char *InterfAssocName;	/* String naming the interface association, reported to the host. */
		char *InterfaceName;	/* String naming the interface, reported to the host. */
		usb_intf_callback_t InterfaceCallback;	/* pointer to the interface callback */
		uint32_t ZeroTermination;	/* 0 do not use zero length packets to terminate xfers, 1 do. */
		uint8_t ZLP;	/* 0 - disabled, 1 - Bulk In enabled, 2 - Interrupt In Enabled, 3 - Bulk/Int In enabled */
	} usb_intf_config_t;

/** ioctl commands. */
typedef enum 
{
    e_close = 1,  ///< Upper level close, io error or timeout will cause this, pipeconnect required for next read.

    /// The following are all used by usb mass storage device.
    e_STALLIN,          ///< Stall the in endpoint.
    e_STALLOUT,         ///< Stall the out endpoint.

    /// the following are currently not used or implemented.
    e_StatusReport,
    e_WaitForWrites,
    e_ResetWrites,       ///< Wipe out all pending writes and abort current write.
    e_ClearAllWrites,     ///< Get rid of all writes for all endpoints.
    e_TransferActive    ///< Returns a 1 if something has not xfered since last time we called.

} ioctl_cmd_t ;

/** 
 * Opaque structure pointer for the lower level device to cast into its
 * internal data type.
 */
typedef struct io_device_impl_s io_device_impl_t;

/**
 * \brief Initialize the usb interface
 *
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_init(void);

/**
 * \brief Add the specified interface. This identifies an
 * interface to the usb driver.  The order that these calls are
 * made is the order that they will be reported to the host.
 * The configuration of the interface is stored in the driver in
 * an interface config table. Do NOT use this call in an attempt
 * to init the control endpoint.  It is done by default.
 * \param[in] interfaceID The Interface that is to be initialized.
 * \param[in] interfaceConfig This defines what the interface looks like.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_intf_add(usb_intf_id_t interfaceID,
			usb_intf_config_t *interfaceConfig);

/**
 * \brief Remove all interfaces.  Stop the USB device with
 * usb_dev_stop before calling this function.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_intf_remove_all(void);

/**
 * \brief Turn on the usb device and allow it to enumerate.
 * This will turn on the D+ pullup resistor.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_start(void); 

/**
 * \brief Turn off the usb device and drop off the bus.  This
 * will turn off the D+ pullup resistor.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_stop(void);

/**
 * \brief Return the current usb device configuration
 * \param config Pointer to the structure to fill out.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_get_config(usb_sys_config_t *config);

/**
 * \brief Set the usb config to the values in config
 * To use this you first must stop the interface, using usb_dev_stop, then do the
 * usb_dev_set_config, and then start the interface using usb_dev_start.
 * \param[in] config Pointer to the structure containing the data to update.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_set_config(const usb_sys_config_t *config);

/**
 * \brief Open a given interface in usb.
 * \param[out] *handle The handle returned after the open.
 * \param[in] interfaceID The interface ID to open.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_intf_open(io_device_impl_t **handle, usb_intf_id_t interfaceID);

/**
 * \brief Close a previously opened interface.
 * \param[in] handle The handle returned from the open call.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_intf_close(io_device_impl_t *handle);

/**
 * \brief Read from a given endpoint.
 * This function attempts to read data into the buffer of len amount within timeout period.
 * \param[in] handle The handle returned from an open command.
 * \param[in] pBuffer The address of the buffer to fill.
 * \param[in] len The number of bytes max to read into the buffer.
 * \param[in] timeout The number of clock ticks to wait for len
 * amount of data to arrive. If 0 then return only the data that
 * currently exists in local buffers.
 * \return number of bytes actually transferred, or a CONIO_XXX negative error code.
 */
	error_type_t usb_dev_intf_read(io_device_impl_t* handle, void *pBuffer, uint32_t len, uint32_t timeout);

/**
 * \brief Write a data buffer to the interface pointed to by handle.  The buffer must be 
 * allocated since the driver will free the buffer when the write completes on the USB Bus.
 * \param[in] handle The handle returned from an open call.
 * \param[in] pBuffer The data buffer from which data is written. This buffer must be allocated 
 * and not used after this call.  The driver will "free" the buffer when the data is transferred
 * to the usb host.
 * \param[in] len The number of bytes in the buffer to write.
 * \param[in] timeout Currently ignored.
 * \return  number of bytes actually transferred, or a CONIO_XXX negative error code.
 */
	error_type_t usb_dev_intf_write(io_device_impl_t *handle, void *pBuffer, uint32_t len, uint32_t timeout);

/**
 * \brief Write a data buffer to the interrupt interface pointed to by handle.  The buffer must
 * be allocated since the driver will free the buffer when the write completes on the USB Bus.
 * \param[in] handle The handle returned from an open call.
 * \param[in] pBuffer The data buffer from which data is written. This buffer must be allocated 
 * and not used after this call.  The driver will "free" the buffer when the data is transferred
 * to the usb host.
 * \param[in] len The number of bytes in the buffer to write.
 * \param[in] timeout Not implemented.
 * \return  number of bytes actually transferred, or a CONIO_XXX negative error code.
 */
	error_type_t usb_dev_intf_interrupt_write(io_device_impl_t *handle, void *pBuffer, uint32_t len, uint32_t timeout);

/**
 * \brief Interface control functionality for the usb interface.  The ioctl commands
 * are defined in "io_device.h".
 * \param[in] handle The handle returned from an open call.
 * \param[in] cmd The command to execute
 * \param[in] ptrArg A pointer that is specific to a cmd.
 * \return error_type_t - OK 0 or FAIL -1
 */
	error_type_t usb_dev_intf_ioctl(io_device_impl_t *handle, ioctl_cmd_t cmd, void *ptrArg);

/**
 * \brief read usb data to appointed buffer.
 * \param[out] *pBuffer buffer to be read.
 * \param[in] len read length.
 * \param[out] timeout timeout value.
 * \param[in] EPNum endpoint number to be read.
 * \return read length or CONIO_TIMEOUT or CONIO_CONERR
 */
	int32_t usb2Read(void *pBuffer, uint32_t len, uint32_t timeout,
			 uint32_t EPNum);

/**
 * \brief read usb data to appointed buffer.
 * \param[out] *pBuffer buffer to be write.
 * \param[in] len write length.
 * \param[out] timeout timeout value.
 * \param[in] EPNum endpoint number to be read.
 * \return written data length or CONIO_CONERR
 */
	int32_t usb2Write(void *pBuffer, uint32_t len, uint32_t timeout,
			  uint32_t endpointNum);

/**
 * \brief a function to test whether the device is connected.
 * 
 * \returns - true if connected, false if not.
 */
bool is_USB_device_connected();

/**
 * \brief check whether mass storage data has been sent.
 * 
 * \returns true or false.
 */
bool check_MS_data_sent();

#ifdef __cplusplus
}
#endif
#endif
