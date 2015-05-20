This README explains the procedure to be followed to setup wm_demo
application with Arrayent cloud.

Setup Details :

Application : wm_demo
Arrayent Cloud : 1.1 version


Steps:

1. Open wm_demo/config.mk file and enable ARRAYENT_CLOUD.
   Disable all other cloud schemes.
   Build the application.
2. Flash it using flashprog.
3. Reset the device.
4. If the device is not provisioned, do provisioning using
   either WPS or WEBUI or Android / IOS APP.
5. After provisioning is completed device will connect to AP.
6. By default the cloud is disabled.
   To enable the cloud use CLI below:
   # psm-set cloud enabled 1
7. Cloud interval is default set to 3 seconds.
   If you want to change the cloud interval use:
   # psm-set cloud interval [time in seconds]
8. Reboot the device by either pressing reset button on
   the device or using  below CLI command:
   #  pm-reboot

   Now the device connects to AP and communicates with
   the cloud server.

Cloud Setup Steps:
-----------------
1.  You can use the Arrayent Web Utility Portal to monitor
    and control individual devices without writing any
    application code.
2.  To connect to the Utility Portal go to:
    https://devkit-api.arrayent.com:8081/Utility/users/login
3.  Login using below credentials:
    Customer Account Name: JohnSmith2017
    Cusomter Account Password: marvell88
    System Account Id: 2017

4.  This will take you to the welcome page where you will find a tab
    "Monitor and Control". Clicking on that tab will take you to the Device
    status page for the devices. Select MWF002 to select wm_demo to
    see the device state.

    Following actions can be done on portal:
    a. Turn on / off LED and the changes are reflected on the device.

CAVEATS:
1. If more than 1 device is connected to
   the Arrayent cloud, it is not
   guaranteed which device will get the settings.
   So it is safe to have a single device connected
   to the Arrayent cloud.

2. After reset to prov, please reboot the device.

3. For Arrayent cloud to work, the network should allow traffic apart from
   port 80. Make sure that you change the firewall settings accordingly.


