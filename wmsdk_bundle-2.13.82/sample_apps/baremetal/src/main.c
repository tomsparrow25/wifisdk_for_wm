/*
 *  Copyright (C) 2008-2013 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Bare-metal Hello World Application
 *
 * Summary:
 *
 * This application overrides os_init call from WMSDK
 * (being a WEAK qualifier in WMSDK) and skips initialization
 * of scheduler. Thus, application executes in standalone,
 * NO-OS environment in which processor (88MC200) uses main
 * stack (minimal in size) and no OS calls are used.
 *
 * Prints Hello World every 5 seconds on the serial console.
 * The serial console is set on UART-0.
 *
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */
#include <wm_os.h>
#include <mc200_uart.h>
#include <board.h>

void init_uart()
{
	UART_CFG_Type uartCfg = {115200,
		UART_DATABITS_8,
		UART_PARITY_NONE,
		UART_STOPBITS_1,
		DISABLE};
	board_uart_pin_config(UART0_ID);
	CLK_SetUARTClkSrc(CLK_UART_ID_0, CLK_UART_SLOW);
	CLK_ModuleClkEnable(CLK_UART0);
	UART_Reset(UART0_ID);
	UART_Init(UART0_ID, &uartCfg);
}

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done.
 */
int main(void)
{
	int count = 0;
	char buf[64];

	/* Initialize console on uart0 */
	init_uart();

	while (1) {
		count++;
		snprintf(buf, sizeof(buf),
			"Bare-metal Hello World: iteration %d\r\n", count);
		UART_WriteLine(UART0_ID, (uint8_t *) buf);

		/* Sleep  5 seconds */
		_os_delay(5000);
	}
	return 0;
}

/*
 * This overrides os_init from WMSDK, we will not initialize scheduler and
 * just execute application in standalone environment.
 */
int os_init()
{
	main();

	/* Nothing to do, wait forever */
	for (;;)
		;
}
