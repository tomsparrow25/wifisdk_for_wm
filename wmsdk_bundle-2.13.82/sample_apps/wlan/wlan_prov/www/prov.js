function ProvDataModel(){this.sys=null;this.scanList=null;this.state="unknown";var b=null;var e=0;var a=0;var d=false;var c=false;this.changedState=new Event(this);this.stateMachine(this,"init")}ProvDataModel.prototype={stateMachine:function(b,a){console.log("In stateMachine "+b.state+" "+a);switch(b.state){case"unknown":if(a==="init"){b.fetchSys(b,3)}else{if(a==="sysdata_updated"){if(b.sys.connection.station.configured===0){b.uuid=b.sys.uuid;b.state="unconfigured";b.changedState.notify({state:b.state})}else{if(b.sys.connection.station.configured===1){b.state="configured";b.changedState.notify({state:b.state})}}}else{if(a==="conn_err"){b.changedState.notify({state:b.state,err:"conn_err"})}}}break;case"unconfigured":if(a==="scanlist_updated"){b.changedState.notify({state:b.state,event:"scanlist_updated"})}else{if(a==="conn_err"){b.changedState.notify({state:b.state,err:"conn_err"})}else{if(a==="nw_posted"){b.state="nw_posted";b.changedState.notify({state:b.state})}}}break;case"nw_posted":if(a==="nw_post_success"){b.state="configured";b.changedState.notify({state:b.state})}else{if(a==="conn_err"){b.changedState.notify({state:b.state,err:"conn_err"})}else{if(a==="nw_post_invalid_param"){b.changedState.notify({state:b.state,err:"invalid_nw_param"});b.state="unconfigured"}}}break;case"configured":if(a==="sysdata_updated"){if(b.uuid&&(b.uuid!==b.sys.uuid)){console.log("Connection moved to different device");b.changedState.notify({state:b.state,err:"network_switched"})}else{if(b.sys.connection.station.configured===1){b.changedState.notify({state:b.state,event:"sysdata_updated"})}else{if(b.sys.connection.station.configured===0){b.state="unconfigured";b.changedState.notify({state:b.state})}}}}else{if(a==="conn_err"){b.changedState.notify({state:b.state,err:"conn_err"})}else{if(a==="reset_to_prov_done"){b.state="reset_to_prov_done";b.changedState.notify({state:b.state})}}}break;case"reset_to_prov_done":if(a==="reset_to_prov_success"){b.state="reset_to_prov_success";b.changedState.notify({state:b.state})}else{if(a==="conn_err"){b.changedState.notify({state:b.state,err:"conn_err"})}}break;case"finish":b.state="unknown";b.changedState.notify({state:b.state});break}},reinit:function(){this.sys=null;this.scanList=null;this.state="unknown";this.stateMachine(this,"init")},destroy:function(){this.sys=null;this.scanList=null},finish:function(){this.stateMachine(this,"finish")},connectionError:function(){this.stateMachine(this,"conn_err")},sendProvDoneAck:function(){var a=new Object();a.prov=new Object();a.prov.client_ack=1;$.ajax({type:"POST",url:"/sys",async:"false",data:JSON.stringify(a),contentType:"application/json",timeout:5000}).done()},fetchSys:function(f,d,a){var c=null;if(typeof a==="undefined"){a=3000}var b=function(){if(!(typeof d==="undefined")){f.failedSysGet++;if(d===f.failedSysGet){f.connectionError()}else{c=this;setTimeout(function(){$.ajax(c)},a)}}else{if(f.sysRequestAborted){return}f.connectionError()}};var e=function(g){f.failedSysGet=0;if(f.sysRequestAborted){return}f.sys=new Object();$.extend(true,f.sys,g);f.stateMachine(f,"sysdata_updated")};f.sysRequestAborted=false;$.ajax({type:"GET",url:"/sys",async:"false",dataType:"json",cache:false,timeout:5000,success:e,error:b}).done()},cancelSysRequest:function(){this.sysRequestAborted=true},fetchScanList:function(f,d,a){var c=null;if(typeof a==="undefined"){a=3000}var b=function(){if(!(typeof d==="undefined")){f.failedScanGet++;if(d===f.failedScanGet){f.connectionError()}else{c=this;setTimeout(function(){$.ajax(c)},a)}}else{if(f.scanListRequestAborted){return}f.connectionError()}};var e=function(k){console.log("!!");f.failedScanGet=0;if(f.scanListRequestAborted){return}f.scanList=new Array();for(var h=0;h<k.networks.length;h++){f.scanList[h]=new Array();for(var g=0;g<k.networks[h].length;g++){f.scanList[h][g]=k.networks[h][g]}}f.scanList.sort(function(j,i){return i[4]-j[4]});f.stateMachine(f,"scanlist_updated")};f.scanListRequestAborted=false;console.log("!");$.ajax({type:"GET",url:"/sys/scan",async:"true",dataType:"json",timeout:5000,cache:false,success:e,error:b}).done()},cancelScanListRequest:function(){this.scanListRequestAborted=true},getScanResults:function(){return this.scanList},getScanListEntry:function(a){return this.scanList[a]},resetToProvDone:function(){var c=new Object();var d=this;$.mobile.loading("show",{text:"Please wait...",textVisible:true,textonly:true,html:""});c.connection=new Object();c.connection.station=new Object();c.connection.station.configured=0;var a=function(e){d.stateMachine(d,"reset_to_prov_success")};var b=function(){d.stateMachine(d,"conn_err")};this.stateMachine(this,"reset_to_prov_done");$.ajax({type:"POST",url:"/sys",data:JSON.stringify(c),async:"false",dataType:"json",contentType:"application/json",timeout:5000,success:a,error:b}).done()},setNetwork:function(c){var d=this;var b=function(g,e,f){d.stateMachine(d,"conn_err")};var a=function(e){if(e.success===0){d.stateMachine(d,"nw_post_success")}else{d.stateMachine(d,"nw_post_invalid_param")}};$.ajax({type:"POST",url:"/sys/network",data:JSON.stringify(c),async:"false",dataType:"json",contentType:"application/json",timeout:5000,success:a,error:b}).done();this.stateMachine(this,"nw_posted")}};function ProvDataView(e,c,b,g,i){this._model=e;this.pageContent=c;this.pageHeaderTitle=b;this.pageHeaderHomeBtn=g;this.pageHeaderBackBtn=i;var f=this;var d=null;var a=null;var h=null;this.pageHeaderHomeBtn.hide();this.pageHeaderBackBtn.hide();this.pageHeaderTitle.text("Please wait...");this.backToScanList=function(j,k){f.selectedNw=null;f.autoRefreshScanList(5000);f.render({state:"unconfigured",event:"scanlist_updated"})};this.submit_clicked=function(){var j=new Object();$.mobile.loading("show",{text:"Please wait...",textVisible:true,textonly:true,html:""});if(f.selectedNw[2]===1||f.selectedNw[2]===3||f.selectedNw[2]===4||f.selectedNw[2]===5){if($("#show_pass").is(":checked")===true){j.key=$("#wpa_pass_plain").val()}else{j.key=$("#wpa_pass_crypt").val()}}j.ssid=f.selectedNw[0];j.security=f.selectedNw[2];j.channel=f.selectedNw[3];j.ip=1;f._model.setNetwork(j)};this.scanEntrySelect=function(j){f.cancelAutoRefreshScanList();f.selectedNw=f._model.getScanListEntry(j.data.index);f.renderSelectNetwork()};this.resetToProvIntended=function(){f.renderResetToProv()};this.resetToProvDone=function(){f._model.resetToProvDone()};this.backFromResetToProv=function(){f.render({state:"configured"})};this.backToSelectedNetwork=function(){f.renderSelectNetwork()};this.show=function(j){this._model.changedState.attach(function(l,k){f.render(k)});if(j==="reset_to_prov"){this.renderResetToProv(j)}else{this.pageHeaderTitle.text("Please wait...");this.pageContent.html("");this._model.reinit();this._model.fetchSys(this._model,3)}return this};this.autoRefreshSys=function(j){f._model.fetchSys(f._model,5);f.sysTimer=setInterval(f._model.fetchSys,j,f._model,5)}}ProvDataView.prototype={render:function(a){console.log("Render: "+a.state+" Er:"+a.err+" Ev:"+a.event);$.mobile.loading("hide");switch(a.state){case"unknown":if(a.err==="conn_err"){this.renderConnError()}break;case"unconfigured":if(typeof a.event==="undefined"&&typeof a.err==="undefined"){this.autoRefreshScanList(5000)}else{if(a.event==="scanlist_updated"){this.renderScanResults()}else{if(a.err==="conn_err"){this.renderConnError()}}}break;case"nw_posted":if(a.err==="invalid_nw_param"){this.renderInvalidNetworkParam()}else{this.pageHeaderHomeBtn.hide();this.pageHeaderBackBtn.hide();this.pageContent.html("");this.pageHeaderTitle.text("Configuring...");$.mobile.loading("show",{text:"Please wait...",textVisible:true,textonly:true,html:""})}break;case"configured":if(a.err==="network_switched"){this.renderConnError()}else{if(a.err==="conn_err"){this.renderConnLostAfterProv()}else{if(this._model.sys.connection.station.status===1||this._model.sys.connection.station.status===0){if(a.event==="sysdata_updated"){this.renderConnecting()}else{setTimeout(this.autoRefreshSys,1000,5000)}}else{if(this._model.sys.connection.station.status===2){this._model.sendProvDoneAck();this.renderConnected()}}}}break;case"reset_to_prov_done":if(a.err==="conn_err"){this.renderConnError()}break;case"reset_to_prov_success":this.renderResetToProvSuccess();break}return},hide:function(){this._model.finish();this.cancelAutoRefreshScanList();this.cancelAutoRefreshSys();this._this=null;$.mobile.loading("hide");this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.hide();this.pageContent.html("");this.pageHeaderTitle.text("")},get_img:function(b,c,e){var d=b-c;var f,a;if(d>=40){f=3}else{if(d>=25&&d<40){f=2}else{if(d>=15&&d<25){f=1}else{f=0}}}a="signal"+f;if(e){a+="L"}a+=".png";return a},get_security:function(a){if(a===0){return"Unsecured open network"}else{if(a===1){return"WEP secured network"}else{if(a===3){return"WPA secured network"}else{if(a===4){return"WPA2 secured network"}else{if(a===5){return"WPA/WPA2 Mixed secured network"}else{return"Invalid security"}}}}}},renderInvalidNetworkParam:function(){this.pageHeaderHomeBtn.hide();this.pageContent.html("");this.pageHeaderTitle.text("Error");this.pageHeaderBackBtn.unbind("click");this.pageHeaderBackBtn.bind("click",this.backToSelectedNetwork);this.pageHeaderBackBtn.show();this.pageContent.append($("<h3/>").append("Incorrect configuration parameters specified. Please retry."))},renderConnError:function(){this.pageHeaderBackBtn.hide();this.pageContent.html("");this.pageHeaderTitle.text("Error");this.pageHeaderHomeBtn.show();this.pageContent.append($("<h3/>").append("The connection to the device has been lost. Please re-connect to the device network and reload the page to restart the provisioning"))},renderConnLostAfterProv:function(){this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.show();this.pageContent.html("");this.pageHeaderTitle.text("Success");this.pageHeaderHomeBtn.show();this.pageContent.append($("<h3/>").append("The device has been configured with provided settings. However this client has lost the connectivity with the device.  Please reconnect to the device or home network and try to reload this page. Please check status indicators on the device to check connectivity."))},renderConnecting:function(){this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.show();this.pageContent.html("");this.pageHeaderTitle.text("Success");console.log(this._model.sys);var a=this._model.sys.connection.station.status;var c=this._model.sys.connection.station.failure;var b=this._model.sys.connection.station.failure_cnt;console.log(a+" "+c+" "+b);if(a===1){if(typeof c==="undefined"||typeof b==="undefined"){this.pageContent.append($("<h3/>").append("The device is configured with provided settings. The device is trying to connect to configured network."));$.mobile.loading("show",{text:"Please wait...",textVisible:true,textonly:true,html:""})}else{this.pageContent.append($("<h3/>").append("The device is configured with provided settings. However device can't connect to configured network."));if(c==="auth_failed"){this.pageContent.append($("<p/>").append("Reason: Authentication failure").append($("<p/>").append("Number of attempts:"+b)))}else{if(c==="network_not_found"){this.pageContent.append($("<p/>").append("Reason: Network not found").append($("<p/>").append("Number of attempts:"+b)))}}this.pageContent.append($("<h3/>").append("Click ").append($("<a/>",{href:"#",id:"reset_prov"}).append("here")).append(" to reset to provisioning mode."));$("#reset_prov").bind("click",this.resetToProvIntended);$.mobile.loading("hide")}}},doReinit:function(a){a._model.reinit()},renderResetToProvSuccess:function(){this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.show();this.pageContent.html("");this.pageHeaderTitle.text("Success");if(this._model.sys["interface"]==="station"){this.pageContent.append($("<h3/>").append("The device has been successfully reset to provisioning mode. Please reconnect to the device network and refresh."))}else{if(this._model.sys["interface"]==="uap"){this.pageContent.append($("<h3/>").append("The device has been successfully reset to provisioning mode. You shall be automatically redirected to select the network."));$.mobile.loading("show",{text:"Please wait...",textVisible:true,textonly:true,html:""});setTimeout(this.doReinit,5000,this)}}},renderResetToProv:function(a){var b=this;this.pageHeaderTitle.text("Reset to Provisioning");$.mobile.loading("hide");if(a==="reset_to_prov"){this.pageHeaderHomeBtn.show();this.pageHeaderBackBtn.hide()}else{this.pageHeaderHomeBtn.hide();this.pageHeaderBackBtn.unbind("click");this.pageHeaderBackBtn.bind("click",this.backFromResetToProv);this.pageHeaderBackBtn.show()}this.pageContent.html("");this.cancelAutoRefreshSys();this.pageContent.append($("<div/>",{"class":"ui-grid-a"}).append($("<div/>",{"class":"ui-block-a"}).append($("<a/>",{href:"#","data-role":"button",id:"reset-cancel-btn","data-theme":"c"}).append("Cancel"))).append($("<div/>",{"class":"ui-block-b"}).append($("<a/>",{href:"#","data-role":"button",id:"reset-to-prov-btn"}).append("Reset"))));this.pageContent.trigger("create");if(a==="reset_to_prov"){$("#reset-cancel-btn").bind("click",function(){b.pageHeaderHomeBtn.trigger("click")})}else{$("#reset-cancel-btn").bind("click",this.backFromResetToProv)}$("#reset-to-prov-btn").bind("click",this.resetToProvDone)},renderConnected:function(){this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.hide();this.pageContent.html("");var a=this._model.sys.connection.station.ssid;this.pageHeaderHomeBtn.show();this.cancelAutoRefreshSys();this.pageHeaderTitle.text("Success");this.pageContent.append($("<h3/>").append('The device is configured and connected to "'+a+'".'))},autoRefreshScanList:function(a){this._model.fetchScanList(this._model,5);this.scanListTimer=setInterval(this._model.fetchScanList,a,this._model,5)},cancelAutoRefreshScanList:function(){if(this.scanListTimer!==null){clearInterval(this.scanListTimer);this._model.cancelScanListRequest();this.scanListTimer=null}},cancelAutoRefreshSys:function(){if(this.sysTimer){clearInterval(this.sysTimer);this._model.cancelSysRequest();this.sysTimer=null}},renderScanResults:function(){this.pageHeaderBackBtn.hide();this.pageHeaderHomeBtn.hide();this.pageContent.html("");this.pageHeaderTitle.text("Provisioning");this.pageHeaderHomeBtn.show();var d=$("<ul/>",{"data-role":"listview","data-inset":true,id:"my-listview"});this.pageContent.append(d);var b=this._model.getScanResults();for(var c=0;c<b.length;c++){var a=this.get_img(b[c][4],b[c][5],b[c][2]);d.append($("<li/>",{"data-icon":false,"data-theme":"c"}).append($("<a/>",{href:"#","data-rel":"dialog",id:"scanEntry"+c}).append($("<p/>",{"class":"my_icon_wrapper"}).append($("<img/>",{src:""+a}))).append($("<h3/>").append(b[c][0])).append($("<p/>",{"class":"ui-li-desc"}).append(this.get_security(b[c][2])))));$("#scanEntry"+c).bind("click",{index:c},this.scanEntrySelect)}$("#my-listview").listview().listview("refresh")},checkbox_show_pass_changed:function(){if($("#show_pass").is(":checked")===true){$("#wpa_pass_plain").val($("#wpa_pass_crypt").val());$("#div_wpa_pass_crypt").hide();$("#div_wpa_pass_plain").show()}else{$("#wpa_pass_crypt").val($("#wpa_pass_plain").val());$("#div_wpa_pass_plain").hide();$("#div_wpa_pass_crypt").show()}},getNetworkData:function(){var b=this._model.scanList[this._model.selectedNetworkIndex];var a=new Object();if(b[2]===3||b[2]===4){if($("#show_pass").is(":checked")===true){a.key=$("#wpa_pass_plain").val()}else{a.key=$("#wpa_pass_crypt").val()}}a.ssid=b[0];a.security=b[2];a.ip=1;return a},renderSelectNetwork:function(){this.pageHeaderHomeBtn.hide();this.pageHeaderBackBtn.unbind("click");this.pageHeaderBackBtn.bind("click",this.backToScanList);this.pageHeaderBackBtn.show();this.pageContent.html("");console.log(this);var a=this.selectedNw;this.pageHeaderTitle.text(a[0]);if(a[2]===1||a[2]===3||a[2]===4||a[2]===5){var b;if(a[2]===3||a[2]===4||a[2]===5){this.pageContent.append($("<label/>",{"for":"basic"}).append("Passphrase"));b="Passphrase"}else{this.pageContent.append($("<label/>",{"for":"basic"}).append("WEP Key"));b="WEP Key"}this.pageContent.append($("<div/>",{id:"div_wpa_pass_plain","class":"ui-hide-label"}).append($("<input/>",{type:"text",id:"wpa_pass_plain",value:"",placeholder:b})));this.pageContent.append($("<div/>",{id:"div_wpa_pass_crypt","class":"ui-hide-label"}).append($("<input/>",{type:"password",id:"wpa_pass_crypt",value:"",placeholder:b})));this.pageContent.append($("<input/>",{type:"checkbox",id:"show_pass","data-mini":"true","class":"custom","data-theme":"c"}));this.pageContent.append($("<label/>",{"for":"show_pass"}).append("Unmask "+b))}this.pageContent.append($("<div/>",{"class":"ui-grid-a"}).append($("<div/>",{"class":"ui-block-a"}).append($("<a/>",{href:"#","data-role":"button",id:"cancel-btn","data-theme":"c"}).append("Cancel"))).append($("<div/>",{"class":"ui-block-b"}).append($("<a/>",{href:"#","data-role":"button",id:"submit-btn"}).append("Connect"))));this.pageContent.trigger("create");$("#div_wpa_pass_plain").hide();$("#show_pass").bind("change",this.checkbox_show_pass_changed);$("#cancel-btn").bind("click",this.backToScanList);$("#submit-btn").bind("click",this.submit_clicked)}};var prov_init=function(){return new ProvDataModel()};var prov_show=function(a,b){return new ProvDataView(a,$("#page_content"),$("#header #title"),$("#header #home"),$("#header #back")).show(b)};