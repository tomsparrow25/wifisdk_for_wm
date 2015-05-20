/*! \file wmstdio.h
 *  \brief Standard Input Output
 */
/*  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _WMSTDIO_H_
#define _WMSTDIO_H_

#include <wmtypes.h>
#include <mdev_uart.h>

/** Maximum number of characters that can be printed using wmprintf */
#define MAX_MSG_LEN 127

typedef struct stdio_funcs {
	int (*sf_printf)(char *);
	int (*sf_flush)();
	int (*sf_getchar)(uint8_t *);
	int (*sf_putchar)(char *);
} stdio_funcs_t;

extern stdio_funcs_t *c_stdio_funcs;

/** The printf function
 * 
 * This function prints data to the output port. The function prototype is analogous
 * to the printf() function of the C language.
 *
 * \param[in] format The format string
 *
 * \return Number of bytes written to the console.
 */
extern
int wmprintf(const char *format, ...);

/** The printf function
 *
 * This function prints character to the output port. This function should be
 * used for printing data character by character.
 *
 * \param[in] ch Pointer to the character to written to the console
 *
 * \return Number of bytes written to the console.
 */
extern
int wmstdio_putchar(char *ch);

/** The printf function
 *
 * This function prints data to the output port. This function should be
 * used for while printing from interrupts.
 *
 * \return Number of bytes written to the console.
 */
#ifdef CONFIG_LL_DEBUG
extern
uint32_t ll_printf(const char *format, ...);
#else
#define ll_printf(...)
#endif
/** Flush Standard I/O
 *
 * This function flushes the send buffer to the output port.
 *
 * @return WM_SUCCESS
 */
extern
uint32_t wmstdio_flush(void);

/**
 * Read one byte
 * 
 * \param pu8c Pointer to the location to store the byte read.
 *
 * \return Number of bytes read from console port.
 */
extern
uint32_t wmstdio_getchar(uint8_t *pu8c);
/**
 * Returns port id used for console
 *
 * \param[out] port_id pointer to port_id
 *
 * \return WM_SUCCESS
 */
extern
int wmstdio_getconsole_port(int *port_id);

/**
 * Initialize the standard input output facility
 * 
 * \note This function is called by the wm_core_init function. Applications need
 * not explicity call this function if they have already called wm_core_init().
 *
 * \param[in] uart_port UART Port to be used as the console (e.g. UART0_ID,
 * UART1_ID etc.
 * \param[in] baud Baud rate to be used on the port. 0 implies the default baud
 * rate
 * \return WM_SUCCESS on success, error code otherwise.
 */
extern
int wmstdio_init(UART_ID_Type uart_port, int baud);

/* To be used in situations where you HAVE to send data to the console */
uint32_t __console_wmprintf(const char *format, ...);

/* Helper functions to print a float value. Some compilers have a problem
 * interpreting %f
 */

#define wm_int_part_of(x)     ((int)(x))
static inline int wm_frac_part_of(float x, short precision)
{
	int scale = 1;

	while (precision--)
		scale *= 10;

	return (x < 0 ? (int)(((int)x - x) * scale) : (int)((x - (int)x) * scale));
}

#endif

