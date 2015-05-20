var device_names = new Object();
device_names["power_switch"] = "Power Switch";
device_names["wm_demo"] = "WM Demo";
var months=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
var device_classes;
Backbone.emulateHTTP = true;
 _.templateSettings = {
      interpolate : /\{\{(.+?)\}\}/g
    };
$.fn.extend({
    sliderLabels: function(left,right) {
	var $this = $(this); 
	var $sliderdiv= $this.next("div.ui-slider");
	var $sliderdiv= $this;
	$sliderdiv
	.css({'font-weight': 'normal'}); 
	$sliderdiv
		.prepend('<span style="position: absolute; left:0px; top:20px;">'+left+ '</span>')
	.append('<span class="ui-slider-inner-label" style="position: absolute; right:0px; top:20px;">'+right+ '</span>');	 
    }
});


var Device = Backbone.Model.extend({
	idAttribute: "uuid",
	urlRoot: '/devices',
	defaults: {
			time: '',
			rssi: '',
			uuid: '',
			name: '',
			type: '',
			long_poll: '',
			latest: {
				epoch:'',
				sequence:''
			},
			data: {
				cloud : {
					url:''
				}
			}
		
	},
	initialize: function()  {
		console.log('Single device init');
	}
});

var DeviceList = Backbone.Collection.extend({
	model: Device,
	url: '/devices'
});
var devices = new DeviceList();
var DeviceContainerView = Backbone.View.extend({
	refresh_timer : '',
	refresh_time : 3,
	appView : '',
	deviceView : '',
	initialize: function() {
		_.bindAll(this);
		var self = this;
		self.render();
	},
	render: function() {
		this.appView = null;
		this.deviceView = null;
		$('#dvselectdev').nextAll('*').remove();
		if (device_views[this.model.get('type')] != undefined)
			this.appView = new device_views[this.model.get('type')]({model : this.model});
		this.deviceView = new DeviceView({model : this.model});
		clearTimeout(this.refresh_timer);
		this.refresh_timer = setTimeout(this.refetch, this.refresh_time * 1000);
	},
	refetch: function() {
		if(this.appView != null)
			this.appView.refetch();
		if(this.deviceView != null)
			this.deviceView.refetch();
		clearTimeout(this.refresh_timer);
		this.refresh_timer = setTimeout(this.refetch, this.refresh_time * 1000);
	}
});

var Power_Switch_AppView = Backbone.View.extend({
	events: {
		'slidestop .toggle_switch': 'submit_state_request',
	},
	header: 'power_switch_mgt_hdr',
	initialize: function() {
		_.bindAll(this);
		var self = this;
		var template = $('#' + self.model.get('type') + "-app-template");
		self.template = _.template(template.html());
		console.log('Power Switch Appview init');
		self.render();
	},
	clear_status: function() {
		this.$(".post_status").text("");
	},
	submit_data: function(post_data) {
		var self = this;
		this.model.save(post_data, {
			patch: true,
			success: function(response) {
				console.log("Post succeeded for " + self.model.get('uuid'));
			},
			error: function() {
				console.log("Post failed for " + self.model.get('uuid'));
				self.$(".post_status").text("Failed to Submit");
				setTimeout(self.clear_status , 3000);
			}
		});
	},
	submit_state_request: function ()  {
		value = this.$(".toggle_switch").slider("value");
		var post_data = new Object();
		var data = new Object();
		var power_switch = new Object();
		power_switch["state"] = parseInt(value);
		data.power_switch = power_switch;
		post_data.data = data;
/* This is a work around for the UI to work on iPAD + Safari.
 * It was found that the object post_data created above causes problem because of
 * the presence of the variable name 'state'. However, if we also add some another variable
 * in the same object, there is no problem.
 * But, even in this case, if the variable name is some static name, only 2 consecutive
 * POSTs work and it starts failing thereafter. Hence, here we are fetching the system time
 * and using it so as to get a different dummy variable name each time.
 */
		var d = new Date();
		var dummy = d.getTime();
		post_data[dummy] = 1;
		this.submit_data(post_data);
	},
	fetch_current_state : function() {
		var self = this;
		var post_data = new Object();
		var data = new Object();
		var power_switch = new Object();
		power_switch["state"] = '?';
		data.power_switch = power_switch;
		post_data.data = data;
/* See above for the explanation of the dummy variable */
		var d = new Date();
		var dummy = d.getTime();
		post_data[dummy] = 1;
		this.model.save(post_data, {
			patch: true,
			success: function(response) {
				self.refresh();
			},
			error: function() {
				console.log("Fetch failed for " + self.model.get('uuid'));
			}
		});
	},
	render: function() {
		var self = this;
		$("#accordion").append('<h3 id="' + this.header + '">Power Switch Management</h3>');
		$("#accordion").append(this.$el.html(this.template(this.model.toJSON())));
		this.$(".toggle_switch").slider({ min: 0, max: 1, step: 1, animate: "fast", value: 0 }).css({width: "4em"});
		this.$(".toggle_switch").sliderLabels('Off','On');

		$("#accordion").accordion("refresh");
		$("#accordion").accordion( "option", "active", 1 );
		this.fetch_current_state();
		return this;
	},
	refetch: function() {
		if($('#' + this.header).next().is(':visible') == false)
			return;
		this.fetch_current_state();
		console.log('Refetching Power Switch info');
	},
	refresh: function() {
		data = this.model.get('data');
		if((data.power_switch && data.power_switch.state && typeof(data.power_switch.state.value)) != undefined) {
			$("#toggle_switch").slider("value", parseInt(this.model.get('data').power_switch.state.value))
		}
	}
});


var Wm_Demo_AppView = Backbone.View.extend({
	events: {
		'slidestop .toggle_switch': 'submit_state_request',
	},
	header: 'wm_demo_mgt_hdr',
	initialize: function() {
		_.bindAll(this);
		var self = this;
		var template = $('#' + self.model.get('type') + "-app-template");
		self.template = _.template(template.html());
		console.log('WM Demo Appview init');
		self.render();
	},
	clear_status: function() {
		this.$(".post_status").text("");
	},
	submit_data: function(post_data) {
		var self = this;
		this.model.save(post_data, {
			patch: true,
			success: function(response) {
				console.log("Post succeeded for " + self.model.get('uuid'));
			},
			error: function() {
				console.log("Post failed for " + self.model.get('uuid'));
				self.$(".post_status").text("Failed to Submit");
				setTimeout(self.clear_status , 3000);
			}
		});
	},
	submit_state_request: function ()  {
		value = this.$(".toggle_switch").slider("value");
		var post_data = new Object();
		var data = new Object();
		var wm_demo = new Object();
		wm_demo["led_state"] = parseInt(value);
		data.wm_demo = wm_demo;
		post_data.data = data;
/* This is a work around for the UI to work on iPAD + Safari.
 * It was found that the object post_data created above causes problem because of
 * the presence of the variable name 'state'. However, if we also add some another variable
 * in the same object, there is no problem.
 * But, even in this case, if the variable name is some static name, only 2 consecutive
 * POSTs work and it starts failing thereafter. Hence, here we are fetching the system time
 * and using it so as to get a different dummy variable name each time.
 */
		var d = new Date();
		var dummy = d.getTime();
		post_data[dummy] = 1;
		this.submit_data(post_data);
	},
	fetch_current_state : function() {
		var self = this;
		var post_data = new Object();
		var data = new Object();
		var wm_demo = new Object();
		wm_demo["led_state"] = '?';
		data.wm_demo = wm_demo;
		post_data.data = data;
/* See above for the explanation of the dummy variable */
		var d = new Date();
		var dummy = d.getTime();
		post_data[dummy] = 1;
		this.model.save(post_data, {
			patch: true,
			success: function(response) {
				self.refresh();
			},
			error: function() {
				console.log("Fetch failed for " + self.model.get('uuid'));
			}
		});
	},
	render: function() {
		var self = this;
		$("#accordion").append('<h3 id="' + this.header + '">Wireless Microcontroller Demo</h3>');
		$("#accordion").append(this.$el.html(this.template(this.model.toJSON())));
		this.$(".toggle_switch").slider({ min: 0, max: 1, step: 1, animate: "fast", value: 0 }).css({width: "4em"});
		this.$(".toggle_switch").sliderLabels('Off','On');

		$("#accordion").accordion("refresh");
		$("#accordion").accordion( "option", "active", 1 );
		this.fetch_current_state();
		return this;
	},
	refetch: function() {
		if($('#' + this.header).next().is(':visible') == false)
			return;
		this.fetch_current_state();
		console.log('Refetching WM Demo info');
	},
	refresh: function() {
		data = this.model.get('data');
		if((data.wm_demo && data.wm_demo.led_state && typeof(data.wm_demo.led_state.value)) != undefined) {
			$("#toggle_led").slider("value", parseInt(this.model.get('data').wm_demo.led_state.value))
		}
	}
});

var DeviceView = Backbone.View.extend({
	events: {
		'click .submit_cloud_request': 'submit_cloud_request',
      	},
	header: 'dev_mgt_hdr',
	initialize: function() {
		_.bindAll(this);
		var self = this;
		this.model.fetch({
			success: function(data){
				console.log("Device details get successful");
				data = self.model.get('data');
				if((data.sys && data.sys.time && typeof(data.sys.time.value)) != undefined) {
					var d = new Date(data.sys.time.value * 1000);
					self.model.set('time', d.toDateString() + " " + d.toTimeString());
				}
				if((data.sys && data.sys.rssi && typeof(data.sys.rssi.value)) != undefined) {
					self.model.set('rssi', data.sys.rssi.value);
				}
				self.render();
			},
			error: function() {
				alert('Error fetching Data');
				console.log("Error");
			}
		})
		this.template = _.template($('#device-template').html());
		console.log('Deviceview init');
	},
	handle_upgrades: function(data) {
		if(data.sys == undefined)
			sys = new Object();
		else
			sys = data.sys;
		if($("#upgrade_check").is(':checked')) {
			var firmware = new Object();
			if(this.$('.wififw_url').val() !='') {
				firmware.wififw_url = this.$('.wififw_url').val();
			}
			if(this.$('.fs_url').val() !='') {
				firmware.fs_url = this.$('.fs_url').val();
			}
			if(this.$('.fw_url').val() !='') {
				firmware.fw_url = this.$('.fw_url').val();
			}
			if(!jQuery.isEmptyObject(firmware))
				sys.firmware = firmware;
		}
		if(!jQuery.isEmptyObject(sys))
			data.sys = sys;
		return data;
	},
	handle_reboot: function(data) {
		if(data.sys == undefined)
			sys = new Object();
		else
			sys = data.sys;
		if($("#reboot_check").is(':checked')) {
			sys.reboot = "1";
		}
		if(!jQuery.isEmptyObject(sys))
			data.sys = sys;
		return data;
	},
	handle_get_additional_info: function(data) {
		if(data.sys == undefined)
			sys = new Object();
		else
			sys = data.sys;
		if($("#getinfo_check").is(':checked')) {
			if($("#rssi").is(':checked'))
				sys.rssi = "?";
			if($("#time").is(':checked'))
				sys.time = "?";
			var diag = new Object();
			if($("#diag_live").is(':checked'))
				diag.diag_live = "?";
			if($("#diag_history").is(':checked'))
				diag.diag_history = "?";
			if(!jQuery.isEmptyObject(diag))
				sys.diag = diag;
		}
		if(!jQuery.isEmptyObject(sys))
			data.sys = sys;
		return data;
	},
	handle_cloud_config: function(data) {
		if(data.cloud == undefined)
			cloud = new Object();
		else
			cloud = data.cloud;
		if($("#cloud_url_check").is(':checked')) {
			if(this.$('.cloud_url').val() != '')
				cloud.url = this.$('.cloud_url').val();
		}
		if(!jQuery.isEmptyObject(cloud))
			data.cloud = cloud;
		return data;
	},
	handle_set_time: function(data) {
		if(data.sys == undefined)
			sys = new Object();
		else
			sys = data.sys;
		if($("#time_check").is(':checked')) {
			var time = new Object();
			year = $('.year_select').val();
			month = $('.month_select').val();
			day = $('.day_select').val();
			hours = $('.hr_select').val();
			minutes = $('.min_select').val();
			var d = new Date(year, month, day, hours, minutes, 0, 0);
			var myEpoch = d.getTime()/1000;
			sys.time = parseInt(myEpoch);
		}
		if(!jQuery.isEmptyObject(sys))
			data.sys = sys;
		return data;
	},
	clear_status: function() {
		this.$(".post_status").text("");
	},
	submit_cloud_request : function(ev) {
		var data = new Object();
		data = this.handle_upgrades(data);
		data = this.handle_reboot(data);
		data = this.handle_get_additional_info(data);
		data = this.handle_cloud_config(data);
		data = this.handle_set_time(data);
		var self = this;
		if(!jQuery.isEmptyObject(data)) {
			var post_data = new Object();
			post_data.data = data;
			console.log("Posting data: " + JSON.stringify(post_data));
		} else {
			self.$(".post_status").text("Please select atleast one option");
			setTimeout(self.clear_status , 3000);
		}
		if(!jQuery.isEmptyObject(data)) {
			this.model.save(post_data, {
				patch: true,
		    		success: function(response) {
					console.log("Post succeeded for " + self.model.get('uuid'));
					self.$(".post_status").text("Submitted Successfully");
					$("#getinfo_check").attr('checked', false);
					setTimeout(self.clear_status , 3000);
				},
				error: function() {
					console.log("Post failed for " + self.model.get('uuid'));
					self.$(".post_status").text("Failed to Submit");
					setTimeout(self.clear_status , 3000);
				}
			});
		}
	},
	render: function()  {
		$("#accordion").append('<h3 id="' + this.header + '" >Device Management</h3>');
		$("#accordion").append(this.$el.html(this.template(this.model.toJSON())));
/* The click event handler has been added to show better icons for the checkbox when selected
 * and when not*/
		this.$('.selectdevoperations').button({
			icons: {primary: "ui-icon-radio-off"},
			text: false
		}).click(function () {
			if (this.checked) {
				$(this).button("option", "icons", {primary: "ui-icon-circle-check"})
			} else {
				$(this).button("option", "icons", {primary: "ui-icon-radio-off"})
		}
		})
		.filter(":checked").button({icons: {primary: "ui-icon-circle-check"}});
		for (i=1; i<32; i++) {
			var o = new Option("value", i);
			$(o).html(i);
			this.$(".day_select").append(o);
		}
		for (i=0; i<months.length; i++) {
			var o = new Option("value", i);
			$(o).html(months[i]);
			this.$(".month_select").append(o);
		}
		for (i=1970; i<2030; i++) {
			var o = new Option("value", i);
			$(o).html(i);
			this.$(".year_select").append(o);
		}
		for (i=0; i<24; i++) {
			var o = new Option("value", i);
			$(o).html(i);
			this.$(".hr_select").append(o);
		}
		for (i=0; i<60; i++) {
			var o = new Option("value", i);
			$(o).html(i);
			this.$(".min_select").append(o);
		}
		this.refresh();
		this.$(".info_buttons").buttonset();
		this.$('.submit_cloud_request').button({label:'Submit'});
		$("#accordion").accordion("refresh");
		$("#accordion").accordion( "option", "active", 1 );
		return this;
	},
	refetch: function() {
/* This checks if the div following the accordion header is visible or not
 * View is refreshed only if the div is visible */
		if($('#' + this.header).next().is(':visible') == false)
			return;
		console.log('Refetching Device info');
		var self = this;
		var post_data = new Object();
		var data = new Object();
		var sys = new Object();
		sys.time = '?';
		sys.rssi = '?';
		data.sys = sys;
		post_data.data = data;
/* This dummy variable is used because if the POST request always has the same data, the Safari on
 * iPAD does not post it */
		var d = new Date();
		var dummy = d.getTime();
		post_data[dummy] = 1;
		this.model.save(post_data, {
			patch: true,
			success: function(response) {
				data = self.model.get('data');
				if((data.sys && data.sys.time && typeof(data.sys.time.value)) != undefined) {
					var d = new Date(data.sys.time.value * 1000);
					self.model.set('time', d.toDateString() + d.toTimeString());
				}
				if((data.sys && data.sys.rssi && typeof(data.sys.rssi.value)) != undefined) {
					self.model.set('rssi', data.sys.rssi.value);
				}
				self.refresh();
			},
			error: function() {
				console.log("Device Refetch failed for " + self.model.get('uuid'));
			}
		});
	},
	refresh : function() {
		var d = new Date();
		this.$(".day_select").val(d.getDate());
		this.$(".month_select").val(d.getMonth());
		this.$(".year_select").val(d.getFullYear());
		this.$(".hr_select").val(d.getHours());
		this.$(".min_select").val(d.getMinutes());
		this.$(".epoch").text(this.model.get('latest').epoch);
		this.$(".sequence").text(this.model.get('latest').sequence);
		this.$(".cur_time").text(this.model.get('time'));
		this.$(".cur_rssi").text(this.model.get('rssi'));
	}
});

var device_views = new Object();
device_views["power_switch"] = Power_Switch_AppView;
device_views["wm_demo"] = Wm_Demo_AppView;

var DevicesView = Backbone.View.extend({
	model: devices,
	tagName: 'div',
	deviceContainerview : '',
	events: {
	'change .selectdevtype': 'selectdevtype_changed',
	'change .selectdev': 'selectdev_changed',
      	},
	initialize: function() {
		_.bindAll(this);
		var self = this;
		devices.fetch({
	  		success: function(data) 
			{ 
				console.log('Device list get successful');
				self.render();
			},
			error: function() { console.log('Cannot retrive models from server'); }
		});
		this.template = _.template($('#devicelist-template').html());
	},
	dev_types_populate: function() {
		device_classes = new Array();
		nr_found = this.model.toArray().length;
		_.each(this.model.toArray(), function(device, i) {
			device_classes[device.get('type')] = 0;
		});
		for (var i in device_classes) {
			var o = new Option("value", i);
			if(device_names[i] != undefined)
				$(o).html(device_names[i]);
			else
				$(o).html(i);
			this.$(".selectdevtype").append(o);
		}
		this.$(".selectdevtype").multiselect('refresh');
		$("#select-title").html("Select Device ("+nr_found+" found)");
	},

	selectdev_changed : function(ev) {
		$("#select-title").html("Select Device ("+nr_found+" found, 1 selected)");
		var uuid = $('.selectdev').val();
		deviceContainerview = new DeviceContainerView({model : this.model.get(uuid)}).
		$("#accordion").accordion("refresh");
		return;
	},

	selectdevtype_changed : function(ev) {
		this.$('.selectdev').find('option').remove().end();

		for (var i in device_classes) {
			device_classes[i] = 0;
		}

		this.$('.selectdevtype :selected').each(function(i, selected){
			device_classes[$(selected).val()] = 1;
		});

		_.each(this.model.toArray(), function(device, i) {
			if (device_classes[device.get('type')] == 1) {
				var name = device.get('name');
				if(name == "unknown") {
					name = name + ":" + device.get('uuid');
				}
				var o = new Option(name, device.get('uuid'));
				this.$(".selectdev").append(o);
			}
		});
		this.$(".selectdev").multiselect('refresh');
		$("#select-title").html("Select Device ("+nr_found+" found, 0 selected)");
	},


	render: function() {
		div = $('#dvselectdev');
		div.html(this.$el.html(this.template));
		this.$('.selectdevtype').multiselect({
			header: false,
			noneSelectedText:"Select Device Type",
			selectedText:"# Types Selected" });

		this.$('.selectdev').multiselect({
			header: false,
			noneSelectedText:"Select Device",
			selectedText:"# Device Selected",
			multiple: false
		});
		this.dev_types_populate();
		this.$('.selectdevtype').multiselect('refresh');
		this.$(".selectdev").multiselect('refresh');
		return this;
	}
});
$(document).ready(function() {
	console.log("Document ready");
	devicesView = new DevicesView();
	$("#accordion").accordion({ heightStyle: "content" });
	$("#select-title").html("Select Device (None selected)");

});
