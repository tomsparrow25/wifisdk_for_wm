/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Sample Application demonstrating the use of Arrayent Cloud
 * This application communicates with Arrayent cloud once the device is
 * provisioned. Device can be provisioned using the psm CLIs as mentioned below.
 * After that, it periodically gets/updates (toggles) the state of board_led_1()
 * and board_led_2() from/to the Arrayent cloud.
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <mdev_gpio.h>
#include <arrayent/aca.h>

/* Thread handle */
static os_thread_t app_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 8 * 1024);


/*------------------Macro Definitions ------------------*/

/* Device credentials & server info as assigned by Arrayent
 * THESE VALUES ARE SHARED WITH ALL MARVELL ACA_DEMO USERS AND THEREFORE YOU MAY
 * SEE SOME CONFLICTS IF MULTIPLE USERS ARE EVALUATING THE DEMO AT THE SAME
 * TIME. PLEASE ONLY USE THE DEMO FOR A SHORT EVALUATION. FOR FURTHER
 * DEVELOPMENT PLEASE CONTACT ARRAYENT SALES.
 */

 /* Device credentials & server info as assigned by Arrayent */
#define DEVICE_NAME         "MWF004"
#define DEVICE_PASSWORD     "84"
#define PRODUCT_ID          2017
#define LOAD_BALANCER_ONE   "devkit-ds1.arrayent.com"
#define LOAD_BALANCER_TWO   "devkit-ds2.arrayent.com"
#define LOAD_BALANCER_THREE "devkit-ds3.arrayent.com"
#define DEVICE_AES_KEY      "0D4F1E4E23A6A78215341E99FB3F2687"
#define PRODUCT_AES_KEY     "658598F5F482768C137D2729EE89F034"

#define LOAD_BALANCER_PORT  80

/* Just a reasonable max for this device's receive buffers */
#define ACA_RECEIVE_MAX_SIZE    255
/* Just a reasonable max for this device's properties */
#define ACA_PROPERTY_BUF_SIZE   32
/* Just a reasonable max for this property */


#define END_CHAR        '\r'
#define INBUF_SIZE      80  /* Arbitrary max value */

/*------------------Global Variable Definitions ---------*/

/* The various properties of this specific device type which was defined by the
 * Arrayent Configurator */
char led1[] = "led1";
char led2[]  = "led2";
char button_a_property[] = "pushbutton1";
char button_b_property[] = "pushbutton2";


/* These hold the LED gpio pin numbers */
static uint32_t led_1;
static uint32_t led_2;
/* These hold each pushbutton's count, updated in the callback ISR */
volatile uint32_t pushbutton_a_count;
volatile uint32_t pushbutton_b_count;
volatile uint32_t pushbutton_a_count_prev = -1;
volatile uint32_t pushbutton_b_count_prev = -1;


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

static int arrayent_configure(void)
{
	arrayent_return_e ret;

	char  *load_balancer[] = {
		LOAD_BALANCER_ONE,
		LOAD_BALANCER_TWO,
		LOAD_BALANCER_THREE};
	arrayent_config_t   config  = {
		.product_id                 = PRODUCT_ID,
		.product_aes_key            = PRODUCT_AES_KEY,
		.load_balancer_domain_names = { load_balancer[0],
						load_balancer[1],
						load_balancer[2]},
		.load_balancer_udp_port     = LOAD_BALANCER_PORT,
		.load_balancer_tcp_port     = LOAD_BALANCER_PORT,
		.device_name                = DEVICE_NAME,
		.device_password            = DEVICE_PASSWORD,
		.device_aes_key             = DEVICE_AES_KEY
	};
	/*  Configure the product/device information for this target */
	if ((ret = ArrayentConfigure(&config)) != ARRAYENT_SUCCESS) {
		wmprintf("ArrayentConfigure Failed %d\r\n", ret);
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

static int arrayent_check_connection_status(void)
{
	arrayent_net_status_t status;
	int firsttime = 1;

	while (!(ArrayentNetStatus(&status) == ARRAYENT_SUCCESS)
		|| !(status.connected_to_server)) {
		if (firsttime) {
			wmprintf("Arrayent daemon not connected to server\r\n");
			wmprintf("Waiting for good Arrayent network "
				"status..\r\n");
			wmprintf("Arrayent network status: 0x%02X\r\n",
				status);
			firsttime = 0;
		} else
			 wmprintf(" 0x%02X\r\n", status);

		/* Arbitrary polling rate */
		os_thread_sleep(os_msec_to_ticks(800));
	}

	return WM_SUCCESS;
}

/*
 * This is the main aca demo routine with polling loop.
 * This routine assumes the board and GPIOs are configured and a
 * network connection is established.
 */
static void arrayent_demo_main(os_thread_arg_t data)
{
	char *property = NULL;
	char property_out_buf[ACA_PROPERTY_BUF_SIZE];
	char property_in_buf[ACA_PROPERTY_BUF_SIZE];

	uint32_t property_in_value = 0;
	uint32_t led_1_state = 0;
	uint32_t led_2_state = 0;
	char recv_data[ACA_RECEIVE_MAX_SIZE+1];
	uint16_t recv_data_len = ACA_RECEIVE_MAX_SIZE;

	/* Configure the ACA Daemon for contacting the
	 * Arrayent servers as this device. */
	arrayent_configure();

	/* Initialize the Arrayent layer */
	wmprintf("Initializing Arrayent daemon...\r\n");
	while (ArrayentInit() < 0) {
		wmprintf("Initializing Arrayent daemon failed!\r\n");
		wmprintf("Retrying:");
	}

	wmprintf("Arrayent daemon initialized.\r\n");

	/* Poll the Arrayent connection status until they're connected */
	arrayent_check_connection_status();
	wmprintf("\r\nConnected to Arrayent Cloud!\r\n");

	/* Drop into polling loop for button presses and
	 * Arrayent cloud events */
	do {
		/* Check Arrayent daemon connection with Arrayent cloud */
		arrayent_check_connection_status();

		/* Any pushbutton A events */
		if (pushbutton_a_count_prev != pushbutton_a_count) {
			/* Toggle LED on board, if state=on, turn it off */
			if (led_1_state)
				board_led_off(led_1);
			else
				board_led_on(led_1);
			/* Update Arrayent Utility Application
			 * about button and LED */
			property = button_a_property;
			sprintf(property_out_buf, "%d", pushbutton_a_count);
			wmprintf("Sending '%s %s' to Arrayent Utility\r\n",
				 property, property_out_buf);
			if (ArrayentSetProperty(property,
						property_out_buf) < 0) {
				wmprintf("Sending property failed\r\n");
			} else {
				property = led1;
				/* Update to new state and inform cloud */
				led_1_state ^= 1;
				sprintf(property_out_buf, "%d", led_1_state);
				wmprintf("Sending '%s %s' to"
					 "Arrayent Utility\r\n",
					 property, property_out_buf);
				if (ArrayentSetProperty(property,
							property_out_buf) < 0) {
					wmprintf("Sending property failed\r\n");
					/* Reset the state since
					 * the server didn't get it */
					led_1_state ^= 1;
				} else {
					pushbutton_a_count_prev
						= pushbutton_a_count;
				}
			}
		}

		/* Any pushbutton B events */
		if (pushbutton_b_count_prev != pushbutton_b_count) {
			/* Toggle LED on board, if state=on, turn it off */
			if (led_2_state)
				board_led_off(led_2);
			else
				board_led_on(led_2);

			/* Update Arrayent Utility Application about
			 * button and LED */
			property = button_b_property;
			sprintf(property_out_buf, "%d", pushbutton_b_count);
			wmprintf("Sending '%s %s' to Arrayent Utility\r\n",
				 property, property_out_buf);
			if (ArrayentSetProperty(property,
						property_out_buf) < 0) {
				wmprintf("Sending property failed\r\n");
			} else {
				property = led2;
				/* Update to new state and inform cloud */
				led_2_state ^= 1;
				sprintf(property_out_buf, "%d", led_2_state);
				wmprintf
					("Sending '%s %s' to Arrayent"
					 "Utility\r\n",
					  property, property_out_buf);
				if (ArrayentSetProperty(property,
							property_out_buf) < 0) {
					wmprintf("Sending property failed\r\n");
					/* Reset the state since the
					   server didn't get it */
					led_2_state ^= 1;
				} else {
					pushbutton_b_count_prev =
						pushbutton_b_count;
				}
			}
		}

		/* Check for any property updates from the Arrayent Utility
		 * Application */
		recv_data_len = ACA_RECEIVE_MAX_SIZE;
		if (ArrayentRecvProperty(recv_data, &recv_data_len, 0) ==
		    ARRAYENT_SUCCESS) {
			/* NULL Terminate the data received so that its treated
			 * like a string */
			recv_data[recv_data_len] = 0;
			wmprintf("Received '%s' from Arrayent Utility\r\n",
				 recv_data);
			sscanf(recv_data, "%s %d", property_in_buf,
			       &property_in_value);
			/* Process the various properties we know about */
			if (strcmp(property_in_buf, led2) == 0) {
				led_2_state = property_in_value;
				/* Set board LED to match the given state */
				if (led_2_state)
					board_led_on(led_2);
				else
					board_led_off(led_2);
			}
			if (strcmp(property_in_buf, led1) == 0) {
				led_1_state = property_in_value;
				/* Set board LED to match the given state */
				if (led_1_state)
					board_led_on(led_1);
				else
					board_led_off(led_1);
			}
		}
		os_thread_sleep(os_msec_to_ticks(200));
	} while (1);
	return;
}

/* This function is called when push button A is pressed*/
static void pushbutton_cb_a()
{
	/* Little debounce with previous, only count if the server
	 * has been updated */
	if (pushbutton_a_count_prev == pushbutton_a_count) {
		pushbutton_a_count++;
	}
}

/* This function is called when push button B is pressed*/
static void pushbutton_cb_b()
{
	/* Little debounce with previous, only count if the server
	 * has been updated */
	if (pushbutton_b_count_prev == pushbutton_b_count) {
		pushbutton_b_count++;
	}
}

/* Configure GPIO pins to be used as LED and push button */
static void configure_gpios()
{
	mdev_t *gpio_dev;
	uint32_t pushbutton_a;
	uint32_t pushbutton_b;

	/* Get the corresponding pin numbers using the board specific calls */
	/* also configures the gpio accordingly for LED */
	led_1 = board_led_1();
	led_2 = board_led_2();
	pushbutton_a = board_button_1();
	pushbutton_b = board_button_2();

	/* Initialize & Open handle to GPIO driver for push button
	 * configuration */
	gpio_drv_init();
	gpio_dev = gpio_drv_open("MDEV_GPIO");

	/* Register a callback for each push button interrupt */
	gpio_drv_set_cb(gpio_dev, pushbutton_a, GPIO_INT_FALLING_EDGE,
		pushbutton_cb_a);
	gpio_drv_set_cb(gpio_dev, pushbutton_b, GPIO_INT_FALLING_EDGE,
		pushbutton_cb_b);

	/* Close driver */
	gpio_drv_close(gpio_dev);
}



/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_cloud_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;

		if (i == APP_NETWORK_NOT_PROVISIONED) {
			wmprintf("\r\nPlease provision the device "
				"and then reboot it:\r\n\r\n");
			wmprintf("psm-set network ssid <ssid>\r\n");
			wmprintf("psm-set network security <security_type>"
				"\r\n");
			wmprintf("    where: security_type: 0 if open,"
				" 3 if wpa, 4 if wpa2\r\n");
			wmprintf("psm-set network passphrase <passphrase>"
				" [valid only for WPA and WPA2 security]\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf("pm-reboot\r\n\r\n");
		} else
			app_sta_start();

		break;
	case AF_EVT_NORMAL_CONNECTED:
		if (!is_cloud_started) {
			configure_gpios();
			ret = os_thread_create(&app_thread,  /* thread handle */
				"arrayent_demo_thread",/* thread name */
				arrayent_demo_main,  /* entry function */
				0,          /* argument */
				&app_stack,     /* stack */
				OS_PRIO_2);     /* priority - medium low */
			is_cloud_started = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmstdio_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return;
}

int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		appln_critical_error_handler((void *) -WM_FAIL);
	}
	return 0;
}
