/* 
 *  Copyright (C) 2011-2012, Marvell International Ltd.
 *  All Rights Reserved.
 */    

var networks;
var selected_nw;
var is_provisioned = 1;
var sysinfo_timer;
var g_frompg;
var g_last_cloud_operation_req;

function get_security(security) {
        if (security == 0)
                return "Unsecured open network";
        else if (security == 1)
                return "WEP secured network";
        else if (security == 3)
                return "WPA secured network";
        else if (security == 4)
                return "WPA2 secured network";
        else
                return "Invalid security";
}

function get_img(rssi, nf) {
	var snr;

	snr = rssi - nf;

	if (snr >= 40)
                return "signal3.png";
        else if (snr >= 25 && snr < 40)
                return "signal2.png";
        else if (snr >= 15 && snr < 25)
                return "signal1.png";
        else
                return "signal0.png";
}

function ChangePage(page, from, to)
{
	var forward = from < to;
	if (from == to) {
		$.mobile.changePage(page, "none");
	} else {
		$.mobile.changePage(page, {transition: "slide", reverse: !forward});
	}
}

function failed_connection_prov()
{
        $("#prov_status").html("Error: Connection error");
        $("#prov_status_details").html("Connection error has occurred. Please retry after re-connection.");
	
	$("#btn_wpapass_ok").removeClass('ui-disabled');
	$("#btn_wepkey_ok").removeClass('ui-disabled');
	$("#btn_open_ok").removeClass('ui-disabled');
        
	ChangePage('#status_page', g_frompg, 2);
}


function set_network_success(data)
{
        if(data["success"] == "0") {
                $("#prov_status").html("Provisioning Successful");
                $("#prov_status_details").html("The Wireless microcontroller is now configured. " +
					       "It will automatically associate with the configured wireless network \"" +
					       networks[selected_nw].ssid + "\"");
		is_provisioned = 1;

        } else {
                $("#prov_status").html("Error in Provisioning");
                $("#prov_status_details").html("An internal error has occurred. Please retry.");
	}

	$("#btn_wpapass_ok").removeClass('ui-disabled');
	$("#btn_wepkey_ok").removeClass('ui-disabled');
	$("#btn_open_ok").removeClass('ui-disabled');
        
	ChangePage('#status_page', g_frompg, 2);
}

function change_network_success(data)
{
        if(data["success"] == "0") {
                $("#adv_status").html("Setting New Network Successful");
                $("#adv_status_details").html("The network settings are configured again. " +
					      "It will automatically associate with the configured wireless network \"" +
					      $("#pg31_ssid").val() + "\"");
		is_provisioned = 1;

        } else {
                $("#adv_status").html("Error in Setting Network");
                $("#adv_status_details").html("An internal error has occurred. Please retry.");
        }
	$("#btn_change_nw").removeClass('ui-disabled');
        ChangePage('#adv_status_page', g_frompg, 7);
}

function set_network(postnw, success_fn, failed_fn)
{
        $.ajax({
			type: 'POST',
				url: "/sys/network",
				data: JSON.stringify(postnw),
				async: "false",
				dataType: 'json',
				timeout: 5000,
				success: success_fn,
				error: failed_fn,
				});

}

function wpapass_ok(from_page)
{
        var pass;
        var postnw = new Object();
	
	g_frompg = from_page;

	$("#btn_wpapass_ok").addClass('ui-disabled');

        postnw.ssid = networks[selected_nw].ssid;
        postnw.security = networks[selected_nw].security;
        postnw.ip = 1;

        if ($("#show_pass").attr("checked") == "checked") {
                pass = $("#wpa_pass_plain").val();
        } else {
                pass = $("#wpa_pass_crypt").val();
        }
        postnw.key = pass;

        set_network(postnw, set_network_success, failed_connection_prov);  
}


function wepkey_ok(from_page)
{
        var key;
        var postnw = new Object();

	g_frompg = from_page;

	$("#btn_wepkey_ok").addClass('ui-disabled');

        postnw.ssid = networks[selected_nw].ssid;
        postnw.security = networks[selected_nw].security;
        postnw.ip = 1;

        if ($("#show_key").attr("checked") == "checked") {
                key = $("#wep_key_plain").val();
        } else {
                key = $("#wep_key_crypt").val();
        }
        postnw.key = key;

        set_network(postnw, set_network_success, failed_connection_prov);  
}

function open_ok(from_page)
{
        var postnw = new Object();

	g_frompg = from_page;

	$("#btn_open_ok").addClass('ui-disabled');

        postnw.ssid = networks[selected_nw].ssid;
        postnw.security = networks[selected_nw].security;
        postnw.ip = 1;

        set_network(postnw, set_network_success, failed_connection_prov);  
}

function entry_clicked(nw)
{
        if (networks[nw].security == 0) {
                $.mobile.changePage('#enteropen');
                $("#netname2_h1").html(networks[nw].ssid);
        } else if (networks[nw].security == 1) {
                $("#div_wep_key_plain").hide();
                $.mobile.changePage('#enterwepkey');
                $("#netname1_h1").html(networks[nw].ssid);
        } else {
                $.mobile.changePage('#enterwpapass');
                $("#div_wpa_pass_plain").hide();
                $("#netname0_h1").html(networks[nw].ssid);
        }

        selected_nw = nw;
}

function scan_recv(data) {
        var $scanlist = $("#list-scan");
        var li_1 = "<li data-icon=\"false\" ><a href=\"#\" data-rel=\"dialog\" onClick=\"entry_clicked("
                var li_2= ")\"><p class=\"my_icon_wrapper\"><img src=\"";
        var li_3 = "\"></p><h3>";
        var li_4 = "</h3><p>";
        var li_5 = "</p></a></li>";

        data["networks"].sort(function(a,b) {return b[4] - a[4];});

        $scanlist.empty();
        networks = new Array();
        for (var i = 0; i < data["networks"].length; i++) {
                networks[i] = new Object();
                networks[i].ssid = data["networks"][i][0];
                networks[i].bssid = data["networks"][i][1];
                networks[i].security = data["networks"][i][2];
                networks[i].channel = data["networks"][i][3];
                networks[i].rssi = data["networks"][i][4];
                networks[i].nf = data["networks"][i][5];

                var img = get_img(networks[i].rssi, networks[i].nf);
                var sec = get_security(networks[i].security);
                var c = li_1 + i +li_2 + img + li_3 + networks[i].ssid + li_4 + sec + li_5;
                $scanlist.append(c);
        }
        $scanlist.listview("refresh");
	/* Second refresh is required for rounded corners of first and last entry */
        $scanlist.listview("refresh");
	$("#refreshbtn").removeClass('ui-disabled');
}

function refresh_btn_clicked() {

	$("#refreshbtn").addClass('ui-disabled');

        $.ajax({
			url: "/sys/scan",
				async: "false",
				success: scan_recv,
				error: failed_connection_prov,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();
}

function checkbox_adv_show_pass_changed()
{
        if ($("#adv_show_pass").attr("checked") == "checked") {
                $("#pass_plain").val($("#pass_crypt").val());
                $("#div_pass_crypt").hide();
                $("#div_pass_plain").show();
        } else {
                $("#pass_crypt").val($("#pass_plain").val());
                $("#div_pass_crypt").show();
                $("#div_pass_plain").hide();
        }
}


function checkbox_show_pass_changed()
{
        if ($("#show_pass").attr("checked") == "checked") {
                $("#wpa_pass_plain").val($("#wpa_pass_crypt").val());
                $("#div_wpa_pass_crypt").hide();
                $("#div_wpa_pass_plain").show();
        } else {
                $("#wpa_pass_crypt").val($("#wpa_pass_plain").val());
                $("#div_wpa_pass_crypt").show();
                $("#div_wpa_pass_plain").hide();
        }
}

function checkbox_show_key_changed()
{
        if ($("#show_key").attr("checked") == "checked") {
                $("#wep_key_plain").val($("#wep_key_crypt").val());
                $("#div_wep_key_crypt").hide();
                $("#div_wep_key_plain").show();
        } else {
                $("#wep_key_crypt").val($("#wep_key_plain").val());
                $("#div_wep_key_crypt").show();
                $("#div_wep_key_plain").hide();
        }
}
function select_provisioning(from_page)
{
	var prov_page = 1;

        if (is_provisioned) {
                select_sysinfo(3);
        } else {
                ChangePage('#page1', from_page, prov_page);
                refresh_btn_clicked();
        }
}

function get_time_done(data)
{
        if(data["epoch"] != "") {
                var curTime, hr, ms, sec;
                curTime = new Date(data["epoch"] * 1000);
                hr = curTime.getHours();
                ms = curTime.getMinutes();
                sec = curTime.getSeconds();

		if (hr < 10) hr = "0"+hr;
		if (ms < 10) ms = "0"+ms;
		if (sec < 10) sec = "0"+sec;

                $("#p_time").html(hr + ":" + ms + ":" + sec);
        }
}

function get_sysinfo_done(data)
{
	$("#refreshsysinfo").removeClass('ui-disabled');
        
	if((data["xmit"] != "") &&
           (data["recv"] != "") &&
           (data["network_errors"] != "")) {
                $("#p_packet_ct").html("Transmitted: "+data["xmit"]+
                                       ", Received: "+data["recv"]+
                                       ", Errors: "+data["network_errors"])
			}

        if(! is_provisioned) {
		$("#li_rssi").hide();
		if(data["conn_list"].length != 0) {
			$("#p_conn_cli").empty();
			for(var i = 0; i < data["conn_list"].length; i++) {
				if(i != 0) {
					$("#p_conn_cli").append(", ");
				}
				$("#p_conn_cli").append(data["conn_list"][i][0]+" ("+data["conn_list"][i][1]+")");
			}
		}
        } else {
                $("#li_conn_cli").hide();
                $("#li_dhcpcount").hide();
                $("#p_rssi").html(data["rssi"]);
        }

        if(data["verext"] != "") {
                $("#p_fw_ver").html(data["verext"]);
        }
        if(data["heap_usage"] != "") {
                $("#p_heap").html(data["heap_usage"] + " %");
        }
        if(data["dhcp_served"] != "") {
                $("#p_dhcpcount").html(data["dhcp_served"]);
        }
        if(data["cpu_freq"] != "") {
                $("#p_cpufreq").html(data["cpu_freq"] + " MHz");
        }

	sysinfo_timer = window.setTimeout(refresh_sysinfo, 7000);
}

function start_sysinfo_timer()
{
	refresh_sysinfo();
}

function stop_sysinfo_timer()
{
	clearTimeout(sysinfo_timer);
}

function submit_time(from_page)
{
	var myDate = new Date("Jan 1, 1970 " + $("#hr").val() + ":" + $("#min").val() + ":00");
	var myEpoch = myDate.getTime()/1000;
	var obj = new Object();
	obj.epoch = myEpoch;

	g_frompg = from_page;

        $.ajax({
			type: 'POST',
				url: "/sys/time",
				data: JSON.stringify(obj),
				async: "false",
				dataType: 'json',
				timeout: 5000,
				success: back_to_sysinfo,
				error: back_to_sysinfo,
				});
}

function select_dev_time(from_page)
{
        ChangePage('#page2_1', from_page, 4);

}

function back_to_sysinfo()
{
	select_sysinfo(g_frompg);
}

function select_sysinfo(from_page)
{
        ChangePage('#page2', from_page, 3);
}

function refresh_sysinfo()
{
	$("#refreshsysinfo").addClass('ui-disabled');

	$.ajax({
			url: "/sys/diag/info",
				async: "false",
				success: get_sysinfo_done,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();
 
	$.ajax({
			url: "/sys/time",
				async: "false",
				success: get_time_done,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();
      
}

function get_mode_done(data)
{
        if (data["mode"] == "0") {
                is_provisioned = 0;
                $("#div_scanlist").show();
                $("#div_provisioned").hide();
                select_provisioning();
        } else {
                is_provisioned = 1;
                $("#div_scanlist").hide();
                $("#div_provisioned").show();
                select_sysinfo();
        }
}

function get_mode()
{
        $.ajax({
			url: "/sys/mode",
				async: "false",
				success: get_mode_done,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();
}

function success_reboot(data)
{
        if(data["success"] == "0") {
                $("#adv_status").html("Device Reboot Successful");
                $("#adv_status_details").html("The device is rebooted successfully. Please re-launch the application when device boots up.");

        } else {
                $("#adv_status").html("Error in rebooting device");
                $("#adv_status_details").html("An internal error has occurred. Please retry.");
        }

	$("#btn_reboot").removeClass('ui-disabled');

	ChangePage('#adv_status_page', g_frompg, 7);

}

function failed_connection_adv()
{
        $("#adv_status").html("Error: Connection error");
        $("#adv_status_details").html("Connection error has occurred. Please retry after re-connection.");
	
	$("#btn_reboot").removeClass('ui-disabled');
	$("#btn_reset_to_prov").removeClass('ui-disabled');
	$("#btn_change_nw").removeClass('ui-disabled');
        
	ChangePage('#adv_status_page', g_frompg, 7);
}

function success_cloud_operation(data)
{
	if(data["success"] == "0") {
		if (g_last_cloud_operation_req) {
			$("#btn_submit_enable_cloud").removeClass('ui-disabled');
			$("#btn_submit_enable_cloud").hide();
			$("#btn_submit_disable_cloud").show();
			$("#cloud-status").html("Active");
		} else {
			$("#btn_submit_disable_cloud").removeClass('ui-disabled');
			$("#btn_submit_enable_cloud").show();
			$("#btn_submit_disable_cloud").hide();
			$("#cloud-status").html("Not started");
		}
	} else {
		$("#cloud-status").html("Unknown");
	}
}

function reboot_device(from_page)
{
        var reboot;
        reboot = new Object();

        reboot.command = "reboot";

	g_frompg = from_page;
	
	$("#btn_reboot").addClass('ui-disabled');

        $.ajax({
			type: 'POST',
				url: "/sys/command",
				data: JSON.stringify(reboot),
				async: "false",
				dataType: 'json',
				timeout: 5000,
				success: success_reboot,
				error: failed_connection_adv,
				});
}

function success_reset_to_prov(data)
{
        if(data["success"] == "0") {
                $("#adv_status").html("Reset to Provisioning Successful");
                $("#adv_status_details").html("The device is reset to provisioning mode. " + 
					      "Please re-connect to the device network and relaunch the application.");

        } else {
                $("#adv_status").html("Error in Reset to Provisioning");
                $("#adv_status_details").html("An internal error has occurred. Please retry.");
        }
	$("#btn_reset_to_prov").removeClass('ui-disabled');
        ChangePage('#adv_status_page', g_frompg, 7);

}

function reset_to_prov(from_page)
{
        var modeobj;
        modeobj = new Object();

	$("#btn_reset_to_prov").addClass('ui-disabled');

        modeobj.mode = 0;
	g_frompg = from_page;

        $.ajax({
			type: 'POST',
				url: "/sys/mode",
				data: JSON.stringify(modeobj),
				async: "false",
				dataType: 'json',
				timeout: 5000,
				success: success_reset_to_prov,
				error: failed_connection_adv,
				});
}


function select_advanced(from_page)
{
        ChangePage('#page3', from_page, 5);
}

function select_fw_upgrade(from_page)
{
        ChangePage('#page3_2', from_page, 6);
}

function select_reboot(from_page)
{
        ChangePage('#page3_3', from_page, 6);
}

function select_resetprov(from_page)
{
        ChangePage('#page3_4', from_page, 6);
}

function submit_cloud_operation(from_page, value)
{
	var json_req;
	if (value == 1) {
		$("#btn_submit_enable_cloud").addClass('ui-disabled');
		json_req = "{\"enabled\":1}";
	} else {
		$("#btn_submit_disable_cloud").addClass('ui-disabled');
		json_req = "{\"enabled\":0}";
	}

	g_last_cloud_operation_req = value;

        $.ajax({
			type: 'POST',
				url: "/wm_demo/cloud",
				data: json_req,
				async: "false",
				dataType: 'json',
				timeout: 5000,
				success: success_cloud_operation,
				error: failed_connection_adv,
				});
}

function get_cloud_done(data)
{
	var cloud_status = ["Inactive", "Not started", "Active",
			    "Connection error", "Transaction error",
			    "Stopped", "Thread Deleted"];

	$("#cloud-status").html(cloud_status[data["status"]]);

	if (data["status"] == 2) {
		$("#btn_submit_enable_cloud").hide();
		$("#btn_submit_disable_cloud").show();
	}

	if (data["status"] == 1) {
		$("#btn_submit_enable_cloud").show();
		$("#btn_submit_disable_cloud").hide();
	}
}

function get_sys_done(data)
{
	$("#dev-uuid").html(data["uuid"]);
}

function select_cloud(from_page)
{
    ChangePage('#page3_5', from_page, 6);
	$("#btn_submit_disable_cloud").hide();
	$("#btn_submit_enable_cloud").hide();

	/* Get Cloud status */
  	$.ajax({
			url: "/wm_demo/cloud",
				async: "false",
				success: get_cloud_done,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();

	/* Get device UUID */
	$.ajax({
			url: "/sys",
				async: "false",
				success: get_sys_done,
				dataType: "json",
				cache: "false",
				context: document.body
				}).done();


}
function get_ip(str)
{
        var ip1, ip2, ip3, ip4;

        ip1 = $("#"+str+"1").val();
        ip2 = $("#"+str+"2").val();
        ip3 = $("#"+str+"3").val();
        ip4 = $("#"+str+"4").val();
        
        return (ip1 + "." + ip2 + "." + ip3 + "." + ip4);
}

function submit_change_nw(from_page)
{
        var nwconfig;
        nwconfig = new Object();

	g_frompg = from_page;
	$("#btn_change_nw").addClass('ui-disabled');

        nwconfig.ssid = $("#pg31_ssid").val();
        if ($("#select-security").val() == "open") {
                nwconfig.security = 0;

        } else {
                if($("#select-security").val() == "wep") {
                        nwconfig.security = 1;
                }
                if($("#select-security").val() == "wpa") {
                        nwconfig.security = 3;
                }
                if($("#select-security").val() == "wpa2") {
                        nwconfig.security = 4;
                }
                if ($("#adv_show_pass").attr("checked") == "checked") {
                        nwconfig.key = $("#pass_plain").val();
                } else {
                        nwconfig.key = $("#pass_crypt").val();
                }
        }

        if ($("#select-ip").val() == "static") {
                nwconfig.ip = 0;
                nwconfig.ipaddr = get_ip("ip");
                nwconfig.ipmask = get_ip("nm");
                nwconfig.ipgw = get_ip("gw");
                nwconfig.ipdns1 = get_ip("pd");
                nwconfig.ipdns2 = get_ip("sd");
        } else {
                nwconfig.ip = 1;
        }

        set_network(nwconfig, change_network_success, failed_connection_adv);
}

function select_change_nw(from_page)
{
        $("#div_sel_sec_pass").hide();
        $("#div_static_ip").hide();
        $("#div_pass_plain").hide();
        $("#div_pass_crypt").show();
        ChangePage('#page3_1', from_page, 6);
        $("#select-security").val("open"); 
        $("#select-security").selectmenu("refresh");
        $("#select-ip").val("dhcp"); 
        $("#select-ip").selectmenu("refresh");
}

function select_sec_changed()
{
        if($("#select-security").attr("value") == "open") {
                $("#div_sel_sec_pass").hide();
        } else {
                $("#div_sel_sec_pass").show();
        }
}

function select_ip_mode_changed()
{
	if($("#select-ip").attr("value") == "dhcp") {
                $("#div_static_ip").hide();
        } else {
                $("#div_static_ip").show();
        }
}

function check_for_ie()
{
	if (navigator.appName=="Microsoft Internet Explorer") {
		$("#non_ie_client").hide();

		$("#note_h3").html("Browser not supported");
		$("#note_p").html("Please use Firefox/Google Chrome/Safari web browser to use this feature");

		$("#ie_client").show();
	} else {
		$("#ie_client").hide();
		$("#non_ie_client").show();
	}
}

function load() 
{

        $("#refreshbtn").bind("click", refresh_btn_clicked);
        $("#show_pass").bind("change", checkbox_show_pass_changed); 
        $("#adv_show_pass").bind("change", checkbox_adv_show_pass_changed); 
        $("#show_key").bind("change", checkbox_show_key_changed); 
        $("#select-security").bind("change", select_sec_changed); 
        $("#select-ip").bind("change", select_ip_mode_changed); 

                
	$("#div_scanlist").hide();
        $("#div_provisioned").hide();

	$("#page2").bind('pageshow', function () { start_sysinfo_timer(); });
	$("#page2").bind('pagehide', function () { stop_sysinfo_timer(); });
	$("#page3_2").bind('pageshow', function () { check_for_ie(); });

        get_mode();
}


function processPost(fileInput, postResource)
{
	var file = fileInput.files[0];

	// Get an XMLHttpRequest instance
	var xhr = new XMLHttpRequest();
	xhr.open('POST', postResource, true);

	xhr.setRequestHeader("Content-Type", "application/octet-stream");

	// Set up events
	xhr.upload.addEventListener('loadstart', onloadstartHandler, false);
	xhr.upload.addEventListener('progress', onprogressHandler, false);
	xhr.upload.addEventListener('load', onloadHandler, false);
	xhr.addEventListener('readystatechange', onreadystatechangeHandler, false);

	var result = document.getElementById('result');
	result.innerHTML = "";

	xhr.send(file);
}

// Handle the start of the transmission
function onloadstartHandler(evt) {
	var div = document.getElementById('upload-status');
	div.innerHTML = 'Upload started!';
}

// Handle the end of the transmission
function onloadHandler(evt) {
	var div = document.getElementById('progress');
	div.innerHTML = 'Progress: 100%';

	div = document.getElementById('upload-status');
	div.innerHTML = 'Upload successful!';
}

// Handle the progress
function onprogressHandler(evt) {
	var div = document.getElementById('progress');
	var percent = (evt.loaded/evt.total*100).toFixed(0);
	div.innerHTML = 'Progress: ' + percent + '%';
}

// Handle the response from the server
function onreadystatechangeHandler(evt) {
	var status = null;

	try {
		status = evt.target.status;
	}
	catch(e) {
		return;
	}

	if (status == '200' && evt.target.responseText) {
		var obj = jQuery.parseJSON(evt.target.responseText);
		try {
			var result = document.getElementById('result');
			if (typeof obj.success === 'undefined')
				result.innerHTML = 'Upgrade status: <strong style="color:red;">Failed</strong>';
			else
				result.innerHTML = 'Upgrade status: <strong style="color:green;">Success</strong>';
		} catch(err) {}
	}
}
