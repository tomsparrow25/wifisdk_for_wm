/**
 *
 * @file   aca.h
 * @brief  Arrayent Connect Agent (ACA) library interface.
 *
 * Copyright (c) 2013 Arrayent Inc.  Company confidential.
 * Please contact sales@arrayent.com to get permission to use in your application.
 *

@section Summary
The Arrayent Connect Agent (ACA) provides connectivity for devices to the Arrayent Connect Cloud.

@section Changelog
    @li 1.2.0.0: 2013/01/27
                 Add encryption support.
                 Change ACA threads' priorities to 7.
                 Automatically call ArrayentReset() after successful post-initialization call to
                 ArrayentConfigure().
                 Fix ArrayentConfigure() accepting input that could later be rejected by
                 ArrayentInit().
                 Fix main ACA thread not sleeping between loops on WICED.
    @li 1.1.2.0: 2013/01/09
                 Fix ArrayentRecvProperty() not setting its length argument to zero on receive
                 failure.
                 Add IAR ARM-compatible versions of the library to release packages.
    @li 1.1.1.0: 2013/12/16
                 Make ArrayentConfigure() take a monolithic configuration structure.
                 Null-terminate properties received using ArrayentRecvProperty().
                 Rename library interface file to aca.h.
    @li 1.1.0.1: 2013/11
                 First public beta release.

@section ACA Details
Your connected device firmware will be composed of two parts: an embedded client application and
the Arrayent Connect Agent. The embedded application refers to the embedded software that drives
your product. This part of the software is developed by your product engineering team. Your product
engineering team will integrate the Arrayent Connect Agent and the embedded client application.
This guide explains the functions available in the Arrayent Connect Agent Application Programming
Interface (the Arrayent API) for communicating with the Arrayent cloud.

Arrayent will supply the Arrayent Connect Agent and a board support package to abstract the Arrayent
Agent from the hardware-specific code. Therefore, it is important to identify the microcontroller
hardware and development toolchain early in the development process, so that Arrayent can ensure
that your hardware is supported. The Arrayent Connect Agent requires 32 to 64 KB of program
memory and approximately 15KB of RAM on a 32-bit embedded processor.

Arrayent Connect Agent's duties include:
* Logging in to the Arrayent Cloud and managing your device's session with the Arrayent cloud.
* Accepting messages from your
* Embedded Client Application and forwarding these messages into the
Arrayent cloud, and vice versa.

Messages are represented as key-value pairs. The key represents an attribute of your device, and
the value represents the current value of that attribute. For example, suppose that your device has
an embedded temperature sensor, which your device samples at a predefined interval (say every 10
minutes). To send this data into the Arrayent Cloud, your client application sends the following
message to the Arrayent agent: "temperature 84"

The Arrayent Endpoint simply forwards the message up into the cloud.
*/


#ifndef _ACA_H_
#define _ACA_H_

#include <stdint.h>


// Version of the code shipped with this header file.
#define ACA_VERSION_MAJOR       1   ///< Incremented after ten minor versions.
#define ACA_VERSION_MINOR       2   ///< Incremented on feature release.
#define ACA_VERSION_REVISION    0   ///< Incremented on bug fix.
#define ACA_VERSION_SUBREVISION 0   ///< Incremented internally.
extern const char* const aca_version_string;    ///< Global ASCII string containing ACA version.

/************************************************
 *                   CONSTANTS
 ***********************************************/
#define ARRAYENT_AES_KEY_LENGTH                 32 ///< Length of AES keys
#define ARRAYENT_NUM_LOAD_BALANCERS              3 ///< Number of load balancers
#define ARRAYENT_LOAD_BALANCER_HOSTNAME_LENGTH  36 ///< Maximum length of load balancer domain name
#define ARRAYENT_DEVICE_NAME_LENGTH             15 ///< Maximum length of device name
#define ARRAYENT_DEVICE_PASSWORD_LENGTH         10 ///< Maximum length of device password

/************************************************
 *              ARRAYENT DATA TYPES
 ***********************************************/
/** @brief All possible return values of ACA function calls.
 */
typedef enum {
    ARRAYENT_SUCCESS = 0,   ///< Command completed successfully.
    ARRAYENT_FAILURE = -1,  ///< Command failed.
    ARRAYENT_TIMEOUT = -2,  ///< Command failed: timed out.
    ARRAYENT_BUFFER_TOO_BIG = -3,   ///< Command failed; buffer too large.
    ARRAYENT_CANNOT_MULTI_PROPERTY = -4,    ///< Command failed; device is not configured for
                                            ///< receiving multi-property messages.
    ARRAYENT_ACCEPTED_CONFIG_BUT_CANNOT_RESET = -5, ///< Reconfiguration successful, but unable to
                                                    ///< reset afterward.
    ARRAYENT_BAD_PRODUCT_ID = -100,         ///< Product ID is invalid.
    ARRAYENT_BAD_PRODUCT_AES_KEY = -101,    ///< Product AES key is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_1 = -102,    ///< First load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_2 = -103,    ///< Second load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_3 = -104,    ///< Third load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_UDP_PORT = -105, ///< Load balancer UDP port is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_TCP_PORT = -106, ///< Load balancer TCP port is invalid.
    ARRAYENT_BAD_DEVICE_NAME = -107,    ///< Device name is invalid.
    ARRAYENT_BAD_DEVICE_PASSWORD = -108,    ///< Device password is invalid.
    ARRAYENT_BAD_DEVICE_AES_KEY = -109  ///< Device AES key is invalid.
} arrayent_return_e;


/** @brief Structure to use with @c ArrayentConfigure() to configure the ACA.
 */
typedef struct {
    uint16_t    product_id;
    const char* product_aes_key;
    const char* load_balancer_domain_names[3];
    uint16_t    load_balancer_udp_port;
    uint16_t    load_balancer_tcp_port;
    const char* device_name;
    const char* device_password;
    const char* device_aes_key;
    uint8_t     device_can_multi_attribute:1;   ///< Indicates that the device is capable of
                                                ///< receiving multi-attribute messages. Setting
                                                ///< this to 1 will cause the server to encapsulate
                                                ///< all attributes sent to this device in multi-
                                                ///< attribute messages.
                                                ///< @warn Currently unused.
} arrayent_config_t;


/** @brief Arrayent sleep levels - use these with @c ArrayentSleep()
 */
typedef enum {
    ARRAYENT_NAP = 0,
    ARRAYENT_SLEEP,
    ARRAYENT_HIBERNATE
} arrayent_sleep_level_e;

/** @brief Arrayent network status - returned by @c ArrayentNetStatus()
 */
typedef struct {
    uint8_t server_ip_obtained:1;   ///< Arrayent server IP has been found
    uint8_t heartbeats_ok:1;        ///< Heartbeats to the Arrayent server are fine
    uint8_t using_udp:1;            ///< Connection to Arrayent server is over UDP
    uint8_t using_tcp:1;            ///< Connection to Arrayent server is over TCP
    uint8_t connected_to_server:1;  ///< Connection to Arrayent server is okay
    uint16_t padding:11;
} arrayent_net_status_t;

/** @brief Arrayent timestamp data type (output of @c ArrayentGetTime())
 */
typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
} arrayent_timestamp_t;



/************************************************
 *              ARRAYENT FUNCTIONS
 ***********************************************/
/** Configure the Arrayent Connect Agent.
 *
 * Use this function to set all of your device's Arrayent attributes before starting the ACA.
 *
 * If your application does not successfully call @c ArrayentConfigure() for each Arrayent property
 * before calling @c ArrayentInit(), the Arrayent thread may hang.
 *
 * If this function is called after @c ArrayentInit(), it will automatically call @c ArrayentReset()
 * if the reconfiguration structure was accepted. In the unlikely case that reconfiguration is
 * successful but resetting is not, the host application must retry resetting the ACA until it
 * succeeds before attempting any communication through the ACA.
 *
 * @param[in] config    Structure fully populated with Arrayent configuration arguments.
 *
 * @return          Indicates whether or not the configuration was accepted.
 */
arrayent_return_e ArrayentConfigure(arrayent_config_t* config);


/** Start the ACA.
 *
 * This function prepares the ACA for communication with the Arrayent server.
 * Do not call this function until every Arrayent property has been configured successfully using
 * @c ArrayentConfigure(). Doing so may cause the Arrayent thread to hang.
 *
 * Note that before calling this function, the Arrayent code expects the user application to have
 * seeded the machine's random number generator using srand().
 *
 * @return    Indicates if initialization was completed successfully.
 */
arrayent_return_e ArrayentInit(void);


/** Check connection to the ACA.
 *
 * This function sends a hello message to the ACA. If the ACA interface is functioning correctly,
 * the hello message will receive a response message from the routing daemon to indicate a working
 * connection.
 *
 * @return    Indicates if the hello was verified.
 */
arrayent_return_e ArrayentHello(void);


/** Check Arrayent network status.
 *
 * This function returns a bit field indicating the network status of the ACA.
 *
 * @param[out]  status  Pointer to the status structure to be updated. See @c arrayent_net_status_t.
 *
 * @return    Indicates if the status was updated.
 */
arrayent_return_e ArrayentNetStatus(arrayent_net_status_t* status);


/** Put the ACA to sleep.
 * @warn THIS FUNCTION IS NOT YET IMPLEMENTED
 *
 * This function instructs the ACA to go to sleep until ArrayentWake() is called.
 *
 * Do not call any ACA functions other than @c ArrayentWake() while ACA is sleeping.
 *
 * @param[in]   sleep_level   Sets the exact sleep behavior.
 *      ARRAYENT_NAP: Arrayent threads are paused, and dynamic networking resources are torn down.
 *                    On wake, networking resources are recreated and the Arrayent state machines
 *                    resume with whatever state they were in prior to being put to sleep.
 *                    Use NAP when your application expects to sleep for less than a minute or so.
 *                    If you aren't sure how long your application will sleep for, use NAP.
 *                    Waking up from this sleep level requires the least amount of IP traffic
 *                    (and therefore the least amount of power) of all.
 *      ARRAYENT_SLEEP: Same as NAP, but use this when your application expects to sleep for more
 *                      than a minute. Waking up from this sleep level requires slightly more IP
 *                      traffic than NAP, therefore slightly more power.
 *      ARRAYENT_HIBERNATE: All dynamic Arrayent resources are torn down, including threads.
 *                          On wake, threads are recreated and everything starts fresh. Waking from
 *                          this sleep levelrequires the most IP traffic, and therefore power, of
 *                          all.
 *
 * @return  Indicates whether or not the ACA was successfully suspended.
 */
arrayent_return_e ArrayentSleep(arrayent_sleep_level_e sleep_level);


/** Wake up the ACA.
 * @warn THIS FUNCTION IS NOT YET IMPLEMENTED
 *
 * This function brings the routing daemon out of sleep, causing it to reestablish its
 * connection with the Arrayent server. Exact behavior depends on what sleep_level argument was
 * passed to ArrayentSleep().
 *
 * Applications should poll ArrayentNetStatus() for success after this function has been called and
 * before sending property updates to the cloud.
 *
 * @return  Indicates whether or not the ACA is now in the process of resuming.
 */
arrayent_return_e ArrayentWake(void);


/** Reset the ACA.
 *
 * This function forces the ACA's state machine to reset and log back in to the server. It may be
 * called at any time after initialization is complete.
 *
 * Be sure to poll @c ArrayentNetStatus() for server connection before attempting Arrayent
 * communication after a reset.
 *
 * @return    Indicates if the reset was accepted.
 */
arrayent_return_e ArrayentReset(void);


/** Retrieve the current time from the server.
 *
 * This function blocks for up to three seconds waiting for a response from the server.
 *
 * @param[out]  timestamp   Pointer to structure to fill with current time
 *
 * @param[in]   timezone    GMT offset of desired time, in hours
 *
 * @return          Indicates if the timestamp was received. If not ARRAYENT_SUCCESS, *timestamp
 *                  does NOT contain a valid timestamp.
 */
arrayent_return_e ArrayentGetTime(arrayent_timestamp_t* timestamp, int16_t timezone);


/** Write a new property to the server.
 *
 * This function sets an arbitrary property on the Arrayent servers.
 *
 * @param[in]   property  Pointer to the property name.
 *
 * @param[in]   value     Pointer to the property value.
 *
 * @return          Indicates if the property was set correctly.
 */
arrayent_return_e ArrayentSetProperty(char* property, char* value);


/** Receive a property message.
 *
 * This function listens for a property message from the Arrayent cloud. It is blocking; it
 * will not return until either a property message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @param[out]      data    Where to place the received property message. This buffer is not touched
 *                          if no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 one character longer than the longest expected message, to allow
 *                                 for null-termination.
 *                          Output: if a property message was received, this number is set to the
 *                                  length of the data written to the data buffer. In the event of
 *                                  an insufficiently sized receive buffer, the received message is
 *                                  truncated to the length of the passed buffer. The value of len
 *                                  does not include the null terminator, which is always set by
 *                                  this function.
 *                                  If no property message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a property was received from the server.
 */
arrayent_return_e ArrayentRecvProperty(char* data, uint16_t* len, uint32_t timeout);


/** Write multiple attributes to the server simultaneously.
 * @warn THIS FUNCTION IS NOT YET IMPLEMENTED
 *
 * This function sends a multi-attribute message to the Arrayent servers.
 *
 * @param[in]   data    Property buffer. See documentation on Arrayent multi-attribute message
 *                      format for description of contents.
 * @param[in]   len     Length of property buffer. Currently cannot exceed 128 bytes.
 *
 * @return    Indicates if the attributes were sent to and accepted by the server.
 */
arrayent_return_e ArrayentSetMultiAttribute(uint8_t* data, uint16_t len);


/** Receive a multi-attribute message.
 * @warn THIS FUNCTION IS NOT YET IMPLEMENTED
 *
 * This function listens for a multi-attribute message from the Arrayent cloud. It is blocking; it
 * will not return until either a multi-attribute message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @warn In order to use this call, you must have specified that your device is capable of
 * receiving multi-attribute messages in your call to @c ArrayentConfigure().
 *
 * @param[out]      data    Where to place the received multi-attribute message. This buffer is
 *                          not touched if no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 as long as the longest expected message.
 *                          Output: if a multi-attribute message was received, this number is set to
 *                                  the length of the data written to the data buffer. In the event
 *                                  of an insufficiently sized receive buffer, this value will
 *                                  equal the size of the buffer, and the received message is
 *                                  truncated to the size of the buffer.
 *                                  If no property message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a multi-attribute message was received from the server.
 */
arrayent_return_e ArrayentRecvMultiAttribute(uint8_t* data, uint16_t* len, uint32_t timeout);


/** Send binary data with optional timeout.
 * @warn this function is maintained for legacy applications only.
 *
 * This function sends data messages to the Arrayent servers synchronously. It is blocking, and
 * will not return until a response message has been received from the server or the timeout value
 * expires.
 *
 * @param[in]   data    Pointer to the data for transmission.
 *
 * @param[in]   len     Length of data to send
 *
 * @param[in]   timeout Timeout for transmission to complete. This value is specified in
 *                  milliseconds.
 *
 * @return    Indicates if the data was sent to the server correctly.
 */
arrayent_return_e ArrayentSendData(char* data, uint16_t len, uint32_t timeout);


/** Receive a raw data message.
 * @warn this function is maintained for legacy applications only.
 *
 * This function attempts to receive a data message from the Arrayent cloud. It is blocking; it
 * will not return until either a data message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @param[out]      data    Where to place the received data message. This buffer is not touched if
 *                          no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 the length of the longest expected message.
 *                          Output: if a data message was received, this number is set to the
 *                                  length of the data written to the data buffer. In the event of
 *                                  an insufficiently sized receive buffer, this value will not
 *                                  equal the length of the received data.
 *                                  If no data message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a data message was received from the server.
 */
arrayent_return_e ArrayentRecvData(char* data, uint16_t* len, uint32_t timeout);

#endif /* _ACA_H_ */
