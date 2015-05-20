/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 * This is sample application to demonstrate power save state of mc200 with
 * I2C mdev layer.
 * Specification of I2C1 pins are:
 *| Pin No(mc200) | Use     |
 *---------------------------
 *|   IO4         | SDA     |
 *|   IO5         | SCL     |
 *|	  I40		  | wakeup	|
 * --------------------------
 * Connect IO4 and IO5 pin of i2c-master device to IO4 and IO5 pin
 * of i2c-slave device.
 * Connecte wakeup pin(I40) of i2c-master to WAKEUP0 of i2c-slave
 */

/*
 * Simple Application that uses Power Management Framework.
 *
 * This application demostrates the use case where the processor enters low
 * power mode, while idle.
 * In this demo two mc200 devices interact over I2C and enable the processor
 * to enter into power save mode (specifically PM2) opportunistically.
 *
 * Description:
 *
 * This application uses APIS exposed by MDEV layer of I2C driver.
 * In this application mc200 is configured in master mode.
 * It iteratively sends byte data (command) from the slave and receives 16
 * bytes data as response to the commmand. The response depends on the command
 * sent to the slave device. Processor enters low power mode (PM2) when
 * it is idle.
 * Master device ensures that slave device is woken up from power save mode
 * before starting the communication.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_i2c.h>
#include <pwrmgr.h>
#include <mdev_pinmux.h>
#include <mdev_gpio.h>
#include <cli.h>
#include <wmtime.h>

/*------------------Macro Definitions ------------------*/
#define BUF_LEN 16
#define wake1 "wakeup"
#define I2C_SLV_ADDR 0xC8
#define COMMAND_1 1
#define COMMAND_2 2
#define WAKEUP_PIN GPIO_40
#define HIGH 1
#define THRESHOLD_TIME 20
#define LOW 0

/*------------------Global Variable Definitions ---------*/
/* Thread handle */
static os_thread_t app_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 4096);

os_semaphore_t i2c_sem;

mdev_t *i2c1;

mdev_t *pinmux_dev, *gpio_dev;

uint8_t write_cmd;

uint8_t read_data[BUF_LEN];

uint8_t write_data[BUF_LEN];

/* This function asserts the gpio pin (HIGH) to wake up the slave */
static void wakeup_slave()
{
	if (pinmux_drv_init() != WM_SUCCESS) {
		wmprintf("Error: cannot init pinmux\n\r");
		return;
	}
	pinmux_dev = pinmux_drv_open("MDEV_PINMUX");
	pinmux_drv_setfunc(pinmux_dev, WAKEUP_PIN, PINMUX_FUNCTION_0);

	if (gpio_drv_init() != WM_SUCCESS) {
		wmprintf("Error: cannot init gpio\n\r");
		os_thread_self_complete(NULL);
	}

	gpio_dev = gpio_drv_open("MDEV_GPIO");
	gpio_drv_setdir(gpio_dev, WAKEUP_PIN, GPIO_OUTPUT);
	gpio_drv_write(gpio_dev, WAKEUP_PIN, HIGH);

}

/* This function startes communication over I2C bus */
static void i2c_master_demo(os_thread_arg_t data)
{
	wmprintf("I2C Writer Application Started\r\n");

	uint8_t write_cmd = COMMAND_1;
	int iteration_id = 1;
	int i, len = 0;

	while (1) {

		/* Enable I2C port */
		i2c_drv_enable(i2c1);

		/* Prevent entry to power save mode once the I2C
		 * communication begins
		 */

		wakelock_get("wakelock");

		wakeup_slave();

		wmprintf("write iteration: %d by master\r\n", iteration_id);

		/* There are 2 dummy commands COMMAND_1/2 which are used by
		 * master alternately.
		 */

		if (iteration_id % 2)
			/* COMMAND_1 for odd iterations */
			write_cmd = COMMAND_1;
		else
			/* COMMAND_2 for even iterations */
			write_cmd = COMMAND_2;

		/* wait for slave to be ready to receive data */
		os_thread_sleep(120);
Retry:
		/* write command over I2C bus */
		len = i2c_drv_write(i2c1, &write_cmd, 1);

		wmprintf("Sent %d byte/s command to slave"
				"[Command = %d]\r\n", len, write_cmd);

		uint32_t i2c_status = 0;

		while (1) {
			i2c_drv_get_status_bitmap(i2c1, &i2c_status);

			if (i2c_status & I2C_ERROR) {
				wmprintf("I2C Transaction aborted "
						"Possible cause:slave in PM2\r\n");
				os_thread_sleep(40);
				goto Retry;
			}
			if (i2c_status & I2C_INACTIVE)
				break;
		}

		wmprintf("Mater reading ...\r\n");

		/* Receive data over I2C bus */
		int read_bytes = i2c_drv_read(i2c1, read_data, BUF_LEN);

		wmprintf("Received %d bytes from slave\r\n", read_bytes);

		for (i = 0; i < BUF_LEN; i++)
			wmprintf("%02x ", read_data[i]);

		wmprintf("\r\n");	/* End of received data */

		/* Disable I2C port before entering power save mode */
		i2c_drv_disable(i2c1);

		/* Disable slave wakeup (Allow slave to sleep) */
		gpio_drv_write(gpio_dev, WAKEUP_PIN, LOW);

		/* Master device may enter into power save state */
		wakelock_put("wakelock");

		os_thread_sleep(1000);

		iteration_id++;

	}

	os_thread_self_complete(NULL);
}


/**
 * All application specific initialization is performed here
 */
int main(void)
{
	/* Initialize required modules */
	wmstdio_init(UART0_ID, 0);
	cli_init();
	wmtime_init();
	pm_init();

	wmprintf("I2C_Demo application Started\r\n");
	/*-------- I2C driver specific calls -------*/

	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1 is configured as master */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_SLAVEADR(I2C_SLV_ADDR >> 1));

	/* Configure device to enter power save state opportunistically */
	pm_mc200_cfg(true, PM2, THRESHOLD_TIME);

	/* Create the main application thread */
	return os_thread_create(&app_thread,	/* thread handle */
			"i2c_demo",	/* thread name */
			i2c_master_demo,	/* entry function */
			0,	/* argument */
			&app_stack,	/* stack */
			OS_PRIO_2);	/* priority - medium low */
}
