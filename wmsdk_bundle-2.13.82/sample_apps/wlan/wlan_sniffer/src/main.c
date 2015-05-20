/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple sniffer application
 *
 * Summary:
 *
 * This application configures the Wi-Fi firmware to act as a
 * sniffer. Important information about all the frames received is printed on
 * the console.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * The application starts the sniffer mode of the Wi-Fi module. A sniffer
 * callback is registered that gets called whenever a frame is received from the
 * firmware.
 *
 * The sniffer callback prints important information about the beacon or data
 * packets that are received.
 *
 */

#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <cli_utils.h>
#include <wlan.h>
#include <psm.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <httpd.h>

/*-----------------------Global declarations----------------------*/

char ssid[33];

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 *
 */
void appln_critical_error_handler(void *data)
{
	while (1)
		;
	/* do nothing -- stall */
}

void print_frame_info(const wlan_frame_t *frame)
{
	if (frame->frame_type == BEACON) {
		wmprintf("********* Beacon Info *********");
		wmprintf("\r\nType: 0x%x", frame->frame_type);
		wmprintf("\r\nFrame Control flags: 0x%x",
				frame->frame_data.beacon_info.frame_ctrl_flags);
		wmprintf("\r\nSequence Number: %d",
				wlan_get_seq_num(frame->frame_data.
				beacon_info.seq_frag_num));
		wmprintf("\r\nFragmentation Number: %d",
				wlan_get_frag_num(frame->frame_data.
				beacon_info.seq_frag_num));
		snprintf(ssid, frame->frame_data.beacon_info.ssid_len + 1,
				"%s", frame->frame_data.beacon_info.ssid);
		wmprintf("\r\nSSID: %s", ssid);
		wmprintf("\r\n*******************************\r\n");
	} else if (frame->frame_type == DATA) {
		if (frame->frame_data.data_info.frame_ctrl_flags & 0x01) {
			wmprintf("********* Data Packet Info *********");
			if (frame->frame_data.data_info.frame_ctrl_flags & 0x08)
				wmprintf("\r\nThis is a retransmission\r\n");
			wmprintf("\r\nType: 0x%x", frame->frame_type);
			wmprintf("\r\nFrame Control flags: 0x%x",
				frame->frame_data.data_info.frame_ctrl_flags);
			wmprintf("\r\nBSSID: ");
			print_mac(frame->frame_data.data_info.bssid);
			wmprintf("\r\nSource: ");
			print_mac(frame->frame_data.data_info.src);
			wmprintf("\r\nDestination: ");
			print_mac(frame->frame_data.data_info.dest);
			wmprintf("\r\nSequence Number: %d",
				wlan_get_seq_num(frame->frame_data.
				data_info.seq_frag_num));
			wmprintf("\r\nFragmentation Number: %d",
				wlan_get_frag_num(frame->frame_data.
				data_info.seq_frag_num));
			wmprintf("\r\nQoS Control Flags: 0x%x",
				frame->frame_data.data_info.qos_ctrl);
			wmprintf("\r\n*******************************\r\n");
		}
	}
}

/** This Sniffer callback is called from a thread with small stack size,
 * So do minimal memory allocations for correct behaviour.
 */
void sniffer_cb(const wlan_frame_t *frame, const uint16_t len)
{
	if (frame) {
		print_frame_info(frame);
	}
}

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 * The app-framework tells the handler whether there is
 * valid network information stored in persistent memory.
 *
 * The handler can then chose to connect to the network.
 *
 * We ignore the data and just start a Micro-AP network
 * with DHCP service. This will allow a client device
 * to connect to us and receive a valid IP address via
 * DHCP.
 */
void event_wlan_init_done(void *data)
{
	int ret;
	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	ret = wlan_iw_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_iw_init failed");

	/** Filter flags:
	 *		Bit[0]: Enable/disable management frame receive,
	 *		Bit[1]: Enable/disable control frame receive,
	 *		Bit[2]: Enable/disable data frame receive.
	 *
	 * radio_type:
	 *		Band configuration type
	 *		Bits[7:6]: Reserved (set to 0)
	 *		Bits[5:4]: Secondary channel offset
	 *		     00 = no secondary channel
	 *		     01 = secondary channel is above primary channel
	 *		     10 = reserved
	 *		     11 = secondary channel is below primary channel
	 *		Bits[3:2]: Channel width
	 *		     00 = 20 MHz channel width
	 *		     01, 10 and 11 = reserved
	 *		Bits[1:0]: Band information
	 *		     00 = 2.4 GHz band
	 *		     01 = 5 GHz band
	 *		     10 and 11 = reserved
	 *
	 * channel:
	 * 		Channel number in respective band.
	 *
	 * To sniff packets in 40 MHz channel bandwidth, specify secondary
	 * channel offset above/below in radio type.
	 *
	 * wlan_sniffer_start(4, 17, 40, sniffer_cb);
	 *
	 * The above call with start sniffer for data frames on channel
	 * number 40 and 44 in 5 GHz band, as secondary channel specified
	 * is above primary channel.
	 *
	 * Following call starts WLAN sniffer capturing control, management
	 * and data frames on channel 8 of 2.4 GHz band.
	 *
	 * All captured frames are delivered to registered sniffer callback
	 * sniffer_cb.
	 *
	 */
	ret = wlan_sniffer_start(0x07, 0x00, 0x08, sniffer_cb);
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_sniffer_start failed");
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Initialize CLI Commands
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
				appln_critical_error_handler((void *) -WM_FAIL);
	}
	return 0;
}
