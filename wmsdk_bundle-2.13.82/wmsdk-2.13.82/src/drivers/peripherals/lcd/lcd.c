/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* lcd.c
 * This file contains generic LCD functions
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_i2c.h>
#include <peripherals/lcd.h>

#include <mdev_gpio.h>
#include <mdev_pinmux.h>
#include <pwrmgr.h>
#include <board.h>

#define I2C_LCD_ADDR		0x7c

/* The LCD Screen operates in one of the two modes, configuration and
 * data. In configuration mode various parameters specific to the LCD
 * screen are set while in data mode data is written in the RAM of the
 * screen which is then displayed.
 * The device stays in this mode until a I2C_STOP is received. */
#define LCD_CMD_CONFIG_MODE             0x00
#define LCD_CMD_DATA_MODE               0x41

/*
 * LCD Line 1 address :00h
 * LCD Line 2 address :40h
 * The first byte we send is the configuration/data mode byte, thus in the
 * buffer line addresses are off by 1
 */
#define LCD_LINE1_ADDR    1
#define LCD_LINE2_ADDR    41

/* Number of characters on 1 LCD line */
#define LCD_LINE_LENGTH   16

#define write_line1(buff, data_src)    \
	strncpy(buff + LCD_LINE1_ADDR, data_src,\
		LCD_LINE2_ADDR - LCD_LINE1_ADDR - 1);

#define write_line2(buff, data_src)    \
	strncpy(buff + LCD_LINE2_ADDR, data_src,\
		sizeof(buff) - LCD_LINE2_ADDR - 1); \

#define end_of_line2(buff) (LCD_LINE2_ADDR + strlen(buff + LCD_LINE2_ADDR))

static mdev_t MDEV_lcd;
static const char *MDEV_NAME_lcd = "MDEV_LCD";
static os_mutex_t lcd_mutex;

mdev_t *gpio_dev, *pinmux_dev;

/* Initialise the LCD configuration */
static void init_lcd(void)
{
	uint8_t buf[20];
	mdev_t *i2c_dev;

	pinmux_dev = pinmux_drv_open("MDEV_PINMUX");
	if (pinmux_dev == NULL) {
		lcd_d("Pinmux driver init is required before open");
		return;
	}

	gpio_dev = gpio_drv_open("MDEV_GPIO");
	if (gpio_dev == NULL) {
		lcd_d("GPIO driver init is required before open");
		return;
	}

	board_lcd_reset();

	i2c_dev = i2c_drv_open(MDEV_lcd.private_data,
			I2C_SLAVEADR(I2C_LCD_ADDR >> 1));
	if (i2c_dev == NULL) {
		lcd_d("I2C driver init is required before open");
		return;
	}

	/* The LCD Screen operates in one of the two modes, configuration and
	 * data. In configuration mode various parameters specific to the LCD
	 * screen are set while in data mode data is written in the RAM of the
	 * screen which is then displayed. Set the configuration mode first.
	 * The device stays in this mode until a I2C_STOP is received. */
	buf[0] = LCD_CMD_CONFIG_MODE;	/* Com send */
	/* Internal OSC Frequency
	 * Bits: 0 0 0 1 BS F2 F1 F0
	 * BS: 1 -> 1/4 bias, 0 -> 1/5 bias
	 * F2~0: internal oscillator frequency
	 */
	buf[1] = 0x14;
	/* Function Set
	 * Bits: 0 0 1 DL N DH 0 IS
	 * DL: interface data 1 -> 8 bits, 0 -> 4 bits
	 * N: number of lines 1 -> 2 lines, 0 -> 1 line
	 * DH: enable double height font
	 * IS: Instruction set select 0/1
	 */
	buf[2] = 0x39;
	/* Contrast Set
	 * Bits: 0 1 1 1 C3 C2 C1 C0
	 * C3~0: Last 4 bits of the contrast value
	 */
	buf[3] = 0x78;
	/* Contrast Set
	 * Bits: 0 1 0 1 I B C5 C4
	 * I: ICON display on/off
	 * B: booster circuit on/off
	 * C5~4: Higher order 2 bits of the contrast value
	 */
	buf[4] = 0x55;
	/* Follower Control
	 * Bits: 0 1 1 0 F RAB2 RAB1 RAB0
	 * F : Follower Ckt on/off
	 * RAB2~0: follower amplifier ratio
	 */
	buf[5] = 0x6d;
	/* Display on/off
	 * Bits: 0 0 0 0 1 D C B
	 * D: display on/off
	 * C: cursor on/off
	 * B: cursor position on/off
	 */
	buf[6] = 0x0c;
	/* Clear Display
	 * Bits: 0 0 0 0 0 0 0 1
	 * Set DDRAM address to 00H, and clear display
	 */
	buf[7] = 0x01;
	/* Entry mode
	 * Bits: 0 0 0 0 0 1 I/D S
	 * I/D: Cursor move direction inc/dec
	 * S: Display shift on/off
	 */
	buf[8] = 0x06;

	os_thread_sleep(os_msec_to_ticks(35));
	i2c_drv_write(i2c_dev, buf, 9);

	i2c_drv_close(i2c_dev);
}

static char lcd_buff[0x67 + 1] = { 0, };

static void __write_to_lcd(mdev_t *dev, const char *line1, const char *line2)
{
	mdev_t *i2c_dev;
	int temp;

	memset(lcd_buff, 0, sizeof(lcd_buff));

	os_thread_sleep(os_msec_to_ticks(35));
	i2c_dev = i2c_drv_open(dev->private_data,
			I2C_SLAVEADR(I2C_LCD_ADDR >> 1));
	if (i2c_dev == NULL) {
		lcd_d("I2C driver init is required before open");
		return;
	}

	lcd_buff[0] = LCD_CMD_CONFIG_MODE;

	/* Set DDRAM Address to line 1
	 * Bits: 1 AC6 AC5 AC4 AC3 AC2 AC1 AC0
	 * AC6-0: The address to be written to:
	 * 00H: line 1
	 * 40H: line 2
	 */
	lcd_buff[1] = 0x80;
	lcd_buff[2] = 0x06;	/* Entry mode as mentioned above */
	os_thread_sleep(os_msec_to_ticks(35));
	temp = os_enter_critical_section();
	i2c_drv_write(i2c_dev, (uint8_t *) lcd_buff, 2);
	os_exit_critical_section(temp);

	/* Write data */
	lcd_buff[0] = LCD_CMD_DATA_MODE;

	if (line1)
		write_line1(lcd_buff, line1);

	if (line2)
		write_line2(lcd_buff, line2);

	os_thread_sleep(os_msec_to_ticks(35));
	temp = os_enter_critical_section();
	i2c_drv_write(i2c_dev, (uint8_t *) lcd_buff, end_of_line2(lcd_buff));
	os_exit_critical_section(temp);
	i2c_drv_close(i2c_dev);
}

mdev_t *lcd_drv_open(const char *name)
{
	mdev_t *mdev_p = mdev_get_handle(name);

	if (mdev_p == NULL) {
		lcd_d("driver open called without registering device"
							" (%s)", name);
		return NULL;
	}

	return mdev_p;
}

int lcd_drv_close(mdev_t *dev)
{
	return 0;
}

#define WL_ID_LCD_WRITE    "lcd_driver_write"

int lcd_drv_write(mdev_t *dev, const char *line1, const char *line2)
{
	int ret;
	ret = wakelock_get(WL_ID_LCD_WRITE);
	if (ret != WM_SUCCESS)
		return ret;

	ret = os_mutex_get(&lcd_mutex, OS_WAIT_FOREVER);
	if (ret == -WM_FAIL) {
		lcd_e("failed to get mutex");
		wakelock_put(WL_ID_LCD_WRITE);
		return ret;
	}
	__write_to_lcd(dev, line1, line2);
	os_mutex_put(&lcd_mutex);

	wakelock_put(WL_ID_LCD_WRITE);

	return 0;
}

int lcd_drv_init(I2C_ID_Type id)
{
	int ret;
	MDEV_lcd.name = MDEV_NAME_lcd;

	if (mdev_get_handle(MDEV_NAME_lcd) != NULL)
		return WM_SUCCESS;

	pinmux_drv_init();
	gpio_drv_init();
	i2c_drv_init(id);

	ret = os_mutex_create(&lcd_mutex, "lcd", OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		lcd_e("Failed to create mutex");
		return -WM_FAIL;
	}

	mdev_register(&MDEV_lcd);
	MDEV_lcd.private_data = (uint32_t) id;
	init_lcd();

	return WM_SUCCESS;
}
