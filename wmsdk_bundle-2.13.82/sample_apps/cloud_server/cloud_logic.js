/** NOTE: DEPRECATED FILE; SOON TO BE REMOVED */

/* This is a function to implement all fancy cloud logic. It is invoked on the
 * POST by a device. wmdevice object contains all latest device data with 
 * appropriate timestamps. The function needs to fill in response object for
 * the additional information to be requested by the device. */
exports.handle_post_common = function(wmdevice, response, lp_request) {

	/*
	if (!wmdevice.data.cloud || !wmdevice.data.cloud.url) {
		response.data["cloud"] = new Object();
		response.data.cloud["url"] = "?";
		return;
	}*/

	/* Example to request RSSI after every 15 requests *
	/*
	if (wmdevice.latest.sequence % 15 == 0) {
		console.log("Requesting RSSI");
		if (!response.data["sys"])
			response.data["sys"] = new Object();
		response.data.sys["rssi"] = "?";
		response.data.sys["time"] = "?";

		response.data.sys["diag"] = new Object();
		response.data.sys.diag["diag_live"] = "?";
		response.data.sys.diag["diag_history"] = "?";
	}
	*/

	/* Dump entire device json */
	console.log("Device: %j\n", wmdevice);


	/* Example of rebooting the device after every 7 requests */
	/*
	if (wmdevice.latest.sequence % 7 == 0) {
		console.log("Requesting reboot");
		if (!response.data["sys"])
			response.data["sys"] = new Object();
		response.data.sys["reboot"] = "1";

	} */

}
