/*! \file mdns.h
 * \brief The mDNS/DNS-SD Responder-Querier
 *
 * mDNS (Multicast DNS) and DNS-SD (DNS based Service Discovery) together
 * enable Service Discovery in Zero configuration networks by using the
 * standard DNS protocol packets but with minor changes.
 *
 *  \section  mdns_usage Example Usage:
 *
 *  The following code announces the service named 
 *  wm_demo._smartenergy._tcp.local
 *  with the given TXT records' key-value pairs and monitors for the services of type 
 *  "_smartenergy._tcp.local".
 *  Any state change to the monitored service is reported through a callback 
 *  function wm_demo_mdns_query_cb.
 *
 *
 * \code
 * #include <mdns.h>
 *
 * static int wm_demo_mdns_query_cb(void *data, const struct mdns_service *s, int status)
 * {
 *        wmprintf("Got callback for %s\r\n", s->servname);
 *
 *        if (status == MDNS_DISCOVERED)
 *        	wmprintf("DISCOVERED:\r\n");
 *        else if (status == MDNS_CACHE_FULL)
 *              wmprintf("NOT_CACHED:\r\n");
 *        else if (status == MDNS_DISAPPEARED)
 *             	wmprintf("DISAPPEARED:\r\n");
 *        else if (status == MDNS_UPDATED)
 *             	wmprintf("UPDATED:\r\n");
 *        else {
 *              wmprintf("Warning: unknown status %d\r\n", status);
 *              return WM_SUCCESS;
 *        }
 *        wmprintf("%s._%s._%s.%s.\r\n",s->servname, s->servtype,
 *                        s->proto == MDNS_PROTO_UDP ? "udp" : "tcp",
 *                        s->domain);
 *        wmprintf("at %d.%d.%d.%d:%d\r\n",
 *                        (s->ipaddr >> 24) & 0xff, (s->ipaddr >> 16) & 0xff,
 *                        (s->ipaddr >> 8) & 0xff, s->ipaddr & 0xff, htons(s->port));
 *        wmprintf("TXT : %s\r\n", s->keyvals ? s->keyvals : "no key vals");
 *
 *        return WM_SUCCESS;
 * }

 * #define MAX_SERVICES 1
 * struct mdns_service *services[MAX_SERVICES +1];
 * struct mdns_service myservices;
 * char service_name[]="wm_demo";
 * char serv_type[]="smartenergy";
 * char keyvals[]="txtvers=2:path=/wm_demo:description=Wireless Microcontroller:Product=WM1";
 * char domain[]="local";
 * char dname[]="myhost";
 * char fqst[]="_smartenergy._tcp.local";
 * int wm_mdns_start()
 * {
 *        int ret = 0;
 *        struct wlan_ip_config addr;
 *        struct mdns_service_config config;
 *        wlan_get_address(&addr);
 *        memset(services,0,sizeof(services));
 *        memset(&myservices,0,sizeof(myservices));
 *        myservices.proto = MDNS_PROTO_TCP;
 *        myservices.servname = service_name;
 *        myservices.servtype = serv_type;
 *        myservices.keyvals = keyvals;
 *        myservices.port = 8888;
 *        myservices.domain = domain;
 *        services[0] = &myservices;
 *        handle = net_get_sta_handle();
 *        config.iface_handle = handle;
 *        config.service = services;
 *        ret = mdns_start(domain, dname);
 *        mdns_add_service_iface(config);
 *        mdns_iface_state_change(handle, UP);
 *        mdns_query_monitor(fqst, wm_demo_mdns_query_cb, NULL);
 *        return ret;
 * }
 *
 * int wm_mdns_stop()
 * {
 *        mdns_stop();
 *        return 0;
 * }
 * \endcode
 *
 *  \section mdns_subtype_support  mDNS subtype base querier support
 * WMSDK mDNS querier also supports subtype base discovery. To browse for
 * a subtype base query, "fqst" should be in following format:
 * \<subtype_name\>._sub._\<user_protocol\>._\<tcp or udp\>.\<domain\>
 *
 * Below is the code snippet where querier searches for "_marvell" subtype
 * based services:
 * \code
 * char fqst[]="_marvell._sub._http._tcp.local";
 *
 * static int wm_demo_mdns_query_cb(void *data, const struct mdns_service *s, int status)
 * {
 *        wmprintf("Got callback for %s\r\n", s->servname);
 *        return WM_SUCCESS;
 * }
 *
 * int wm_mdns_start()
 * {
 *        \/ * Add calls to start mdns, add service and state change * \/
 *        \/ * Register service monitor * \/
 *        mdns_query_monitor(fqst, wm_demo_mdns_query_cb, NULL);
 * }
 * \endcode
 *
 * Following command publishes subtype based service using avahi browse:
 *
 * avahi-publish -s --domain="local" --subtype="_marvell._sub._http._tcp"
 *                                            "MarvellService" "_http._tcp" 80
 *
 * <h2>Service Discovery with Linux</h2>
 *
 * The services published in the way described above can be discovered from a 
 * Linux machine connected in the same network using avahi-tools.
 * For example, if all service instances belonging to a particular type of service
 * in the network need to be discovered.
 *
 * Following command looks for service 
 * instances of the type "_smartenergy._tcp" in the network. The second line 
 * shows the output for the command, which discovered the service instance 
 * published with the given service type.
 * \code
 * avahi-browse _smartenergy._tcp
 * + wlan0 IPv4 wm_demo                                          _smartenergy._tcp    local
 * \endcode
 *
 * <h2>Build Options</h2>
 * A number of build options are available in order to optimize footprint,
 * specify the target system, etc. These are conventionally provided as config
 * options as opposed to being defined directly in a header file. They
 * include:
 *
 * <b>CONFIG_MDNS_QUERY</b>: Developers who require the ability to query for services as
 * opposed to just responding to such queries should define MDNS_QUERY.
 * This enables the mdns_query_* functions described below.  These functions
 * will return -WM_E_MDNS_NOIMPL if the mdns library was built without -WM_E_MDNS_QUERY
 * defined.
 * By default this option is enabled.
 *
 * <b>CONFIG_MDNS_CHECK_ARGS</b>: Define this option to enable various error checking on the input.
 * Developers may wish to enable this during development to ensure that various
 * inputs such as host names are legal.  Then, if the inputs are not going to
 * change, this option can be turned off to save code space.
 * By default this option is disabled.
 *
 * CONFIG_MDNS_DEBUG: Define this option to include debugging and logging.
 * By default this option is disabled.
 *
*/



/*
 * Copyright (C) 2007-2011 cozybit Inc.
 *
 * This file is part of libmdns.
 *
 * libmdns is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libmdns is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libmdns. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MDNS_H
#define MDNS_H

#include <mdns_port.h>
#include <wmerrno.h>
/* mdns control socket ports
 *
 * mdns uses two control sockets to communicate between the mdns threads and
 * any API calls.  This control socket is actually a UDP socket on the loopback
 * interface.  Developers who wish to specify certain ports for this control
 * socket can do so by changing MDNS_CTRL_RESPONDER and MDNS_CTRL_QUERIER.
 */
#ifndef MDNS_CTRL_RESPONDER
#define MDNS_CTRL_RESPONDER 12345
#endif

#ifndef MDNS_CTRL_QUERIER
#define MDNS_CTRL_QUERIER  (MDNS_CTRL_RESPONDER + 1)
#endif

/** This is the maximum number of service instances
 * that can be monitored. See the section below on "mdns
 * query callback" for details on how to tune this value, and field
 * implications of the value of this value.
 */
#ifndef MDNS_SERVICE_CACHE_SIZE
#define MDNS_SERVICE_CACHE_SIZE 2
#endif
/** This is the maximum number of service types that
 * the system can monitor.  If an application only needs to monitor one type of
 * service, this should be 1 to save memory.
 */
#ifndef MDNS_MAX_SERVICE_MONITORS
#define MDNS_MAX_SERVICE_MONITORS 1
#endif

/** Maximum length of labels
 *
 * A label is one segment of a DNS name.  For example, "foo" is a label in the
 * name "foo.local.".  RFC 1035 requires that labels do not exceed 63 bytes.
 */
#define MDNS_MAX_LABEL_LEN	63	/* defined by the standard */

/** Maximum length of names
 *
 * A name is a list of labels such as "My Webserver.foo.local" or
 * mydevice.local.  RFC 1035 requires that names do not exceed 255 bytes.
 */
#define MDNS_MAX_NAME_LEN	255	/* defined by the standard : 255*/

/** Maximum length of key/value pair
 *
 * TXT records associated with a service are populated with key/value pairs.
 * These key/value pairs must not exceed this length.
 */
#define MDNS_MAX_KEYVAL_LEN	255	/* defined by the standard : 255*/

/** mDNS Interface State */
enum iface_state {
	/** Interface is UP and announce services for first time.
	 * mDNS announces services by checking service name conflicts
	 * and resolves them by appending a number to service name
	 * to make it unique.
	 */
	UP = 0,
	/** Interface is DOWN and de-announce services
	 * mDNS sends good bye packet with ttl=0 so that mDNS clients can remove
	 * the services from their mDNS cache table.
	 */
	DOWN,
	/** Forcefully announce services
	 * This state should be used after UP state i.e. services are already
	 * announced and force announcement is needed due to some reason.
	 */
	REANNOUNCE
};

/** MDNS Error Codes */
enum wm_mdns_errno {
	WM_E_MDNS_ERRNO_START = MOD_ERROR_START(MOD_MDNS),
	/** invalid argument*/
	WM_E_MDNS_INVAL,
	/** bad service descriptor*/
	WM_E_MDNS_BADSRC,
	/** not enough room for everything*/
	WM_E_MDNS_TOOBIG,
	/** unimplemented feature*/
	WM_E_MDNS_NOIMPL,
	/** insufficient memory*/
	WM_E_MDNS_NOMEM,
	/** requested resource is in use*/
	WM_E_MDNS_INUSE,
	/** requested resource is in use*/
	WM_E_MDNS_NORESP,
	/** failed to create socket for mdns*/
	WM_E_MDNS_FSOC,
	/** failed to reuse multicast socket*/
	WM_E_MDNS_FREUSE,
	/** failed to bind mdns socket to device*/
	WM_E_MDNS_FBINDTODEVICE,
	/** failed to bind mdns socket*/
	WM_E_MDNS_FBIND,
	/** failed to join multicast socket*/
	WM_E_MDNS_FMCAST_JOIN,
	/** failed to set multicast socket*/
	WM_E_MDNS_FMCAST_SET,
	/** failed to create query socket*/
	WM_E_MDNS_FQUERY_SOC,
	/** failed to create mdns thread*/
	WM_E_MDNS_FQUERY_THREAD,
};
/** service descriptor
 *
 * Central to mdns is the notion of a service.  Hosts advertise service types
 * such as a website, a printer, some custom service, etc.  Network users can
 * use an mdns browser to discover services on the network.  Internally, this
 * mdns implementation uses the following struct to describe a service.  These
 * structs can be created by a user, populated, and passed to mdns_start to
 * specify services that are to be advertised.  When a user starts a query for
 * services, the discovered services are passed back to the user in this
 * struct.
 *
 * The members include:
 *
 * servname: string that is the service instance name that will be advertised.
 * It could be something like "Brian's Website" or "Special Service on Device
 * #123".  This is the name that is typically presented to users browsing for
 * your service.  The servname must not exceed MDNS_MAX_LABEL_LEN bytes.  The
 * MDNS specification allows servname to be a UTF8 string.  However, only the
 * ascii subset of UTF-8 has been tested.
 *
 * servtype: string that represents the service type.  This should be a type
 * registered at http://dns-sd.org/ServiceTypes.html.  For example, "http" is
 * the service type for a web server and "ssh" is for an ssh server.  You may
 * use an unregisterd service type during development, but not in released
 * products.  Consider registering any new service types at the aforementioned
 * webpage.  servtype must be non-NULL.
 *
 * domain: string that represents the domain.  This field is ignored by the
 * responder (i.e., in those mdns_services passed to mdns_start).  In this
 * case, the domain passed to mdns_start is used.  However, the domain is
 * valid for mdns_services that are passed to query callbacks.
 *
 * port: the tcp or udp port on which the service named servname is available
 * in network byte order.
 *
 * proto: Either MDNS_PROTO_TCP or MDNS_PROTO_UDP depending on what protocol
 * clients should use to connect to the service servtype.
 *
 * keyvals: NULL-terminated string of colon-separated key=value pairs.  These
 * are the key/value pairs for the TXT record associated with a service type.
 * For example, the servtype "http" defines the TXT keys "u", "p", and "path"
 * for the username, password, and path to a document.  If you supplied all of
 * these, the keyvals string would be:
 *
 * "u=myusername:p=mypassword:path=/index.html"
 *
 * If keyvals is NULL, no TXT record will be advertised.  If keyvals is ":", a
 * TXT record will appear, but it will not contain any key/value pairs.  The
 * key must be present (i.e., two contiguous ':' characters should never appear
 * in the keyvals string.)  A key may appear with no value.  The interpretation
 * of this depends on the nature of the service.  The length of a single
 * key/value pair cannot exceed MDNS_MAX_KEYVAL_LEN bytes.
 * It is the responsibility of the application to verify that the keyval string
 * is a valid string. The keyval string buffer is used by the mDNS module
 * internally and it can modify it. Hence, during subsequent calls to the mDNS
 * module, it is possible that the original string has been messed up and needs
 * to be recreated.
 *
 * Note that the keyvals string of any struct mdns_services passed to
 * mdns_start WILL be modified by mdns, and therefore must not be const (e.g.,
 * in ROM).  Further, the keyvals member should not be dereferenced after being
 * passed to mdns_start.  On the contrary, struct mdns_services generated in
 * response to queries may be dereferenced and will have the afore-described
 * syntax.
 *
 * ipaddr: The IP address on which the service is available in network byte
 * order.  Note that this member need not be populated for services passed to
 * mdns_start.  It is implied that these services are offered at the ip
 * address supplied as an explicit argument to the mdns_start call.  However,
 * when a struct mdns_service is generated in response to a query, this member
 * will be populated.
 *
 * fqsn, ptrname, kvlen and flags are for internal use only and should not be
 * dereferenced by the user.
 */
struct mdns_service
{
	/** Name of MDNS service  */
	const char *servname;
	/** Type of MDNS service */
	const char *servtype;
	/** Domain for MDNS service */
	const char *domain;
	/** Port number  */
	uint16_t port;
	/** Protocol used */
	int proto;
	/** Key value pairs for TXT records*/
	char *keyvals;
	/** IP Address of device */
	uint32_t ipaddr;

	/** The following members are for internal use only and should not be
	 * dereferenced by the user.
	 */
	uint8_t fqsn[MDNS_MAX_NAME_LEN];
	/** PTR record name */
	uint8_t *ptrname;
	/** Length of keyvals string*/
	uint16_t kvlen;
	/**  MDNS flags */
	uint32_t flags;
};

/** mdns service config structure
 *  
 * config structure contains interface handle and service list to be published
 * on that interface. config->interface holds the interface handle and config->services
 * holds the pointer to list of services(struct mdns_services **services). If service is
 * NULL-terminated list of pointers advertise give list of service. If this value is NULL,
 * no service will be advertised.  Only name resolution will be provided.
 */
struct mdns_service_config {
	/** interface handle */
	void *iface_handle;
	/** list of services to be exposed on given interface */
	struct mdns_service **services;
};

/** Total number of interface config supported by mdns */
#define MDNS_MAX_SERVICE_CONFIG 2

/** protocol values for the proto member of the mdns_service descriptor */
/** TCP Protocol */
#define MDNS_PROTO_TCP 0
/** UDP Protocol */
#define MDNS_PROTO_UDP 1

/** mdns_start
 *
 * Start the responder thread (and the querier thread if querying is enabled).
 *
 *
 * Note that the mdns_start() function must be called only after the network
 * stack is up.
 *
 * The responder thread wait for interface UP event to announce services.
 * Using \ref mdns_add_service_iface call, services are added to mdns service
 * config list. Now use \ref mdns_iface_state_change call to generate UP event
 * which will announce service to given interface(config->iface_handle).
 *
 * The querier thread just opens a socket and then waits until a request for monitoring
 * any given service type or subtype is received from the application . When received,
 * the same is queried and the responses are processed, cached and monitored.
 * 
 * \param domain domain name string.  If this value is NULL, the domain ".local" will
 * be used.  The domain must not exceed \ref MDNS_MAX_LABEL_LEN bytes.
 *
 * \param hostname string that is the hostname to resolve.  This would be the "foo"
 * in "foo.local", for example.  The hostname must not exceed
 * \ref MDNS_MAX_LABEL_LEN bytes.  If hostname is NULL, the responder capability
 * will not be launched, and the services array will be NULL; only query
 * support will be enabled.  This is useful if only the query functionality is
 * desired.
 *
 * \return WM_SUCCESS for success or mdns error code
 *
 * NOTES:
 *
 * The domain and hostname must persist and remain unchanged between calls to
 * mdns_start and mdns_stop. Hence define these variables in global memory.
 *
 * While mdns_start returns immediately, the hostname and any servnames may
 * not be unique on the network.  In the event of a conflict, the names will
 * be appended with an integer.  For example, if the hostname "foo.local" is
 * taken, mdns will attempt to claim "foo-2.local", then foo-3.local, and so on
 * until the conflicts cease.  If mdns gets all the way to foo-9.local and
 * still fail, it waits for 5 seconds (per the mDNS specification) and then
 * starts back at foo.local.  If a developer anticipates a network to have many
 * of her devices, she should devise a sensible scheme outside of mdns to
 * ensure that the names are unique.
 */
int mdns_start(const char *domain, char *hostname);

/** 
 * mdns_stop
 *
 * Halt the mDNS responder thread (and querier thread if querying is enabled),
 * delete the threads and close the sockets
 *
 * Any services being monitored will be unmonitored.
 */
void mdns_stop(void);

/** Note Configuring MDNS_SERVICE_CACHE_SIZE in mDNS Querier
 *
 * Suppose MDNS_SERVICE_CACHE_SIZE is 16 and that a user has invoked
 * mdns_query_monitor to monitor services of type _http._tcp.local. Assume
 * the query callback handler returns WM_SUCCESS for all the instances
 * discovered.
 *
 * Further,suppose that this particular domain has 17 instances of this type.
 * The first 16 instances to be discovered will result in 16 callbacks with the
 * status MDNS_DISCOVERED.  These instances will be cached and monitored for
 * updates, disappearance, etc.  When the 17th instance is discovered, the
 * callback will be called as usual, but the status will be MDNS_CACHE_FULL,
 * and the service will not be monitored.  While a best effort is made to
 * deliver all of the service information, the mdns_service may be incomplete.
 * Specifically, the ipaddr may be 0 and the service name may be "".  Further,
 * the callback may be called again if the 17th instance of the service
 * announces itself on the network again.  If one of the other services
 * disappears, the next announcement from the 17th instance will result in a
 * callback with status MDNS_DISCOVERED, and from that point forward it will be
 * monitored.
 *
 * So what's the "best" value for MDNS_SERVICE_CACHE_SIZE?  This depends on the
 * application and on the field in which the application is deployed.  If a
 * particular application knows that it will never see more than 6 instances of
 * a service, then 6 is a fine value for MDNS_SERVICE_CACHE_SIZE.  In this
 * case, callbacks with a status of MDNS_CACHE_FULL would represent a warning
 * or error condition.  Similarly, if an application cannot handle any more
 * than 10 instances of a service, then MDNS_SERVICE_CACHE_SIZE should be 10
 * and callbacks with a status of MDNS_CACHE_FULL can be ignored.  If the
 * maximum number of service instances is not known, and the application
 * retains its own state for each instance of a service, it may be able to use
 * that state to do the right thing when the callback status is
 * MDNS_CACHE_FULL.
 *
 * For applications with constrained memory ,a point to note is that each
 * service instance requires little above 1K bytes. This should be considered
 * while deciding the MDNS_SERVICE_CACHE_SIZE.
 *
 * The current MDNS_SERVICE_CACHE_SIZE is set to 2.
 */


/** mdns query callback
 *
 * A user initiates a query for services by calling the mdns_query_monitor
 * function with a fully-qualified service type, an mdns_query_cb, and an
 * opaque argument.  When a service instance is discovered, the query callback
 * will be invoked with following arguments:
 *
 * \param data a void * that was passed to mdns_query_monitor().  This can be
 * anything that the user wants, such as pointer to a custom internal data
 * structure.
 *
 * \param s A pointer to the struct mdns_service that was discovered.  The struct
 * mdns_service is only valid until the callback returns.  So if attributes of
 * the service (such as IP address and port) are required by the user for later
 * use, they must be copied and preserved elsewhere.
 *
 * \param status A code that reports the status of the query.  It takes one of the
 * following values:
 *
 * MDNS_DISCOVERED: The mdns_service s has just been discovered on the network
 * and will be monitored by the mdns stack.
 *
 * MDNS_UPDATED: The mdns_service s, which is being monitored, has been updated
 * in some way (e.g., it's IP address has changed, it's key/value pairs have
 * changed.)
 *
 * MDNS_DISAPPEARED: The mdns_service has left the network.  This usually
 * happens when a service has shut down, or when it has stopped responding
 * queries.  Applications may also detect this condition by some means outside
 * of mdns, such as a closed TCP connection.
 *
 * MDNS_CACHE_FULL: The mdns_service has been discovered.  However, the number
 * of monitored service instances has exceeded MDNS_SERVICE_CACHE_SIZE.  So the
 * returned mdns_service may not be complete.  See NOTES below on other
 * implications of an MDNS_CACHE_FULL status.
 *
 * NOTES:
 *
 * The query callback should return WM_SUCCESS in the case where it has
 * discovered service of interest (MDNS_DISCOVERED, MDNS_UPDATED status). If
 * the callback return non-zero value for MDNS_DISCOVERED and MDNS_UPDATED
 * status codes, that particular service instance is not cached by the mDNS
 * querier. This is required as each cached service instance takes little above
 * 1KB memory and the device can't monitor large number of service instances.
 *
 * Callback implementers must take care to not make any blocking calls, nor to
 * call any mdns API functions from within callbacks.
 *
 */
typedef int (* mdns_query_cb)(void *data,
							  const struct mdns_service *s,
							  int status);

#define MDNS_DISCOVERED		1
#define MDNS_UPDATED		2
#define MDNS_DISAPPEARED	3
#define MDNS_CACHE_FULL		4

/** mdns_query_monitor
 *
 * Query for and monitor instances of a service
 *
 * When instances of the specified service are discovered, the specified
 * query callback is called as described above.
 *
 * \param fqst Pointer to a null-terminated string specifying the fully-qualified
 * service type.  For example, "_http._tcp.local" would query for all http
 * servers in the ".local" domain.
 *
 * \param cb an mdns_query_cb to be called when services matching the specified fqst
 * are discovered, are updated, or disappear.  cb will be passed the opaque
 * data argument described below, a struct mdns_service that represents the
 * discovered service, and a status code.
 *
 * \param data a void * that will passed to cb when services are discovered, are
 * updated, or disappear.  This can be anything that the user wants, such as
 * pointer to a custom internal data structure.
 *
 * \return WM_SUCCESS: the query was successfully launched.  The caller should expect
 * the mdns_query_cb to be invoked as instances of the specified service are
 * discovered.
 *
 * \return WM_E_MDNS_INVAL: cb was NULL or fqst was not valid.
 *
 * \return -WM_E_MDNS_NOMEM: MDNS_MAX_SERVICE_MONITORS is already being monitored.  Either
 * this value must be increased, or a service must be unmonitored by calling
 * mdns_query_unmonitor.
 *
 * \return -WM_E_MDNS_INUSE: The specified service type is already being monitored by another
 * callback, and multiple callbacks per service are not supported.
 *
 * \return -WM_E_MDNS_NORESP: No response from the querier.  Perhaps it was not launched or
 * it has crashed.
 *
 * Note: multiple calls to mdns_query_service_start are allowed.  This enables
 * the caller to query for more than just one service type.
 */
int mdns_query_monitor(char *fqst, mdns_query_cb cb, void *data);

/** mdns_query_unmonitor
 *
 * Stop monitoring a particular service
 *
 * \param fqst The service type to stop monitoring, or NULL to unmonitor all
 * services.
 *
 * \note Suppose a service has just been discovered and is being processed
 * while the call to mdns_query_monitor is underway.  A callback may be
 * generated before the service is unmonitored.
 */
void mdns_query_unmonitor(char *fqst);

/** mdns_add_service_iface
 *
 * Add interface service configuration in mdns
 *
 * This will add interface service config(\ref mdns_service_config) to
 * mdns service list after checking validity of service. If service is invalid
 * then it will return with appropriate error code.
 *
 * \param config mdns_service_config contains interface and list of services to
 * be published in that interface. config->iface_handle holds the interface
 * handle. One can get interface handle from net_get_sta_handle or
 * net_get_uap_handle. config->services holds the pointer to list of services
 * (\ref mdns_service **services). If service is NULL-terminated list
 * of pointers advertise gives list of service. If this value is NULL, no
 * service will be advertised and only name resolution will be provided.
 *
 * \return WM_SUCCESS: success
 *
 * \return -WM_E_MDNS_INVAL: input was invalid.  Perhaps a label exceeded MDNS_MAX_LABEL_LEN,
 * or a name composed of the supplied labels exceeded MDNS_MAX_NAME_LEN.
 * Perhaps hostname was NULL and the query capability is not compiled.
 *
 * \return -WM_E_MDNS_BADSRV: one of the service descriptors in the services list was
 * invalid.  Perhaps one of the key/val pairs exceeded MDNS_MAX_KEYVAL_LEN.
 *
 * \return -WM_E_MDNS_TOOBIG: The combination of name information and service descriptors
 * does not fit in a single packet, which is required by this implementation.
 *
 * \return -WM_FAIL: No space to add interface service config
 *
 * \note
 *
 * struct mdns_service elements in mdns_service_config must persist and
 * remain unchanged between calls to mdns_start and mdns_stop. Hence define
 * these variables in global memory.
 */
int mdns_add_service_iface(struct mdns_service_config config);

/** mdns_remove_service_iface
 *
 * Remove interface service configuration to mdns
 *
 * This sends good bye packet to "iface" interface.
 *
 * \param iface interface whose services needs to be removed from mdns
 *
 * \return WM_SUCCESS: success
 *
 * \return -WM_E_MDNS_INVAL: invalid parameter; can't find iface in mdns config
 * list
 *
 * \return -WM_E_MDNS_FSOC: failed to create loopback control socket
 *
 * \return -WM_E_MDNS_FBIND: failed to bind control socket
 *
 */
int mdns_remove_service_iface(void *iface);

/** mdns_iface_state_change
 *
 * Send interface state change event to mdns
 *
 * This will start or stop mdns state machine for given interface. Before
 * calling this function for first time, interface and services should be added
 * to mdns using \ref mdns_add_service_iface. For \ref UP state, mdns will
 * initially send init probes to check for name conflict in services and if
 * there are any conflicts it will resolve them by appending integer after
 * service name. After checking name conflicts, it will announce services
 * to multicast group. \ref DOWN state will send good bye packet to iface
 * and \ref REANNOUNCE will forcefully reannounce services. Generally REANNOUNCE
 * should be used during DHCP change event or network up even after link lost
 * event.
 *
 * \param iface interface handle
 *
 * \param state \ref UP, \ref DOWN or \ref REANNOUNCE
 *
 * \return WM_SUCCESS: success
 *
 * \return -WM_E_MDNS_INVAL: invalid parameters
 *
 * \return -WM_E_MDNS_FSOC: failed to create loopback control socket
 *
 * \return -WM_E_MDNS_FBIND: failed to bind control socket
 *
 */
int mdns_iface_state_change(void  *iface, int state);

void mdns_set_hostname(char *hostname);

#ifdef CONFIG_MDNS_TESTS
/** mdns_tests
 *
 * Run internal mdns tests
 *
 * This function is useful to verify that various internal mdns functionality
 * is properly working after a port, or after adding new features.  If
 * MDNS_TEST is undefined, it is an empty function.  It is not meant to be
 * compiled or run in a production system.  Test output is written using
 * mdns_log().
 */
void mdns_tests(void);

int mdns_cli_init(void);
void dname_size_tests(void);
void dname_cmp_tests(void);
void increment_name_tests(void);
void txt_to_c_ncpy_tests(void);
void responder_tests(void);

#endif

/** Set TXT Records
 *
 * This macro sets the TXT record field for a given service.
 * recname is the name of the character string which has been populated
 * with the keyvals as described in struct mdns_service
 */
#define SET_TXT_REC(mdns_service, recname)	\
mdns_service.keyvals = recname;		\
mdns_service.kvlen = strlen(recname) + 1;

#endif
