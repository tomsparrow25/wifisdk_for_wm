/*! \file board.h
 * \brief Board Specific APIs
 *
 *  The board specific APIs are provided to help
 *  with porting the SDK onto new boards. These
 *  APIs are board specific and they need to be
 *  modified/implemented according to the actual
 *  board specifications.
 */
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmtypes.h>

/** Frequency of Main Crystal
 *
 * The MC200 processor can be clocked through
 * either of the following 3 sources:<br>
 * 1) Externally connected crystal.<br>
 * 2) Externally connected oscillator.<br>
 * 3) Internal RC oscillator.<br>
 *
 * If the board has an external crystal for
 * main cpu clocking, mention it's frequency
 * here.
 *
 *  \return Frequency of external crystal if it is
 * present, -WM_FAIL otherwise.
 */
extern int board_main_xtal();

/** Frequency of Main Oscillator
 *
 * The MC200 processor can be clocked through
 * either of the following 3 sources:<br>
 * 1) Externally connected crystal.<br>
 * 2) Externally connected oscillator.<br>
 * 3) Internal RC oscillator.<br>
 *
 * If the board has an external oscillator for
 * main cpu clocking, mention it's frequency
 * here.
 *
 *  \return Frequency of external oscillator if it is
 * present, -WM_FAIL otherwise.
 */
extern int board_main_osc();

/** Frequency at which CPU should run
 *
 * Mention the frequency at which the CPU should
 * run here. If this frequency is greater than the
 * source clock, internal PLL will be configured.<br>
 *
 * NOTE: Valid frequencies depend upon the source clock.
 * Please refer to the datasheet for further details.
 *
 *  \return Frequency at which CPU should operate.
 */
extern int board_cpu_freq();

/** 32 KHz crystal
 *
 * If 32 KHz crystal is present on the board and
 * is functional, return true here.
 *
 *  \return true if 32 KHz crystal is present and
 *  functional, false otherwise.
 */
extern int board_32k_xtal();

/** 32 KHz Oscillator
 *
 * If 32 KHz oscillator is present on the board and
 * is functional, return true here.
 *
 *  \return true if 32 KHz oscillator is present and
 *  functional, false otherwise.
 */
extern int board_32k_osc();

/** Detect the SDIO card
 *
 * If there is a mechanism to detect the SDIO card on
 * the board, use it here, or else, always return the
 * default condition.
 *
 *  \return TRUE if card is present, FALSE otherwise
 */
extern int board_card_detect();

/** Powerdown the SDIO card
 *
 * If there is a GPIO connected to PDn
 * of the SDIO card, ASSERT it in this function.
 *
 * Leave blank if not connected.
 */
extern void board_sdio_pdn();

/** Powerup the SDIO card
 *
 * If there is a GPIO connected to PDn
 * of the SDIO card, DE-ASSERT it in this function.
 *
 * Leave blank if not connected.
 */
extern void board_sdio_pwr();

/** Reset the SDIO card
 *
 * If there is a GPIO connected to RST
 * of the SDIO card, ASSERT it for some
 * duration and DE-ASSERT it again.
 *
 * Leave blank if not connected.
 *
 */
extern void board_sdio_reset();

/** Check if PDn pin of the SDIO card can be
 * controlled by HOST (MC200)
 *
 * \return TRUE if it can be controlled, FALSE otherwise
 */
extern int board_sdio_pdn_support();

/** Boot Override Pushbutton
 *
 * \return GPIO pin number connected to the
 * pushbutton to be used for Boot override mode
 * functionality or -WM_FAIL if such pushbutton
 * is not implemented on the board.
 */
extern int board_button_3();

/** Push Button Pressed
 *
 * Depending upon how the push button is
 * connected, corresponding GPIO needs to
 * be checked for either set or reset.
 *
 * If no pushbuttons are implemented on the board,
 * always return FALSE (i.e. button not pressed).
 *
 * \param[in] pin GPIO pin to be used
 * \return TRUE if pressed, FALSE otherwise
 */
extern int board_button_pressed(int pin);

/** Power On GPIO settings
 *
 * Do board specific power GPIO settings
 * here.
 */
extern void board_gpio_power_on();

/** UART pin config
 *
 * Select which pins will be used for different UART ports
 *
 * Leave Blank if UART is not implemented on the board.
 *
 * \param[in] id UART port number
 */
extern void board_uart_pin_config(int id);

/** I2C pin config
 *
 * Select which pins will be used for different I2C ports
 *
 * Leave Blank if I2C is not implemented on the board.
 *
 * \param[in] id I2C port number
 */
extern void board_i2c_pin_config(int id);

/** SSP pin config
 *
 * Select which pins will be used for different SSP ports
 *
 * Leave Blank if SSP is not implemented on the board.
 *
 * \param[in] id SSP port number
 * \param[in] cs If internal SSPx_FRM (cs=1) should be activated or not.
 */

extern void board_usb_pin_config();
extern void board_ssp_pin_config(int id, bool cs);

/** SDIO pin config
 *
 * Select which pins will be used for SDIO controller
 *
 * Leave Blank if SDIO is not connected on the board.
 */
extern void board_sdio_pin_config();

/** Pin connected to Green LED
 *
 * \return GPIO number or -WM_FAIL if such LED
 * is not implemented on the board.
 */
extern int board_led_1();

/** Pin connected to Yellow LED
 *
 * \return GPIO number or -WM_FAIL if such LED
 * is not implemented on the board.
 */
extern int board_led_2();

/** Pin connected to White LED
 *
 * \return GPIO number or -WM_FAIL if such LED
 * is not implemented on the board.
 */
extern int board_led_3();

/** Pin connected to Blue LED
 *
 * \return GPIO number or -WM_FAIL if such LED
 * is not implemented on the board.
 */
extern int board_led_4();

/** Turn LED on
 *
 * Depending upon how the LED is connected,
 * corresponding GPIO needs to be either
 * set or reset.
 *
 * Leave Blank if LEDs are not implemented
 * on the board.
 *
 * \param[in] pin GPIO pin to be used
 */
extern void board_led_on(int pin);

/** Turn LED off
 *
 * Depending upon how the LED is connected,
 * corresponding GPIO needs to be either
 * set or reset.
 *
 * Leave Blank if LEDs are not implemented
 * on the board.
 *
 * \param[in] pin GPIO pin to be used
 */
extern void board_led_off(int pin);

/** WPS Pushbutton
 *
 * \return GPIO connected to the pushbutton
 * to be used for WPS functionality or -WM_FAIL
 * if such pushbutton is not implemented on the board.
 */
extern int board_button_1();

/** Reset to Provisioning Pushbutton
 *
 * \return GPIO connected to the pushbutton
 * to be used for Reset to Provisioning mode
 * functionality or -WM_FAIL if such pushbutton
 * is not implemented on the board.
 */
extern int board_button_2();

/** Turn LCD backlight off
 *
 * Depending upon how the LCD backlight is controlled
 * corressponding GPIO is to be toggled.
 * Leave Blank if LCD backlight control is not present
 * on the board.
 */
extern void board_lcd_backlight_off();

/** Turn LCD backlight on
 *
 * Depending upon how the LCD backlight is controlled
 * corresponding GPIO is to be toggled.
 * Leave Blank if LCD backlight control is not present
 * on the board.
 */
extern void board_lcd_backlight_on();

/** Reset LCD
 *
 * Reset the on-board LCD.
 * Leave Blank if LCD is not present
 * on the board.
 */
extern void board_lcd_reset();

/*
 * Returns  GPIO pin of WIFI card that is used for
 * host wake up on WLAN feature.
 * @return pin_no  used for wakeup
 *        -WM_FAIL in case no pin available
 */
extern int board_wifi_host_wakeup();

/**
 *  This function  indicates whether  wakeup 0 pin is functional
 *   or not on this board
 *   @return   True or False
 */
extern int board_wakeup0_functional();

/**
 *  This function  indicates whether  wakeup 1 pin is functional
 *   or not on this board
 *   @return   True or False
 */
extern int board_wakeup1_functional();

/**
 *  This function indicates whether wakeup 0 pin is connected
 *  to the WiFi chip's host wakeup pin.
 *   @return   True or False
 */
extern int board_wakeup0_wifi();

/**
 *  This function indicates whether wakeup 1 pin is connected
 *  to the WiFi chip's host wakeup pin.
 *   @return   True or False
 */
extern int board_wakeup1_wifi();
