/* Cloud server configurations */
var http_port = 80; /* HTTP(non-TLS) Server port for devices to communicate */
var https_port = 443; /* HTTP(TLS) Server port for devices to communicate */
var request_logging = true; /* If to log the requests in a file */
var logs_dir = 'logs'; /* Directory to hold device logs */
var http_long_poll_timeout = (5*60*60) /* HTTP long polling socket timeout in sec */
var conn_list = new Array();

/* Required node modules */
var fs = require('fs'),
	path = require('path'),
	http = require('http'),
	https = require('https'),
	express = require('express'),
	WebSocketServer = require('websocket').server;

/* Use bodyParser for parsing JSON natively */
var srv1 = express();
srv1.use(express.bodyParser());
srv1.use(express.static(__dirname + '/www'));

var srv2 = express();
srv2.use(express.bodyParser());
srv2.use(express.static(__dirname + '/www'));

var wmresponse = require('./wmresponse.js');
var wm = require('./wmdevice.js');
var applogic = require ('./applogic.js');


/* Setup HTTPS server certificate and key */
var options = {
	key: fs.readFileSync('server.key'),
	cert: fs.readFileSync('server.crt'),
};

/* Start server */
var httpsServer = https.createServer(options, srv1).listen(https_port, function(){
	console.log("HTTP(TLS) server listening on port " + https_port);
});

var httpServer = http.createServer(srv2).listen(http_port, function(){
	console.log("HTTP(non-TLS) server listening on port " + http_port);
});

wsServer1 = new WebSocketServer({
	httpServer: httpsServer,
	autoAcceptConnections: false,
});

wsServer2 = new WebSocketServer({
	httpServer: httpServer,
	autoAcceptConnections: false,
});

if (request_logging) {
	try {
		fs.mkdirSync(logs_dir);
	}catch(err) {}
}   

console.log("Calling applogic init for HTTP(TLS) Server");
applogic.init(srv1);
console.log("Calling applogic init for HTTP(non-TLS) Server");
applogic.init(srv2);

function log_request(req)
{
	var tmpstr;
	var uuid = req.header.uuid;
	var epoch = req.header.epoch;
	var sequence = req.header.sequence;
	var date = new Date();
	var posixtm = Math.floor(date.getTime() / 1000);

	tmpstr = posixtm + "\t" + epoch + "\t" + sequence + "\n";

	var file = logs_dir + "/" + uuid + '.log';

	fs.appendFile(file, tmpstr, function(err) {
		if (err) {
		   	throw err;
			console.log("Error writing file");
		}
	});
}

var pending_responses = new Array();

var setup_long_polling = function(dev, response, res) {
	var uuid = dev.uuid;

	if (pending_responses[uuid] &&
		pending_responses[uuid].state == "device_requested") {
		console.log("Sending response");
		var response = pending_responses[uuid].response;
		pending_responses[uuid].res.writeHead(200);
		pending_responses[uuid].res.write(JSON.stringify(dev));
		pending_responses[uuid].res.end();
		pending_responses[uuid].state = "device_waiting";
	}

	console.log("Doing HTTP long polling");
	if (!pending_responses[uuid])
		pending_responses[uuid] = new Object();

	pending_responses[uuid].response = response;
	pending_responses[uuid].res = res;
	pending_responses[uuid].state = "device_waiting";
	console.log("Device in waiting state");
	return false;
}

var check_if_pending_response = function(dev, response) {
	var uuid = dev.uuid;
	var pending_response = pending_responses[uuid];

	if (!pending_response)
		return;

	if (pending_response.state == "device_requested") {
		response.deepCopy(response.data, pending_response.response.data);
		pending_response.response = null;
		pending_response.state = "device_waiting";
	}
}

function handleCloudPost(req, res) {
	var request = req.body;

	var uuid = request.header.uuid;
	var epoch = request.header.epoch;
	var sequence = request.header.sequence;
	var device_type = request.header.type;

	console.log("Device POST: %j", request);
	if (!uuid) {
		console.log("Can't accept requests from anonymous device");
		/* Send error code = 400 (Bad Request) */
		res.writeHead(400);
		res.end();
	}

	if (request_logging)
		log_request(request);

	var response = new wmresponse();
	var dev = wm.get_device(uuid);
	
	if (!dev) {
		dev = new wm.wmdevice(uuid, device_type);
		dev.conn_type = 'http';
		dev.update(request);
		applogic.new_device(dev, response);
	} else {
		dev.update(request);
	}

	var send_response = true;

	if (dev.long_poll) {
		/* 5 hours of connection timeout */
		req.connection.setTimeout(http_long_poll_timeout * 1000);
		send_response = setup_long_polling(dev, response, res);
	} else {
		check_if_pending_response(dev, response);
	}
	
	if (send_response) {
		applogic.handle_post(dev, request, response);
		//dev.handle_device_post(response);
		console.log("Cloud->dev %j", response);
		response.send(res, 200);
	}
};

/* Main cloud POST handler */
srv1.post('/cloud', handleCloudPost);
srv2.post('/cloud', handleCloudPost);


function handleDevicePost(req, res) {

	var uuid = req.params.uuid;
	var dev = wm.get_device(uuid);
	if (!dev) {
                console.log("device with requested uuid not found");
                res.writeHead(400);
                res.end();
                return;
        }	
	if (dev.conn_type == 'websocket') {
		if (pending_responses[uuid].state == "device_requested") {
			/* Request already pending for this device. Return error
			 * (406 - not acceptable) so that requester can retry */
			res.writeHead(406);
			res.end();
			return;
		}

		var resp = new wmresponse();
		resp.data.deepCopy(resp.data, req.body.data);

		console.log('Sending: %j ', resp);

		if (conn_list[uuid]) {
			conn_list[uuid].send(JSON.stringify(resp));
			pending_responses[uuid].res = res;
			pending_responses[uuid].state = "device_requested";
		} else {
			res.writeHead(404);
			res.end();
		}

	} else {
		var pending_response;

		if (dev.long_poll) {
			/* Post on the long polling device results in synchronous sending data
		 	* to device, receiving response and sending response to requester */
			pending_response = pending_responses[uuid];
			if (!pending_response) {
				console.log("Insufficient data for long poll");
				res.writeHead(400);
				res.end();
				return;
			}
			if (pending_response.state == "device_requested") {
				/* Request already pending for this device. Return error
				 * (406 - not acceptable) so that requester can retry */
				res.writeHead(406);
				res.end();
				return;
			}
			/* Copy request parameters into response to be sent to device */
			pending_response.response.data.deepCopy(pending_response.response, req.body.data);
			applogic.handle_request(dev, req.body, pending_response.response);

			console.log("Cloud->dev %j", pending_response.response);
			pending_response.response.send(pending_response.res, 200);

			/* Prepare new response to be sent to requester */
			var response = new wmresponse();
			pending_response.response = response;
			pending_response.res = res;
			pending_response.state = "device_requested";
		} else {
			/* Post on the non long polling device results only in queuing request for
			 * next post receive. Requester is immediately sent 200 OK. */
			pending_responses[uuid] = new Object;
			pending_response = pending_responses[uuid];
			
			var response = new wmresponse();
			pending_response.response = response;
			pending_response.state = "device_requested";

			/* Copy request parameters into response to be sent to device */
			pending_response.response.data.deepCopy(pending_response.response.data, req.body.data);
			applogic.handle_request(dev, req.body, pending_response.response);
			res.writeHead(200);
			/* Send a {"success":0} response */
			var response_data = new Object();
			response_data.success = parseInt(0);
			res.write(JSON.stringify(response_data));
			res.end();
		}
	}
};
srv1.post("/devices/:uuid", handleDevicePost);
srv2.post("/devices/:uuid", handleDevicePost);

var test_devices = new Array();
test_devices[0] = new Object();
test_devices[0]["type"] = "power_switch";
test_devices[0]["uuid"] = "c861a4181baf4725b0ee4568ac71";
test_devices[1] = new Object();
test_devices[1]["type"] = "power_switch";
test_devices[1]["uuid"] = "c861a4181baf4725b0ee4568ac72";
test_devices[2] = new Object();
test_devices[2]["type"] = "power_switch";
test_devices[2]["uuid"] = "c861a4181baf4725b0ee4568ac73";

function handleDevicesGet(req, res) {

	var out = new Object();
	out = wm.get_all_devices();	
	res.writeHead(200);
	res.write(JSON.stringify(out));
//	res.write(JSON.stringify(test_devices));
	res.end();
};

srv1.get("/devices", handleDevicesGet);
srv2.get("/devices", handleDevicesGet);

function handleDeviceGet(req, res) {
	var device = wm.get_device(req.params.uuid);

	res.writeHead(200)
	res.write(JSON.stringify(device));
	res.end();
};

srv1.get("/devices/:uuid", handleDeviceGet);
srv2.get("/devices/:uuid", handleDeviceGet);


function originIsAllowed(origin) {
	/* Include logic here to detect whether the specified origin is allowed. */
	return true;
}

function handleWebsocketRequest(request) {

	if (!originIsAllowed(request.origin)) {
		/* Make sure we only accept requests from an allowed origin */
		request.reject();
		return;
	}

	var connection = request.accept('chat', request.origin);
	conn = connection;

	connection.on('message', function(message) {
		if (message.type === 'utf8') {
			var request = JSON.parse(message.utf8Data);
			var uuid = request.header.uuid;
			var epoch = request.header.epoch;
			var sequence = request.header.sequence;
			var device_type = request.header.type;

			console.log('Received Message: ' + message.utf8Data);

			if (request_logging)
				log_request(request);

			var response = new wmresponse();
			var dev = wm.get_device(uuid);

			conn_list[uuid] = connection;

			if (!dev) {
				pending_responses[uuid] = new Object;
				dev = new wm.wmdevice(uuid, device_type);
				dev.conn_type = 'websocket';
				dev.update(request);
				applogic.new_device(dev, response);
			} else {
				dev.update(request);
				if (pending_responses[uuid] &&
				  pending_responses[uuid].state == "device_requested") {
					var res = pending_responses[uuid].res;
					res.writeHead(200);
					res.write(JSON.stringify(dev));
					res.end();
					pending_responses[uuid].state = null;
					pending_responses[uuid].response = null;
				}
			}
			applogic.handle_post(dev, request, response);

		}
		else if (message.type === 'binary') {
			/* Ignoring binary message as of now */
		}
	});

	connection.on('close', function(reasonCode, description) {
		for (var uuid in conn_list) {
                	if(conn_list[uuid] == connection) {
				pending_responses[uuid].state = null;
                                pending_responses[uuid].response = null;
				break;
			}
		}
	});

	connection.on('error', function(error) {
	});
};

wsServer1.on('request', handleWebsocketRequest);
wsServer2.on('request', handleWebsocketRequest);
