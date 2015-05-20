/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application which uses I2C driver.
 *
 * Summary:
 * This application demonstrates use of 88MC200 I2C peripheral in slave mode.
 *
 * Description:
 *
 * This application uses APIs exposed by MDEV layer of I2C driver.
 * In this application 88MC200 is configured in slave mode.
 * It receives 16 bytes data (BUF_LEN) data 0x1 to 0x10(hex) from I2C master
 * and prints it on the console iteratively and sends the same data back to
 * the master. Application ensures that i2c_write occurs only after data
 * is read.
 *
 * This is sample application to demonstrate I2C mdev layer
 * as receiver in slave mode.
 * Specification of I2C1 pins are:
 *| Pin No(mc200) | Use     |
 *---------------------------
 *|   IO4         | SDA     |
 *|   IO5         | SCL     |
 * --------------------------
 * Connect IO4 and IO5 pin of transmitter to IO4 and IO5 pin
 * of receiver.
 */


#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_i2c.h>

/*------------------Macro Definitions ------------------*/
#define BUF_LEN 16
#define wake1 "wakeup"
#define I2C_SLV_ADDR 0xC8

/*------------------Global Variable Definitions ---------*/
/* Thread handle */
static os_thread_t app_thread_read;
static os_thread_t app_thread_write;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack_read, 4096);
static os_thread_stack_define(app_stack_write, 4096);


os_semaphore_t i2c_sem;

mdev_t *i2c1;

uint8_t read_data[BUF_LEN];

uint8_t write_data[BUF_LEN];

/* This function receives data from I2C bus */
static void i2c_slave_read(os_thread_arg_t data)
{
	int len, i, cnt = 0;
	wmprintf("I2C Reader Application Started\r\n");

	while (1) {
		cnt++;
Retry:
		len = i2c_drv_read(i2c1, read_data, BUF_LEN);
		if (!len) {
			goto Retry;
		}
		wmprintf("slave read iteration %d, received %d"
				" bytes from master\r\n", cnt, len);
		os_semaphore_put(&i2c_sem);

		/* Print data on console */
		for (i = 0; i < len ; i++)
			wmprintf("%02x ", read_data[i]);
		if (len)
			wmprintf("\n\r");

		os_thread_sleep(os_msec_to_ticks(1000));
	}
}

/* This function writes data on I2C bus */
static void i2c_slave_write(os_thread_arg_t data)
{
	wmprintf("I2C writer Application Started\r\n");
	int len, i, cnt = 0;
	for (i = 0; i < BUF_LEN; i++)
		write_data[i] = read_data[i];
	i = 0;
	while (1) {
		cnt++;

		os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);

		len = i2c_drv_write(i2c1, read_data, BUF_LEN);

		wmprintf("slave write iteration %d,"
				"sent %d bytes to master\r\n", cnt, len);

		os_thread_sleep(os_msec_to_ticks(1000));
	}

}


/**
* All application specific initializations are performed here.
*/
int main(void)
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("I2C_Demo slave application Started\r\n");

	/*-------- I2C driver specific calls -------*/
	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1_PORT is configured as slave */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_DEVICE_SLAVE
			    | I2C_SLAVEADR(I2C_SLV_ADDR >> 1));

	/* Create the main application thread */

	/* i2c_sem semaphore is used for synchronization between I2c read
	* and write transactions. i2c_drv_write is blocked on i2c_drv_read, to
	* ensure that data is written only after it is read.
	*/

	int status = os_semaphore_create(&i2c_sem, wake1);
	if (status != WM_SUCCESS) {
		wmprintf("Unable to create sem\r\n");
		return 0;
	 }
	os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);

	os_thread_create(&app_thread_read,	/* thread handle */
			"i2c_demo_rd",	/* thread name */
			i2c_slave_read,	/* entry function */
			0,	/* argument */
			&app_stack_read,	/* stack */
			OS_PRIO_3);	/* priority - medium low */

	os_thread_create(&app_thread_write,	/* thread handle */
			"i2c_demo_wr",	/* thread name */
			i2c_slave_write,	/* entry function */
			0,	/* argument */
			&app_stack_write,	/* stack */
			OS_PRIO_2);	/* priority - medium low */
	return 0;

}
