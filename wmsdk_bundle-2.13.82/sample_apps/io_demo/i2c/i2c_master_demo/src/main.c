/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application which uses I2C driver.
 *
 * Summary:
 * This application demonstrates use of 88MC200 I2C peripheral in master mode.
 *
 * Description:
 * This application uses APIs exposed by MDEV layer of I2C driver.
 * In this application 88MC200 is configured in master mode.
 * It writes 16 byte (BUF_LEN) data 0x1 to 0x10(hex) on I2C bus iteratively
 * after 1 sec and reads data sent by the slave and prints it on the console.
 *
 * This is sample application to demonstrate I2C mdev layer
 * as transmitter in master mode.
 * Specification of I2C1 pins (on lk20-v3)are:
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
static os_thread_t app_thread_write;
static os_thread_t app_thread_read;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack_read, 4096);
static os_thread_stack_define(app_stack_write, 4096);

os_semaphore_t i2c_sem;

mdev_t *i2c1;

uint8_t read_data[BUF_LEN];

uint8_t write_data[BUF_LEN];

/* This function sends data on I2C bus */
static void i2c_master_write(os_thread_arg_t data)
{
	int len, i, cnt;

	wmprintf("I2C Writer Application Started\r\n");

	for (i = 0; i < BUF_LEN; i++)
		write_data[i] = i + 1;

	cnt = 0;

	while (1) {
		cnt++;
		len = i2c_drv_write(i2c1, write_data, BUF_LEN);
		wmprintf("Write iteration %d: len = %d\n\r", cnt, len);
		os_semaphore_put(&i2c_sem);

		os_thread_sleep(os_msec_to_ticks(1000));
	}
}

/* This function reads data from I2C bus */
static void i2c_master_read(os_thread_arg_t data)
{
	int len, i, cnt = 0;

	wmprintf("I2C Writer Application Started\r\n");
	i = 0;
	while (1) {
		cnt++;

		os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);
		len = i2c_drv_read(i2c1, read_data, BUF_LEN);
		wmprintf("master read iteration %d: len = %d\n\r", cnt, len);
		/* Print data on console */
		for (i = 0; i < len ; i++)
			wmprintf("%02x ", read_data[i]);
		if (len)
			wmprintf("\n\r");

		/* reset the read buffer */
		os_thread_sleep(os_msec_to_ticks(1000));
	}
}

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("I2C_Demo application Started\r\n");

	/*-------- I2C driver specific calls -------*/

	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1 is configured as master */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_SLAVEADR(I2C_SLV_ADDR >> 1));

	/* i2c_sem semaphore is used for synchronization between I2C read
	* and write transactions. i2c_drv_read is blocked on i2c_drv_write, to
	* ensure that data is read only after a write occurs.
	*/
	int status = os_semaphore_create(&i2c_sem, wake1);

	if (status != WM_SUCCESS) {
		wmprintf("Unable to create sem\r\n");
		return 0;
	}

	os_semaphore_get(&i2c_sem, OS_WAIT_FOREVER);

	/* Create the main application thread */
	os_thread_create(&app_thread_write,	/* thread handle */
			"i2c_demo",	/* thread name */
			i2c_master_write,	/* entry function */
			0,	/* argument */
			&app_stack_write,	/* stack */
			OS_PRIO_2);	/* priority - medium low */

	os_thread_create(&app_thread_read,	/* thread handle */
			"i2c_demo_rd",	/* thread name */
			i2c_master_read,	/* entry function */
			0,	/* argument */
			&app_stack_read,	/* stack */
			OS_PRIO_3);	/* priority - medium low */

	return 0;
}

