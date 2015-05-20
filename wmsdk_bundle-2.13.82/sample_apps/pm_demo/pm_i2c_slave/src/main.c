
/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 * This is sample application to demonstrate power save state of mc200 with
 * I2C mdev layer.
 * as receiver in slave mode.
 * Specification of I2C1 pins are:
 *| Pin No(mc200) | Use     |
 *---------------------------
 *|   IO4         | SDA     |
 *|   IO5         | SCL     |
 * --------------------------
 * Connect IO4 and IO5 pin of i2c-master device to IO4 and IO5 pin
 * of i2c-slave device.
 * Connecte a wakeup pin from i2c-master to WAKEUP0 of i2c-slave

 */
/*
 * Simple Application that uses Power Management framework with I2C driver.
 *
 * Summary:
 * This application demostrates the use case where the processor enters low
 * power mode, while idle.
 * In this demo two mc200 devices interact over I2C and enable the processor
 * to enter into power save mode (specifically PM2) opportunistically.
 *
 * Description:
 *
 * This application uses APIS exposed by MDEV layer of I2C driver.
 * In this application mc200 is configured in slave mode.
 * It iteratively receives 1 byte data (command) from the master and sends 16
 * bytes data as response to the master. The response depends on the command
 * received by the slave device. Processor enters low power mode (PM2) when it
 * is idle.
 * Master device ensures that slave device is woken up from power save mode
 * before starting the communication.
 */

#include <mdev_i2c.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <pwrmgr.h>
#include <wmtime.h>
#include <mdev_pinmux.h>
#include <mdev_gpio.h>
#include <cli.h>

/*------------------Macro Definitions ------------------*/
#define BUF_LEN 16
#define WAKEUP "wakeup"
#define WAKEUP_SEM "wakeup-sem"
#define I2C_SLV_ADDR 0xC8
#define THRESHOLD_TIME 10

/*------------------Global Variable Definitions ---------*/
/* Thread handle */
static os_thread_t app_thread_read;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack_read, 4096);

os_semaphore_t i2c_sem;

mdev_t *i2c1;

mdev_t *pinmux_mdev, *gpio_mdev;

uint8_t read_data;

uint8_t write_cmd[BUF_LEN];

uint8_t write_data[BUF_LEN];

/* This function receives data from I2C bus */
static void i2c_slave_demo(os_thread_arg_t data)
{
	wmprintf("I2C Reader Application Started\r\n");
	int cnt = 0;
	uint8_t comp_data = 1;

	/* Fill the buffer to be sent to master */
	for (cnt = 0; cnt < BUF_LEN; cnt++) {
		/* if master command = 1 */
		write_data[cnt] = cnt + 1;

		/* if master command != 1 */
		write_cmd[cnt] = cnt + 5;
	}
	cnt = 1;
	while (cnt) {

		os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);

		/* enable I2C port */
		i2c_drv_enable(i2c1);

		int len;

		/* When a previously disabled I2C port is enabled it takes some
		 * time for FIFO configuration and init. Wait for first I2C
		 * read till the time-out period expires
		 */
		int time_out = 50000;

		while (time_out > 0) {
			uint32_t time_start = os_get_timestamp();
			len = i2c_drv_read(i2c1, &read_data, 1);

			/* If i2c_drv_read is successfull before time-out period
			 * expires, then continue further.
			 */
			if (len) {
				break;
			}
			uint32_t time_end = os_get_timestamp();
			time_out = time_out - (time_end - time_start);
		}

		if (len) {
			wmprintf("Read iteration:%d\r\n", cnt);

			wmprintf
			    ("Received %d bytes from master, Command = %d\r\n",
			     len, read_data);

		/* check the received command to send apppropriate response */
			if (read_data == comp_data) {
				int sent_bytes =
				    i2c_drv_write(i2c1, write_data, BUF_LEN);
				wmprintf("Sent %d bytes to master\r\n",
					 sent_bytes);

				/* Check I2C status */
				uint32_t i2c_status = 0;
				while (1) {
					i2c_drv_get_status_bitmap(i2c1,
								  &i2c_status);
					if (i2c_status == I2C_ERROR) {
						wmprintf("Error in"
								"I2C write\r\n");
						break;
					}
					if (i2c_status & I2C_INACTIVE)
						break;

				}
			} else {

				int sent_bytes =
				    i2c_drv_write(i2c1, write_cmd, BUF_LEN);
				wmprintf("Sent %d bytes to master\r\n",
					 sent_bytes);

				/* Check I2C status */
				uint32_t i2c_status = 0;
				while (1) {
					i2c_drv_get_status_bitmap(i2c1,
								  &i2c_status);
					if (i2c_status == I2C_ERROR) {
						wmprintf
						    ("Error in I2C write\r\n");
						break;
					}
					if (i2c_status & I2C_INACTIVE)
						break;
				}
			}
		}

		/* Disable I2C port before going to PM2 */
		i2c_drv_disable(i2c1);

		/* Slave device may enter into power save state */
		wakelock_put(WAKEUP);

		cnt++;		/*increment iteration count */
	}

}


static void wk_pin_falling_edge_cb(void)
{
	pinmux_drv_setfunc(pinmux_mdev, GPIO_25, PINMUX_FUNCTION_0);
	gpio_drv_set_cb(gpio_mdev, GPIO_25, GPIO_INT_FALLING_EDGE,
					NULL);
}

void app_ps_cb(power_save_event_t event, void *data)
{
	/* unblock the thread waiting on semaphore only on external pin
	 * wakeup.
	 * The thread that is unblocked does i2c read/write operations*/
	if ((pm_wakeup_source() == EGPIO0)) {

		/* Prevent entry to power save mode once device wakes up by
		 * the external GPIO pin (i.e. woken up by I2C master device);
		 * till the I2C communication with master is complete.
		 */

		wakelock_get(WAKEUP);

		pinmux_drv_setfunc(pinmux_mdev, GPIO_25, GPIO25_GPIO25);
		gpio_drv_set_cb(gpio_mdev, GPIO_25, GPIO_INT_FALLING_EDGE,
						wk_pin_falling_edge_cb);
		os_semaphore_put(&i2c_sem);
	}
}
/**
* All application specific initialization is performed here
*/
int main(void)
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("I2C_Demo slave application Started\r\n");

	wmtime_init();
	cli_init();
	pm_init();
	/*-------- I2C driver specific calls -------*/
	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1_PORT is configured as slave */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_DEVICE_SLAVE
			    | I2C_SLAVEADR(I2C_SLV_ADDR >> 1));

	const char *pinmux_name = "MDEV_PINMUX";
	const char *gpio_name = "MDEV_GPIO";

	 /* pinmux initialization */
	if (pinmux_drv_init() != WM_SUCCESS) {
		wmprintf("Error: cannot init pinmux\n\r");
		return -1;
	}

	pinmux_mdev = pinmux_drv_open(pinmux_name);

	/* gpio initialization */
	if (gpio_drv_init() != WM_SUCCESS) {
		wmprintf("Error: cannot init gpio\n\r");
		return -1;
	}

	gpio_mdev = gpio_drv_open(gpio_name);

	pm_mc200_cfg(true, PM2, THRESHOLD_TIME);

	/* i2c_sem semaphore is used for synchronization between I2C
	 * transactions and power save state of the processor.
	 * The thread blocked on this semaphore begins when the
	 * device is woken up. If the processor has cycles over,
	 * which it is idle has cycles over which it is idle,
	 * it enters power save mode.
	 */

	int status = os_semaphore_create(&i2c_sem, WAKEUP_SEM);
	if (status != WM_SUCCESS) {
		wmprintf("Unable to create sem\r\n");
		return 0;
	 }
	os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);

	/* Register a callback that is executed on PM2 exit.
	 * It unblocks the thread blocked on the semaphore
	 * */

	pm_register_cb(ACTION_EXIT_PM2, app_ps_cb, NULL);

	/* Create the main application thread */
	return os_thread_create(&app_thread_read,	/* thread handle */
			"i2c_demo_rd",	/* thread name */
			i2c_slave_demo,	/* entry function */
			0,	/* argument */
			&app_stack_read,	/* stack */
			OS_PRIO_3);	/* priority - medium low */

}
