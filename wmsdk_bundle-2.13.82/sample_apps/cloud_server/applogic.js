exports.init = function(srv) {
};
/* new_device function gets called when a device UUID is seen first time by the
 * cloud server. The applogic may set wmdevice.long_poll to true if it wants
 * that device to be controlled using long polling */
exports.new_device = function(wmdevice, response) {
	console.log("Applogic new_device called");

	if ((wmdevice.type == "power_switch" || wmdevice.type == "wm_demo") && wmdevice.conn_type == 'http') {
	/* For power_switch if websocket_support is disabled ,
	 * you may use long polling to control the cloud in real time */
		console.log("Long poll true");
		wmdevice.long_poll = true;
	} else {
		wmdevice.long_poll = false;
		console.log("Long poll false");
	}
};

//var upgrade_sent = 0;

/* This function is invoked for every post done by the device. Note that if
 * the device is set-up in long polling mode, this function gets invoked just
 * before sending a response to the device and not when POST actually comes */
exports.handle_post = function(wmdevice, request, response) {
    /* Example of rebooting the device after every 7 requests */
    // if (wmdevice.latest.sequence % 7 == 0) {
    // 	console.log("Requesting reboot");
    // 	if (!response.data["sys"])
    // 	    response.data["sys"] = new Object();
    // 	response.data.sys["reboot"] = "1";
    // }

    /* Example to request RSSI after every 15 requests */
    // if (wmdevice.latest.sequence % 2 == 0) {
    // 	console.log("Requesting RSSI");
    // 	if (!response.data["sys"])
    // 	    response.data["sys"] = new Object();
    // 	response.data.sys["rssi"] = "?";
    // 	response.data.sys["time"] = "?";

    // 	response.data.sys["diag"] = new Object();
    // 	response.data.sys.diag["diag_live"] = "?";
    // 	response.data.sys.diag["diag_history"] = "?";
    // }

    // if (!upgrade_sent && (wmdevice.latest.sequence == 1)) {
    // 	upgrade_sent = 1;
    // 	response.data["sys"] = new Object();
    // 	response.data.sys["firmware"] = new Object();
    // 	response.data.sys.firmware["wififw_url"] = "http://192.168.1.101:8000/sd8787_uapsta.bin.xz";
    // 	response.data.sys.firmware["fs_url"] =
    // 	    "http://192.168.1.101:8000/power_switch.ftfs";
    // 	response.data.sys.firmware["fw_version"] = "1.03";
    // 	response.data.sys.firmware["fw_url"] =
    // 	    "http://192.168.1.101:8000/helloworld.bin";
    // };
};

/* This function is invoked upon receiving an external control/status request
 * from a control device through /devices/[uuid] API.
 */
exports.handle_request = function (wmdevice, request, response) {

};

