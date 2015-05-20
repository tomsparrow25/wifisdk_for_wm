/*
 * Copyright (C) 2008-2012, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <wmstdio.h>
#include <string.h>
#include <mdev_uart.h>
#include <mdev_ssp.h>
#include <mdev_i2c.h>
#include <wm_os.h>
#include <peripherals/dtp_drv.h>

/** Buffer to store received data */
static uint8_t dtp_rx_msg[DTP_FRAME_MAX];

static mdev_t *mdev_dtp_;
static os_thread_t dtp_recv_handler;
static os_thread_stack_define(dtp_recv_stack, 2048);

static void (*dtp_drv_recv_cb) (uint8_t *, uint16_t);
static uint32_t(*dtp_drv_read_cb) (uint8_t *, uint32_t);
static uint32_t(*dtp_drv_write_cb) (uint8_t *, uint32_t);

static struct dtp_drv_diag_stat dtp_diag;
static dtp_proto_t dtype;

static os_mutex_t dtp_tx_mutex;

#define dtp_ntohs dtp_htons
static uint16_t dtp_htons(uint16_t n)
{
	return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

/** Register the call back function which will be called when data will
 * be received */
void dtp_drv_register_recv_cb(void (*fn) (uint8_t *, uint16_t))
{
	dtp_drv_recv_cb = fn;
}

/** Open the mdev interface for UART*/
static mdev_t *dtp_with_uart_mdev(int port_id)
{
	uart_drv_init(port_id, UART_8BIT);
	mdev_dtp_ = uart_drv_open(port_id, 0);
	if (mdev_dtp_ == NULL)
		DTP_DBG("UART driver init is required before open\r\n");
	return mdev_dtp_;
}

/** Open the mdev interface for SPI*/
static mdev_t *dtp_with_spi_mdev(int port_id, int slave)
{
	ssp_drv_init(port_id);
	mdev_dtp_ = ssp_drv_open(port_id, SSP_FRAME_SPI, slave,
			DMA_DISABLE, -1, 0);
	if (mdev_dtp_ == NULL)
		DTP_DBG("SPI driver init is required before open\r\n");
	return mdev_dtp_;
}

/** Open the mdev interface for I2C*/
static mdev_t *dtp_with_i2c_mdev(int port_id, int slave)
{
	i2c_drv_init(port_id);
	if (slave == I2C_MASTER) {
		mdev_dtp_ = i2c_drv_open(port_id,
				I2C_SLAVEADR(I2C_ADDR >> 1));
		if (mdev_dtp_ == NULL)
			DTP_DBG("I2C drv init is required before open\r\n");

	} else {
		mdev_dtp_ = i2c_drv_open(port_id, I2C_DEVICE_SLAVE
				| I2C_SLAVEADR(I2C_ADDR >> 1));
		if (mdev_dtp_ == NULL)
			DTP_DBG("I2C drv init is required before open\r\n");
	}
	return mdev_dtp_;
}

static uint32_t dtp_uart_drv_write(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0;
	len = uart_drv_write(mdev_dtp_, buf, nbytes);
	return len;
}

#define DTP_SPI_ESC 0xfd

static uint32_t dtp_spi_drv_write(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0, i;
	uint8_t esc = DTP_SPI_ESC, data;

	for (i = 0; i < nbytes; i++) {
		/* For SPI, we never send SPI_DUMMY_BYTE (0x00) on tx line.
		 * Before putting a byte on spi interface, we check for
		 * 0x00 and special escape character for SPI
		 * and if found, we send escape character followed
		 * by byte EX-ORed with 0x20
		 */
		if (*buf == SPI_DUMMY_BYTE || *buf == DTP_SPI_ESC) {
			len = ssp_drv_write(mdev_dtp_, &esc, NULL, 1, 0);
			data = buf[i] ^ 0x20;
			len = ssp_drv_write(mdev_dtp_, &data, NULL, 1, 0);
		} else
			len = ssp_drv_write(mdev_dtp_, &buf[i], NULL, 1, 0);
	}
	return len;
}

static uint32_t dtp_i2c_drv_write(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0;
	len = i2c_drv_write(mdev_dtp_, buf, nbytes);
	return len;
}

static uint32_t dtp_uart_drv_read(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0;
	len = uart_drv_read(mdev_dtp_, buf, nbytes);
	return len;
}

static uint32_t dtp_spi_drv_read(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0, i, buf_ix = 0;
	uint8_t *buf_t = os_mem_alloc(nbytes);

	/* We first read in temp buffer so that later we can
	 * escape unvalid data
	 */
	len = ssp_drv_read(mdev_dtp_, buf_t, nbytes);
	/* Application should transmit SPI_DUMMY_BYTE (0x00)
	 * or SPI_ESC (0xfd) byte in following sequence:
	 * 1) Transmit SPI_ESC(0xfd) byte
	 * 2) Transmit special byte by XORing with 0x20
	 */
	for (i = 0; i < len; i++) {
		if (buf_t[i] == SPI_DUMMY_BYTE)
			continue;
		if (buf_t[i] == DTP_SPI_ESC) {
			i++;
			buf[buf_ix++] = buf_t[i] ^ (uint8_t)0x20;
		} else
			buf[buf_ix++] = buf_t[i];

	}
	os_mem_free(buf_t);
	return buf_ix;
}

static uint32_t dtp_i2c_drv_read(uint8_t *buf, uint32_t nbytes)
{
	uint32_t len = 0;
	len = i2c_drv_read(mdev_dtp_, buf, nbytes);
	return len;
}

/** Print message which contains 0xfd escaped data*/
void dtp_drv_print_msg(uint8_t *dtp_msg, int len)
{
	int i;
	for (i = 0; i < len; i++)
		wmprintf("0x%02x ", dtp_msg[i]);
}

/** calculates 8-bit CRC of given data
 * based on the polynomial  X^8 + X^5 + X^4 + 1
 * */
static uint8_t dtp_drv_calc_checksum(char *start, char *end)
{
	uint8_t crc = 0, c;
	int i;
	while (start < end) {
		c = *start;
		for (i = 0; i < 8; i++) {
			if ((crc ^ c) & 1)
				crc = (crc >> 1) ^ 0x8c;
			else
				crc >>= 1;
			c >>= 1;
		}

		start++;
	}
	return crc;
}

static int verify_crc(uint8_t *buf, uint16_t len, uint8_t recv_crc)
{
	uint8_t crc = 0;
	crc = dtp_drv_calc_checksum((char *)buf, (char *)buf+len);
	return crc == recv_crc;
}

static int dtp_drv_tx_state_machine(uint8_t *buf, int n)
{
	int count = 0;
	static uint8_t state = PKT_TX_START, datalen, cmdlen;
	static uint16_t len;
	uint8_t c, esc = DTP_ESCAPE_BYTE;
	uint8_t *ptr;

	while (count < n) {
		c = (uint8_t) *(buf + count);
		switch (state) {
		case PKT_TX_START:
			if (c == DTP_SOF) {
				state = PKT_TX_SOF;
				cmdlen = 0;
				datalen = 0;
			}
			DTP_DBG_SM("PKT_TX_START -> PKT_TX_SOF\r\n");
			break;
		case PKT_TX_SOF:
			cmdlen++;
			if (cmdlen >= CMD_LEN) {
				state = PKT_TX_CMD;
				DTP_DBG_SM("PKT_TX_SOF -> PKT_TX_CMD\r\n");
			}
			break;
		case PKT_TX_CMD:
			ptr = (uint8_t *)&len;
			ptr[datalen++] = c;
			if (datalen >= DATA_LEN) {
				len = dtp_ntohs(len);
				if (!len) {
					state = PKT_TX_PAYLOAD;
					DTP_DBG_SM("PKT_TX_CMD -> "
						"PKT_TX_PAYLOAD %d\r\n", len);
				} else {
					state = PKT_TX_LEN;
					DTP_DBG_SM("PKT_TX_CMD -> "
						"PKT_TX_LEN %d\n", len);
				}
			}
			break;
		case PKT_TX_LEN:
			len--;
			if (len) {
				if (c == DTP_SOF || c == DTP_EOF ||
						c == DTP_ESCAPE_BYTE) {
					dtp_drv_print_msg(&esc, 1);
					dtp_drv_write_cb(&esc, 1);
					c ^= DTP_ESCAPE_VAL;
					DTP_DBG_SM("Escaping byte\r\n");
				}
			} else {
				state = PKT_TX_PAYLOAD;
				DTP_DBG_SM("PKT_TX_LEN -> PKT_TX_PAYLOAD\r\n");
			}
			break;
		case PKT_TX_PAYLOAD:
			if (c == DTP_SOF || c == DTP_EOF ||
					c == DTP_ESCAPE_BYTE) {
				dtp_drv_print_msg(&esc, 1);
				dtp_drv_write_cb(&esc, 1);
				c ^= DTP_ESCAPE_VAL;
				DTP_DBG_SM("Escaping byte\r\n");
			}
			state = PKT_TX_EOF;
			DTP_DBG_SM("PKT_TX_PAYLOAD -> PKT_TX_EOF\r\n");
			break;
		case PKT_TX_EOF:
			if (c == DTP_EOF)
				state = PKT_TX_START;
			DTP_DBG_SM("PKT_TX_EOF -> PKT_TX_START\r\n");
			break;
		default:
			DTP_DBG_SM("Invalid state in TX\r\n");
			dtp_diag.dtp_framing_err++;
			state = PKT_TX_START;
			break;
		}
		dtp_drv_print_msg(&c, 1);
		dtp_drv_write_cb(&c, 1);
		count++;
	}
	return count;
}

/** Send data using the underlying hardware protocol */
int dtp_drv_send_data(uint8_t *msg, int len)
{
	int ret;
	uint8_t c;

	if (!mdev_dtp_) {
		DTP_DBG("Err: invalid mdev handle\r\n");
		return -WM_FAIL;
	}

	if (len > DTP_FRAME_MAX) {
		DTP_DBG("Frame len exceeds max trans size\r\n");
		dtp_diag.dtp_tx_fail++;
		return -WM_FAIL;
	}

	DTP_DBG("Sending packet ==>\r\n");

	/* Protect from multiple thread contexts */
	ret = os_mutex_get(&dtp_tx_mutex, OS_WAIT_FOREVER);
	if (ret == WM_FAIL) {
		DTP_DBG("failed to get mutex\r\n");
		return ret;
	}

	c = DTP_SOF;
	dtp_drv_tx_state_machine(&c, 1);

	ret = dtp_drv_tx_state_machine(msg, len);
	if (ret != len)
		dtp_diag.dtp_tx_fail++;

	c = dtp_drv_calc_checksum((char *) msg, (char *) msg + len);
	dtp_drv_tx_state_machine(&c, 1);

	c = DTP_EOF;
	dtp_drv_tx_state_machine(&c, 1);

	dtp_diag.dtp_tx_pkt++;

	/* Done sending data, release mutex */
	os_mutex_put(&dtp_tx_mutex);

	DTP_DBG("Done\r\n");

	return WM_SUCCESS;
}

static int dtp_rx_state_machine(uint8_t *buf, int n)
{
	static uint8_t state_rx = NO_PKT_RECV, datalen, cmdlen;
	static uint16_t len;
	static uint16_t index;
	uint8_t c, *ptr;

	while (n) {
		c = (char) *buf;

		if (c == DTP_EOF && state_rx != PKT_CKSUM_RECV) {
			DTP_DBG_SM("Received EOF, re-start sm\r\n");
			state_rx = NO_PKT_RECV;
			dtp_diag.dtp_framing_err++;
		}

		switch (state_rx) {
		case NO_PKT_RECV:
			if (c == DTP_SOF) {
				DTP_DBG_SM("NO_PKT_RECV > PKT_START\r\n");
				index = 0;
				state_rx = PKT_START;
				cmdlen = 0;
				datalen = 0;
				dtp_diag.dtp_rx_pkt++;
			}
			break;
		case PKT_START:
			dtp_rx_msg[index++] = c;
			cmdlen++;
			if (cmdlen >= CMD_LEN) {
				state_rx = PKT_CMD_RECV;
				DTP_DBG_SM("PKT_START > PKT_CMD_RECV\r\n");
			}
			break;
		case PKT_CMD_RECV:
			dtp_rx_msg[index++] = c;
			ptr = (uint8_t *)&len;
			ptr[datalen++] = c;

			if (datalen >= DATA_LEN) {
				len = dtp_ntohs(len);
				if (len == 0) {
					DTP_DBG_SM("PKT_CMD_RECV > "
						  "PKT_DATA_RECV_DONE\r\n");
					state_rx = PKT_DATA_RECV_DONE;
				} else {
					DTP_DBG_SM("PKT_CMD_RECV > "
						  "PKT_DATA_RECV\r\n");
					state_rx = PKT_DATA_RECV;
				}
			}
			break;
		case PKT_DATA_RECV:
			if (c == DTP_ESCAPE_BYTE) {
				DTP_DBG_SM("PKT_DATA_RECV > "
					  "PKT_ESCAPE_CHR_RECV\r\n");
				state_rx = PKT_ESCAPE_CHR_RECV;
			} else {
				dtp_rx_msg[index++] = c;
				if ((--len) == 0) {
					DTP_DBG_SM("PKT_DATA_RECV > "
						  "PKT_DATA_RECV_DONE\r\n");
					state_rx = PKT_DATA_RECV_DONE;
				}
			}
			break;
		case PKT_ESCAPE_CHR_RECV:
			dtp_rx_msg[index++] = c ^ DTP_ESCAPE_VAL;
			if ((--len) == 0) {
				DTP_DBG_SM("PKT_ESCAPE_CHR_RECV > "
					  "PKT_DATA_RECV_DONE\r\n");
				state_rx = PKT_DATA_RECV_DONE;
			} else {
				DTP_DBG_SM("PKT_ESCAPE_CHR_RECV > "
					  "PKT_DATA_RECV\r\n");
				state_rx = PKT_DATA_RECV;
			}
			break;
		case PKT_DATA_RECV_DONE:
			if (c == DTP_ESCAPE_BYTE) {
				state_rx = PKT_ESCAPE_CKSUM;
				DTP_DBG_SM("PKT_DATA_RECV_DONE > "
					  "PKT_ESCAPE_CKSUM\r\n");
			} else {
				if (!verify_crc(dtp_rx_msg, index, c)) {
					dtp_diag.dtp_chksum_err++;
					DTP_DBG("CRC mismatch %x %x\r\n",
							index, c);
				}
				DTP_DBG_SM("PKT_DATA_RECV_DONE > "
						"PKT_CKSUM_RECV\r\n");
				state_rx = PKT_CKSUM_RECV;
			}
			break;
		case PKT_ESCAPE_CKSUM:
			c ^= DTP_ESCAPE_VAL;
			if (!verify_crc(dtp_rx_msg, index, c)) {
				dtp_diag.dtp_chksum_err++;
				DTP_DBG("CRC mismatch %x %X\r\n", index, c);
			}
			DTP_DBG_SM("PKT_ESCAPE_CKSUM > PKT_CKSUM_RECV\r\n");
			state_rx = PKT_CKSUM_RECV;
			break;
		case PKT_CKSUM_RECV:
			if (c == DTP_EOF) {
				DTP_DBG_SM("PKT_CKSUM_RECV > NO_PKT_RCV\r\n");
				state_rx = NO_PKT_RECV;
				if (dtp_drv_recv_cb)
					dtp_drv_recv_cb(dtp_rx_msg, index);
				memset(dtp_rx_msg, 0, sizeof(dtp_rx_msg));
			}
			break;
		default:
			DTP_DBG_SM("Invalid RX state\r\n");
			dtp_diag.dtp_framing_err++;
			state_rx = NO_PKT_RECV;
			break;
		}
		buf++;
		n--;
	}

	if (index > DTP_FRAME_MAX) {
		DTP_DBG("Frame exceeds max recv length\r\n");
		index = 0;
	}

	return WM_SUCCESS;
}

#define DTP_RD_CHUNK_SIZE 64
/* Receiver thread */
static void dtp_drv_recv_thread(os_thread_arg_t arg)
{
	int len = 0;
	uint8_t r_buff[DTP_RD_CHUNK_SIZE];

	DTP_DBG("[dtp_drv]: entering receive thread loop\r\n");

	while (1) {
		/* Read data from the driver */
		len = dtp_drv_read_cb(r_buff, sizeof(r_buff));
		if (len > 0) {
			DTP_DBG("Recevied packet <==\r\n");
			dtp_drv_print_msg(r_buff, len);
			DTP_DBG("Done\r\n");
			dtp_rx_state_machine(r_buff, len);
		} else {
			os_thread_sleep(os_msec_to_ticks(10));
		}
	}
}

/** This function initializes the data transfer protocol driver */
static mdev_t *dtp_drv_spi_hw_proto_init(int port_id, int slave)
{
	dtype = DTP_SPI;
	dtp_drv_read_cb = dtp_spi_drv_read;
	dtp_drv_write_cb = dtp_spi_drv_write;
	return dtp_with_spi_mdev(port_id, slave);
}

static mdev_t *dtp_drv_i2c_hw_proto_init(int port_id, int slave)
{
	dtype = DTP_I2C;
	dtp_drv_read_cb = dtp_i2c_drv_read;
	dtp_drv_write_cb = dtp_i2c_drv_write;
	return dtp_with_i2c_mdev(port_id, slave);
}

static mdev_t *dtp_drv_uart_hw_proto_init(int port_id)
{
	dtype = DTP_UART;
	dtp_drv_read_cb = dtp_uart_drv_read;
	dtp_drv_write_cb = dtp_uart_drv_write;
	return dtp_with_uart_mdev(port_id);
}

/** This function cretae one thread which receives data from from driver
 *  when data is avialable in pre-defined frame format
 */
int dtp_drv_init(dtp_proto_t proto, int port_id, int slave)
{
	int ret = 0;

	switch (proto) {
	case DTP_UART:
		if (dtp_drv_uart_hw_proto_init(port_id) == NULL)
			return -WM_FAIL;
		break;
	case DTP_SPI:
		if (dtp_drv_spi_hw_proto_init(port_id, slave) == NULL)
			return -WM_FAIL;
		break;
	case DTP_I2C:
		if (dtp_drv_i2c_hw_proto_init(port_id, slave) == NULL)
			return -WM_FAIL;
		break;
	default:
		DTP_DBG("Err: Invalid protocol\r\n");
		return -WM_FAIL;
	}

	ret = os_thread_create(&dtp_recv_handler,
			       "dtp_recv", dtp_drv_recv_thread, 0,
			       &dtp_recv_stack, OS_PRIO_3);
	if (ret != WM_SUCCESS) {
		DTP_DBG("Failed to create receive thread: %d\r\n", ret);
		return -WM_FAIL;
	}

	ret = os_mutex_create(&dtp_tx_mutex, "dtp_tx_mutex", OS_MUTEX_INHERIT);
	if (ret != WM_SUCCESS) {
		DTP_DBG("Failed to create mutex: %d\r\n", ret);
		return -WM_FAIL;
	}
	return ret;
}

void dtp_drv_diag_info(struct dtp_drv_diag_stat *stat)
{
	stat->dtp_tx_pkt = dtp_diag.dtp_tx_pkt;
	stat->dtp_rx_pkt = dtp_diag.dtp_rx_pkt;
	stat->dtp_tx_fail = dtp_diag.dtp_tx_fail;
	stat->dtp_framing_err = dtp_diag.dtp_framing_err;
	stat->dtp_chksum_err = dtp_diag.dtp_chksum_err;
}
