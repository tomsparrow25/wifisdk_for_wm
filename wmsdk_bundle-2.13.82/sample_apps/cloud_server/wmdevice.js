var g_devices = new Array();

var cloud_logic = require('./cloud_logic.js');

module.exports.wmdevice = function(uuid, type)
{
	if (g_devices[uuid]) {
		return g_devices[uuid];
	}

	/* Create latest and data objects */
	this.uuid = uuid;
	this.type = type;
	this.latest = new Object();
	this.data = new Object();

	Object.defineProperty(Object.prototype, "deepExtend", {
	configurable: true,
	enumerable: false,
	value: function(destination, source, epoch, sequence) {
			for (var property in source) {
				if (typeof source[property] === "object" &&
					source[property] !== null ) {
						destination[property] = destination[property] || {};
						arguments.callee(destination[property], source[property], epoch, sequence);
					} else {
						destination[property] = new Object();
						destination[property]['value'] = source[property];
						destination[property]['epoch'] = epoch;
						destination[property]['sequence'] = sequence;
					}
			}
			return destination;
		}
	});

	this.update = function(request) {
		var epoch = request.header.epoch;
		var seq = request.header.sequence;
		var name = request.header.name;

		/* Fill in latest epoch and sequence */
		this.latest['epoch'] = epoch;
		this.latest['sequence'] = seq;
		this.name = name;
		
		/* Copy data part of the request into object. This function
		 * also creates epoch-seq timestamps for all the leaf node
		 * values. */
		this.data.deepExtend(this.data, request.data, epoch, seq);
	}
	
	this.handle_device_post = function(response, lp_request) {
	//	console.log("JObj %j", this);
		cloud_logic.handle_post_common(this, response, lp_request);
	}

	/* Add new object to global list of devices */
	g_devices[uuid] = this;
	return;
}

module.exports.get_device = function(uuid) {
	if (!uuid)
		return null;
	return g_devices[uuid];
}

module.exports.get_all_devices = function() {
	var arr = new Array();
	var i = 0;

	for (var index in g_devices) {
		arr[i] = new Object();
		arr[i]["uuid"] = index;
		arr[i]["type"] = g_devices[index].type;
		arr[i]["name"] = g_devices[index].name;
		i++;
	}

	return arr;
}
