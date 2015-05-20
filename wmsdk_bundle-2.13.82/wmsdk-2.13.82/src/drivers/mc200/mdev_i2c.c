/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <mc200.h>
#include <mc200_gpio.h>
#include <mc200_pinmux.h>
#include <mc200_clock.h>
#include <board.h>
#include <mdev_i2c.h>
#include <wm_os.h>
#include <mc200_dma.h>
#include <limits.h>

#include <wmlog.h>
#define i2c_e(...)				\
	wmlog_e("i2c", ##__VA_ARGS__)
#define i2c_w(...)				\
	wmlog_w("i2c", ##__VA_ARGS__)

#ifdef CONFIG_I2C_DEBUG
#define i2c_d(...)				\
	wmlog("i2c", ##__VA_ARGS__)
#else
#define i2c_d(...)
#endif				/* ! CONFIG_I2C_DEBUG */

/* #define CONFIG_I2C_FUNC_DEBUG */

#ifdef CONFIG_I2C_FUNC_DEBUG
#define i2c_entry(...)			\
	wmlog_entry(__VA_ARGS__)
#else
#define i2c_entry(...)
#endif				/* ! CONFIG_I2C_DEBUG */

#define DMA_MAX_BLK_SIZE 1023

#define I2C_RX_CHANNEL CHANNEL_0
#define I2C_TX_CHANNEL CHANNEL_1

#define NUM_MC200_I2C_PORTS 3
#define I2C_INT_BASE INT_I2C0
#define I2C_IRQn_BASE I2C0_IRQn
#define I2C_RX_BUF_SIZE 128
#define SEMAPHORE_NAME_LEN 30

os_semaphore_t i2c_dma_tx_sem[NUM_MC200_I2C_PORTS];
os_semaphore_t i2c_dma_rx_sem[NUM_MC200_I2C_PORTS];

/* HIGH_TIME & LOW_TIME values are set using
 * MC200 datasheet- I2C_CLK frequency configuration
 */
#define MIN_SCL_HIGH_TIME_STANDARD 4000	/* 4000ns for 100KHz */
#define MIN_SCL_LOW_TIME_STANDARD  4700	/* 4700ns for 100KHz */
#define MIN_SCL_HIGH_TIME_FAST 600	/* 600ns for 400KHz  */
#define MIN_SCL_LOW_TIME_FAST 1300	/* 1300ns for 400KHz */

/* HCNT & LCNT initialized to min required count */
/* values for standard speed 100KHz*/
static int STANDARD_LCNT = 8;
static int STANDARD_HCNT = 6;
/* values for fast speed 400KHz*/
static int FAST_LCNT = 8;
static int FAST_HCNT = 6;
/* values for High speed 3.4MHz*/
/* Note: High speed is currently not supported */
static int HS_LCNT = 8;
static int HS_HCNT = 6;

static bool is_clkcnt_set = RESET;
#define SLAVE_WRITE_TIMEOUT 100
/* The device objects */
static mdev_t mdev_i2c[NUM_MC200_I2C_PORTS];
static os_mutex_t i2c_mutex[NUM_MC200_I2C_PORTS];
static os_semaphore_t write_slave_sem[NUM_MC200_I2C_PORTS];
static uint32_t tx_tout[NUM_MC200_I2C_PORTS];
static uint32_t rx_tout[NUM_MC200_I2C_PORTS];

#define cpu_freq_MHz() (board_cpu_freq()/1000000)

I2C_DmaReqLevel_Type i2cDmaCfg = { 0, 0 };

/* Device name */
static const char *mdev_i2c_name[NUM_MC200_I2C_PORTS] = {
	"MDEV_I2C0",
	"MDEV_I2C1",
	"MDEV_I2C2"
};

typedef struct i2cdrv_buf_ {
	int buf_size;		/* number of bytes in buffer */
	int buf_head;		/* number of bytes transfer from slave */
	char *buf_p;		/* pointer to buffer storage */
} i2c_buf_t;
typedef struct i2cdrv_ringbuf {
	int wr_idx;		/* write pointer */
	int rd_idx;		/* read pointer */
	int buf_size;		/* number of bytes in buffer *buf_p */
	uint8_t *buf_p;		/* pointer to buffer storage */
} i2c_ringbuf_t;

typedef struct i2cdrv_clkcnt {
	int high_time;
	int low_time;
} i2c_drv_clkcnt;

typedef struct mdev_i2c_priv_data_ {
	i2c_ringbuf_t rx_ringbuf;	/* receive buffer structure */
	i2c_buf_t tx_buf;	/* transfer buffer structure */
	I2C_CFG_Type *i2c_cfg;
	i2c_drv_clkcnt *i2c_clkcnt;
	bool dma;
} mdev_i2c_priv_data_t;

I2C_FIFO_Type i2cFifoCfg = { 0,	/* i2c receive request level */
	0			/* i2c transmit request level */
};

/* Device private data */
static mdev_i2c_priv_data_t i2cdev_priv_data_[NUM_MC200_I2C_PORTS];
/* i2c device specific data */
static I2C_CFG_Type i2cdev_data[NUM_MC200_I2C_PORTS];
static i2c_drv_clkcnt i2cdev_clk_data[NUM_MC200_I2C_PORTS];

static void i2cdrv_read_irq_handler(int port_num);
static void i2cdrv_write_irq_handler(int port_num);

static void i2cdrv_tx_cb(int port_num);
static void i2cdrv_rx_cb(int port_num);

static void (*i2c_read_irq_handler[NUM_MC200_I2C_PORTS]) ();
static void (*i2c_write_irq_handler[NUM_MC200_I2C_PORTS]) ();

static void (*i2c_dma_tx_cb[NUM_MC200_I2C_PORTS]) ();
static void (*i2c_dma_rx_cb[NUM_MC200_I2C_PORTS]) ();

#define SET_RX_BUF_SIZE(i2c_data_p, size) i2c_data_p->rx_ringbuf.buf_size = size
#define GET_RX_BUF_SIZE(i2c_data_p) (i2c_data_p->rx_ringbuf.buf_size)

#define DEFINE_I2C_CB(id, mode)	\
				static void \
				i2cdrv ## id ## _ ## mode ## irq_handler() {\
				i2cdrv_ ## mode ## _irq_handler(id); }
#define GET_I2C_CB(id, mode)	i2cdrv ## id ## _ ## mode ## irq_handler

#define DEFINE_I2C_TX_CB(id) \
				static void \
				i2cdrv_tx ## id ## _cb() {\
				i2cdrv_tx_cb(id); }
#define GET_I2C_TX_CB(id) i2cdrv_tx ## id ## _cb

#define DEFINE_I2C_RX_CB(id) \
				static void \
				i2cdrv_rx ## id ## _cb() {\
				i2cdrv_rx_cb(id); }
#define GET_I2C_RX_CB(id) i2cdrv_rx ## id ## _cb

DEFINE_I2C_CB(0, read)
DEFINE_I2C_CB(1, read)
DEFINE_I2C_CB(2, read)
DEFINE_I2C_CB(0, write)
DEFINE_I2C_CB(1, write)
DEFINE_I2C_CB(2, write)

DEFINE_I2C_TX_CB(0)
DEFINE_I2C_TX_CB(1)
DEFINE_I2C_TX_CB(2)

DEFINE_I2C_RX_CB(0)
DEFINE_I2C_RX_CB(1)
DEFINE_I2C_RX_CB(2)

static void fill_cb_array()
{
	i2c_read_irq_handler[0] = GET_I2C_CB(0, read);
	i2c_read_irq_handler[1] = GET_I2C_CB(1, read);
	i2c_read_irq_handler[2] = GET_I2C_CB(2, read);
	i2c_write_irq_handler[0] = GET_I2C_CB(0, write);
	i2c_write_irq_handler[1] = GET_I2C_CB(1, write);
	i2c_write_irq_handler[2] = GET_I2C_CB(2, write);
	i2c_dma_tx_cb[0] = GET_I2C_TX_CB(0);
	i2c_dma_tx_cb[1] = GET_I2C_TX_CB(1);
	i2c_dma_tx_cb[2] = GET_I2C_TX_CB(2);
	i2c_dma_rx_cb[0] = GET_I2C_RX_CB(0);
	i2c_dma_rx_cb[1] = GET_I2C_RX_CB(1);
	i2c_dma_rx_cb[2] = GET_I2C_RX_CB(2);

}

/* ring buffer apis */

static uint8_t read_rx_buf(mdev_i2c_priv_data_t *i2c_data_p)
{
	uint8_t ret;
	int temp, idx;
	temp = os_enter_critical_section();
	idx = i2c_data_p->rx_ringbuf.rd_idx;
	ret = i2c_data_p->rx_ringbuf.buf_p[idx];
	i2c_data_p->rx_ringbuf.rd_idx = (i2c_data_p->rx_ringbuf.rd_idx + 1)
	    % GET_RX_BUF_SIZE(i2c_data_p);
	os_exit_critical_section(temp);
	return ret;
}

static void write_rx_buf(mdev_i2c_priv_data_t *i2c_data_p, uint8_t wr_char)
{
	int temp, idx;
	temp = os_enter_critical_section();
	idx = i2c_data_p->rx_ringbuf.wr_idx;
	i2c_data_p->rx_ringbuf.buf_p[idx] = wr_char;
	i2c_data_p->rx_ringbuf.wr_idx = (i2c_data_p->rx_ringbuf.wr_idx + 1)
	    % GET_RX_BUF_SIZE(i2c_data_p);
	os_exit_critical_section(temp);
}

static bool is_rx_buf_empty(mdev_i2c_priv_data_t *i2c_data_p)
{
	int rd_idx, wr_idx, temp;
	temp = os_enter_critical_section();
	rd_idx = i2c_data_p->rx_ringbuf.rd_idx;
	wr_idx = i2c_data_p->rx_ringbuf.wr_idx;
	if (rd_idx == wr_idx) {
		os_exit_critical_section(temp);
		return true;
	}
	os_exit_critical_section(temp);
	return false;
}

static bool is_rx_buf_full(mdev_i2c_priv_data_t *i2c_data_p)
{
	int wr_idx, rd_idx, temp;
	temp = os_enter_critical_section();
	rd_idx = i2c_data_p->rx_ringbuf.rd_idx;
	wr_idx = i2c_data_p->rx_ringbuf.wr_idx;
	if (((wr_idx + 1) % GET_RX_BUF_SIZE(i2c_data_p)) == rd_idx) {
		os_exit_critical_section(temp);
		return true;
	}
	os_exit_critical_section(temp);
	return false;
}

static int rx_buf_init(mdev_i2c_priv_data_t *i2c_data_p)
{
	i2c_data_p->rx_ringbuf.rd_idx = 0;
	i2c_data_p->rx_ringbuf.wr_idx = 0;
	i2c_data_p->rx_ringbuf.buf_p = os_mem_alloc
	    (GET_RX_BUF_SIZE(i2c_data_p));
	if (!i2c_data_p->rx_ringbuf.buf_p)
		return -WM_FAIL;
	return WM_SUCCESS;
}

static void i2cdrv_read_irq_handler(int port_num)
{
	uint32_t data;
	mdev_t *dev = &mdev_i2c[port_num];
	mdev_i2c_priv_data_t *i2c_data_p;
	i2c_data_p = (mdev_i2c_priv_data_t *) dev->private_data;
	while (I2C_GetStatus(dev->port_id, I2C_STATUS_RFNE) == SET) {
		data = I2C_ReceiveByte(dev->port_id);
		if (!is_rx_buf_full(i2c_data_p))
			write_rx_buf(i2c_data_p, (uint8_t) data);
	}

}

static void i2cdrv_write_irq_handler(int port_num)
{
	mdev_t *mdev_p = &mdev_i2c[port_num];
	mdev_i2c_priv_data_t *priv_data_p;
	priv_data_p = (mdev_i2c_priv_data_t *) mdev_p->private_data;

	int dma_enabled = priv_data_p->dma;

	if (dma_enabled)
		/* DMA sends out the data on RD_REQ */
		os_semaphore_put(&write_slave_sem[mdev_p->port_id]);

	else {
		if (priv_data_p->tx_buf.buf_p == NULL) {
			/* Sending 0xff when buffer has not data
			 */
			I2C_SendByte(mdev_p->port_id, 0xFF);
			os_semaphore_put(&write_slave_sem[mdev_p->port_id]);
		} else {
			I2C_SendByte(mdev_p->port_id, (priv_data_p->tx_buf.buf_p
						       [priv_data_p->
							tx_buf.buf_head] &
						       0xff));
			++priv_data_p->tx_buf.buf_head;
			if (priv_data_p->tx_buf.buf_size == priv_data_p->
					tx_buf.buf_head) {
				os_semaphore_put(&write_slave_sem
						[mdev_p->port_id]);
				priv_data_p->tx_buf.buf_p = NULL;
			}
		}
	}
}

int i2c_drv_enable(mdev_t *dev)
{
	if (mdev_get_handle(mdev_i2c_name[dev->port_id]) == NULL) {
		i2c_w("trying to enable unregistered device");
		return -WM_FAIL;
	}
	I2C_Enable(dev->port_id);

	return WM_SUCCESS;
}

int i2c_drv_disable(mdev_t *dev)
{
	if (mdev_get_handle(mdev_i2c_name[dev->port_id]) == NULL) {
		i2c_w("trying to disable unregistered device");
		return -WM_FAIL;
	}

	I2C_Disable(dev->port_id);

	return WM_SUCCESS;
}

static void i2cdrv_rx_cb(int port_id)
{
	os_semaphore_put(&i2c_dma_rx_sem[port_id]);
}

static void i2cdrv_tx_cb(int port_id)
{
	os_semaphore_put(&i2c_dma_tx_sem[port_id]);
}

static void dma_error_cb()
{
	ll_printf("error in I2C dma transfer\r\n");
}

static void i2c_dma_rx_config(mdev_t *dev, DMA_CFG_Type
			      *dma, uint32_t addr, int port_id, int num)
{
	switch (dev->port_id) {
	case I2C0_PORT:
		dma->srcDmaAddr = (uint32_t) &I2C0->DATA_CMD.WORDVAL;
		dma->srcHwHdskInterf = DMA_HW_HS_INTER_14;
		DMA_SetHandshakingMapping(DMA_HS14_I2C0_RX);
		break;
	case I2C1_PORT:
		dma->srcDmaAddr = (uint32_t) &I2C1->DATA_CMD.WORDVAL;
		dma->srcHwHdskInterf = DMA_HW_HS_INTER_6;
		DMA_SetHandshakingMapping(DMA_HS6_I2C1_RX);
		break;
	case I2C2_PORT:
		dma->srcDmaAddr = (uint32_t) &I2C2->DATA_CMD.WORDVAL;
		dma->srcHwHdskInterf = DMA_HW_HS_INTER_8;
		DMA_SetHandshakingMapping(DMA_HS8_I2C2_RX);
		break;
	}

	dma->destDmaAddr = addr;
	dma->transfType = DMA_PER_TO_MEM;

	dma->srcBurstLength = DMA_ITEM_1;
	dma->destBurstLength = DMA_ITEM_1;

	dma->srcAddrInc = DMA_ADDR_NOCHANGE;
	dma->destAddrInc = DMA_ADDR_INC;

	dma->srcSwHwHdskSel = DMA_HW_HANDSHAKING;

	dma->channelPriority = DMA_CH_PRIORITY_0;

	dma->fifoMode = DMA_FIFO_MODE_0;

	DMA_ChannelCmd(I2C_RX_CHANNEL, DISABLE);
	DMA_ChannelInit(I2C_RX_CHANNEL, dma);

	dma->srcTransfWidth = DMA_TRANSF_WIDTH_8;
	dma->destTransfWidth = DMA_TRANSF_WIDTH_8;

	DMA_IntClr(I2C_RX_CHANNEL, INT_CH_ALL);
	DMA_IntMask(I2C_RX_CHANNEL, INT_ERROR, UNMASK);

	DMA_IntMask(I2C_RX_CHANNEL, INT_BLK_TRANS_COMPLETE, UNMASK);
	DMA_IntMask(I2C_RX_CHANNEL, INT_ERROR, UNMASK);

	install_int_callback(INT_DMA_CH0, INT_BLK_TRANS_COMPLETE,
			     i2c_dma_rx_cb[dev->port_id]);
	install_int_callback(INT_DMA_CH0, INT_ERROR, dma_error_cb);

	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 0xf);

	I2C_DmaConfig((I2C_ID_Type) dev->port_id, &i2cDmaCfg);

	I2C_DmaCmd((I2C_ID_Type) dev->port_id, ENABLE, ENABLE);
}

static int i2c_drv_slave_dma_read(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	DMA_CFG_Type dma_rx;
	uint16_t dma_trans = DMA_MAX_BLK_SIZE;
	int num = nbytes;
	uint32_t cnt;

	i2c_dma_rx_config(dev, &dma_rx, (uint32_t) buf, dev->port_id, nbytes);
	while (num > 0) {

		if (num < DMA_MAX_BLK_SIZE)
			dma_trans = num;

		DMA_ChannelCmd(I2C_RX_CHANNEL, DISABLE);
		DMA_ChannelInit(I2C_RX_CHANNEL, &dma_rx);
		DMA_IntClr(I2C_RX_CHANNEL, INT_CH_ALL);
		DMA_IntMask(I2C_RX_CHANNEL, INT_BLK_TRANS_COMPLETE, UNMASK);
		DMA_SetBlkTransfSize(I2C_RX_CHANNEL, dma_trans);
		DMA_Enable();

		/* Wait till data is received in the RX FIFO */
		while (I2C_GetStatus(dev->port_id, I2C_STATUS_RFNE) != SET)
			;

		DMA_ChannelCmd(I2C_RX_CHANNEL, ENABLE);

		/* Wait on semaphore i2c_dma_rx_Sem i.e till requested
		 * number of bytes are transferred to memory form I2C
		 * peripheral.
		 */
		int sem_status;
		sem_status = os_semaphore_get(&i2c_dma_rx_sem
					      [dev->port_id], OS_WAIT_FOREVER);

		if (sem_status != WM_SUCCESS)
			return -WM_FAIL;
		dma_rx.destDmaAddr += dma_trans;
		num = num - dma_trans;
		cnt++;
	}
	return nbytes;
}

static void i2c_dma_tx_config(mdev_t *dev, DMA_CFG_Type
			      *dma, uint32_t addr, int num)
{
	I2C_Disable(dev->port_id);

	switch (dev->port_id) {
	case I2C0_PORT:
		dma->destDmaAddr = (uint32_t) &I2C0->DATA_CMD.WORDVAL;
		dma->destHwHdskInterf = DMA_HW_HS_INTER_15;
		DMA_SetHandshakingMapping(DMA_HS15_I2C0_TX);
		break;
	case I2C1_PORT:
		dma->destDmaAddr = (uint32_t) &I2C1->DATA_CMD.WORDVAL;
		dma->destHwHdskInterf = DMA_HW_HS_INTER_12;
		DMA_SetHandshakingMapping(DMA_HS12_I2C1_TX);
		break;
	case I2C2_PORT:
		dma->destDmaAddr = (uint32_t) &I2C2->DATA_CMD.WORDVAL;
		dma->destHwHdskInterf = DMA_HW_HS_INTER_13;
		DMA_SetHandshakingMapping(DMA_HS13_I2C2_TX);
		break;
	}
	/* DMA controller peripheral to memory configuration */
	dma->srcDmaAddr = addr;

	dma->transfType = DMA_MEM_TO_PER;

	dma->srcBurstLength = DMA_ITEM_1;
	dma->destBurstLength = DMA_ITEM_1;

	dma->srcAddrInc = DMA_ADDR_INC;
	dma->destAddrInc = DMA_ADDR_NOCHANGE;

	dma->destSwHwHdskSel = DMA_HW_HANDSHAKING;

	dma->channelPriority = DMA_CH_PRIORITY_0;
	dma->fifoMode = DMA_FIFO_MODE_0;

	DMA_IntClr(I2C_TX_CHANNEL, INT_CH_ALL);

	DMA_ChannelCmd(I2C_TX_CHANNEL, DISABLE);
	DMA_ChannelInit(I2C_TX_CHANNEL, dma);

	dma->srcTransfWidth = DMA_TRANSF_WIDTH_8;
	dma->destTransfWidth = DMA_TRANSF_WIDTH_8;

	DMA_IntClr(I2C_TX_CHANNEL, INT_CH_ALL);

	/* Mask all interrupts before unmasking any */
	DMA_IntMask(I2C_TX_CHANNEL, INT_CH_ALL, MASK);
	DMA_IntMask(I2C_TX_CHANNEL, INT_BLK_TRANS_COMPLETE, UNMASK);
	DMA_IntMask(I2C_TX_CHANNEL, INT_ERROR, UNMASK);

	install_int_callback(INT_DMA_CH1, INT_BLK_TRANS_COMPLETE,
			     i2c_dma_tx_cb[dev->port_id]);

	install_int_callback(INT_DMA_CH1, INT_ERROR, dma_error_cb);

	NVIC_EnableIRQ(DMA_IRQn);

	NVIC_SetPriority(DMA_IRQn, 0xf);

	I2C_DmaConfig((I2C_ID_Type) dev->port_id, &i2cDmaCfg);

	I2C_DmaCmd((I2C_ID_Type) dev->port_id, ENABLE, ENABLE);

	I2C_Enable(dev->port_id);
}

static int i2c_drv_dma_write(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	DMA_CFG_Type dma_tx;
	uint16_t dma_trans = DMA_MAX_BLK_SIZE;
	int num = nbytes;
	uint32_t cnt = 0;
	i2c_dma_tx_config(dev, &dma_tx, (uint32_t) buf, nbytes);

	I2C_CFG_Type *i2c_cfg;
	mdev_i2c_priv_data_t *priv_data_p;
	priv_data_p = (mdev_i2c_priv_data_t *) dev->private_data;
	i2c_cfg = priv_data_p->i2c_cfg;

	while (num > 0) {
		if (num < DMA_MAX_BLK_SIZE)
			dma_trans = num;
		/* for slave mode wait on semaphore write_slave_sem
		   for write request  from master */
		if (i2c_cfg->masterSlaveMode == I2C_SLAVE) {
			os_semaphore_get(&write_slave_sem[dev->port_id]
					 , OS_WAIT_FOREVER);
		}

		DMA_ChannelCmd(I2C_TX_CHANNEL, DISABLE);
		DMA_ChannelInit(I2C_TX_CHANNEL, &dma_tx);
		DMA_IntClr(I2C_TX_CHANNEL, INT_CH_ALL);
		DMA_IntMask(I2C_TX_CHANNEL, INT_BLK_TRANS_COMPLETE, UNMASK);
		DMA_SetBlkTransfSize(I2C_TX_CHANNEL, dma_trans);

		DMA_Enable();
		DMA_ChannelCmd(I2C_TX_CHANNEL, ENABLE);

		/* Wait on semaphore i2c_dma_tx_sem for DMA trasfer to
		   complete */
		int sem_status;
		sem_status = os_semaphore_get(&i2c_dma_tx_sem[dev->port_id]
					      , OS_WAIT_FOREVER);

		if (sem_status != WM_SUCCESS)
			return -WM_FAIL;
		dma_tx.srcDmaAddr += dma_trans;
		num -= dma_trans;
		cnt++;
	}
	return nbytes;
}

static int i2c_drv_pio_read(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	int ret, len = 0;
	uint32_t recvlength = 0, txlength = 0;
	I2C_CFG_Type *i2c_cfg;
	mdev_i2c_priv_data_t *priv_data_p;
	priv_data_p = (mdev_i2c_priv_data_t *) dev->private_data;
	i2c_cfg = priv_data_p->i2c_cfg;
	uint32_t t1 = os_get_timestamp(), t2, tout = rx_tout[dev->port_id];

	if (i2c_cfg->masterSlaveMode == I2C_MASTER) {
		ret = os_mutex_get(&i2c_mutex[dev->port_id], OS_WAIT_FOREVER);
		if (ret == -WM_FAIL) {
			i2c_e("failed to get mutex");
			return -WM_FAIL;
		}
		/*
		 * Note: I2C generates stop condition as soon as its tx fifo
		 * becomes empty (pops out last element from tx fifo), and
		 * hence for multi-byte read make sure to keep tx fifo full
		 * for amount of bytes to read (acknowledge) from slave
		 * device.
		 */

		while (recvlength < nbytes) {
			if ((SET ==
			     I2C_GetStatus(dev->port_id, I2C_STATUS_TFNF))
			    && txlength < nbytes) {
				I2C_MasterReadCmd(dev->port_id);
				txlength++;
			}
			while ((SET ==
				I2C_GetStatus(dev->port_id, I2C_STATUS_RFNE))
			       && recvlength < nbytes) {
				buf[recvlength] =
				    (uint8_t) I2C_ReceiveByte(dev->port_id);
				recvlength++;
				t1 = os_get_timestamp();
			}
			if (tout) {
				t2 = os_get_timestamp();
				if ((((t2 > t1) && ((t2 - t1) > tout)) ||
					((t1 > t2) &&
					(UINT_MAX - t1 + t2) > tout))) {
					recvlength = -WM_E_AGAIN;
					i2c_d("rx timeout");
					break;
				}
			}
		}
		os_mutex_put(&i2c_mutex[dev->port_id]);
		return recvlength;
	} else {
		while (nbytes > 0) {
			if (!is_rx_buf_empty(priv_data_p)) {
				*buf = (uint8_t) read_rx_buf(priv_data_p);
			} else if (I2C_GetStatus(dev->port_id, I2C_STATUS_RFNE)
				   == SET) {
				*buf = (uint8_t) I2C_ReceiveByte(dev->port_id);
			} else
				break;
			nbytes--;
			len++;
			buf++;
		}
		return len;
	}
}

int i2c_drv_read(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	mdev_i2c_priv_data_t *p_data = (mdev_i2c_priv_data_t *)
	    dev->private_data;

	/* check if dma is enabled */
	if (p_data->dma == I2C_DMA_ENABLE) {

		/* check I2C device mode */
		I2C_CFG_Type *i2c_cfg;
		i2c_cfg = p_data->i2c_cfg;
		int slv_mode = i2c_cfg->masterSlaveMode;
		if (slv_mode)
			return i2c_drv_slave_dma_read(dev, buf, nbytes);
		else
			/* Fixme: i2c read for master over dma */
			return -WM_FAIL;
	}

	else
		return i2c_drv_pio_read(dev, buf, nbytes);

}

static int i2c_drv_pio_write(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	uint32_t sentlength = 0;
	I2C_CFG_Type *i2c_cfg;
	mdev_i2c_priv_data_t *priv_data_p;
	priv_data_p = (mdev_i2c_priv_data_t *) dev->private_data;
	i2c_cfg = priv_data_p->i2c_cfg;
	uint32_t t1 = os_get_timestamp(), t2, tout = tx_tout[dev->port_id];

	if (i2c_cfg->masterSlaveMode == I2C_MASTER) {
		ret = os_mutex_get(&i2c_mutex[dev->port_id], OS_WAIT_FOREVER);
		if (ret == -WM_FAIL) {
			i2c_w("failed to get mutex");
			return -WM_FAIL;
		}
		while (sentlength < nbytes) {
			if (SET == I2C_GetStatus(dev->port_id,
						I2C_STATUS_TFNF)) {
				I2C_SendByte(dev->port_id, buf[sentlength]);
				sentlength++;
				t1 = os_get_timestamp();
			} else if (tout) {
				t2 = os_get_timestamp();
				if ((((t2 > t1) && ((t2 - t1) > tout)) ||
					((t1 > t2) &&
					(UINT_MAX - t1 + t2) > tout))) {
					sentlength = -WM_E_AGAIN;
					i2c_d("tx timeout");
					break;
				}
			}
		}
		os_mutex_put(&i2c_mutex[dev->port_id]);
	} else {

		sentlength = nbytes;
		priv_data_p->tx_buf.buf_size = nbytes;
		priv_data_p->tx_buf.buf_p = (char *)buf;
		priv_data_p->tx_buf.buf_head = 0;
		int sem_status =
		    os_semaphore_get(&write_slave_sem[dev->port_id],
				     SLAVE_WRITE_TIMEOUT);

		/* if semaphore time-out occurs set sentlength = 0 */
		if (sem_status != WM_SUCCESS) {
			/* Reset the ring buffer on timeout */
			priv_data_p->tx_buf.buf_size = 0;
			priv_data_p->tx_buf.buf_p = NULL;
			sentlength = 0;
		}
	}
	return sentlength;
}

int i2c_drv_write(mdev_t *dev, uint8_t *buf, uint32_t nbytes)
{
	mdev_i2c_priv_data_t *p_data = (mdev_i2c_priv_data_t *)
	    dev->private_data;

	/* check if dma is enabled */
	if (p_data->dma == I2C_DMA_ENABLE)
		return i2c_drv_dma_write(dev, buf, nbytes);

	else
		return i2c_drv_pio_write(dev, buf, nbytes);
}

int i2c_drv_get_status_bitmap(mdev_t *mdev_p, uint32_t *status)
{
	if (status == NULL)
		return -WM_E_INVAL;

	if (mdev_p == NULL)
		return -WM_E_INVAL;

	uint32_t port_no = mdev_p->port_id;

	/* check if I2C port is configured as a master or slave */
	mdev_i2c_priv_data_t *p_data;

	p_data = (mdev_i2c_priv_data_t *) mdev_p->private_data;

	I2C_CFG_Type *i2c_cfg;

	i2c_cfg = p_data->i2c_cfg;

	int mode = i2c_cfg->masterSlaveMode;

	*status = 0;

	/* check if I2C transaction has aborted */
	if (I2C_GetRawIntStatus(port_no, I2C_INT_TX_ABORT)) {
		uint32_t tx_abrt_reason;
		I2C_GetTxAbortCause(port_no, &tx_abrt_reason);
		i2c_d("I2C Tx_aborted,reason:%d", tx_abrt_reason);
		I2C_IntClr(port_no, I2C_INT_TX_ABORT);
		*status = I2C_ERROR;
		return WM_SUCCESS;
	}

	/* if i2c slave mode check for slave active bit */
	if (mode) {
		if (I2C_GetStatus(port_no, I2C_STATUS_SLV_ACTIVITY))
			*status = I2C_ACTIVE;
		else
			*status = I2C_INACTIVE;

		/* if i2c master mode check for master active bit */
	} else if (I2C_GetStatus(port_no, I2C_STATUS_MST_ACTIVITY)) {
		*status = I2C_ACTIVE;
	}
	/* if none of the conditions checked above is true set I2C_INACTIVE */
	else
		*status = I2C_INACTIVE;

	return WM_SUCCESS;
}

/* Exposed APIs and helper functions */

int i2c_drv_xfer_mode(I2C_ID_Type i2c_id, I2C_DMA_MODE dma_mode)
{
	mdev_t *mdev_p;
	mdev_i2c_priv_data_t *p_data;

	if (i2c_id < 0 || i2c_id >= NUM_MC200_I2C_PORTS) {
		i2c_e("Port %d not present", i2c_id);
		return -WM_FAIL;
	}

	if (!(dma_mode == I2C_DMA_ENABLE || dma_mode == I2C_DMA_DISABLE)) {
		return -WM_FAIL;
	}

	mdev_p = mdev_get_handle(mdev_i2c_name[i2c_id]);

	if (!mdev_p) {
		i2c_e("Unable to open device %s", mdev_i2c_name[i2c_id]);
		return -WM_FAIL;
	}
	p_data = (mdev_i2c_priv_data_t *) mdev_p->private_data;
	p_data->dma = dma_mode;
	return WM_SUCCESS;

}

mdev_t *i2c_drv_open(I2C_ID_Type i2c_id, uint32_t flags)
{
	i2c_entry();
	int mode;
	I2C_CFG_Type *i2c_cfg;
	mdev_i2c_priv_data_t *p_data;

	mdev_t *mdev_p = mdev_get_handle(mdev_i2c_name[i2c_id]);

	if (mdev_p == NULL) {
		i2c_w("driver open called without registering device"
		      " (%s)", mdev_i2c_name[i2c_id]);
		return NULL;
	}

	p_data = (mdev_i2c_priv_data_t *) mdev_p->private_data;
	i2c_cfg = p_data->i2c_cfg;

	int dma_enabled = p_data->dma;
	/* Configure pinmux for i2c pins */
	board_i2c_pin_config(mdev_p->port_id);

	/* Disable i2c */
	I2C_Disable(mdev_p->port_id);

	if (flags & I2C_DEVICE_SLAVE) {
		/* config for slave device */
		mode = I2C_SLAVE;
		i2c_cfg->masterSlaveMode = I2C_SLAVE;
		i2c_cfg->ownSlaveAddr = flags & 0x0ff;

		/* Set the default bit mode to 7 bit mode */
		i2c_cfg->slaveAddrBitMode = I2C_ADDR_BITS_7;

		if (flags & I2C_10_BIT_ADDR)
			i2c_cfg->slaveAddrBitMode = I2C_ADDR_BITS_10;

	} else {

		/* config for master device */
		mode = I2C_MASTER;
		i2c_cfg->masterSlaveMode = I2C_MASTER;

		/* Set the default bit mode to 7 bit mode */
		i2c_cfg->masterAddrBitMode = I2C_ADDR_BITS_7;
		I2C_SetSlaveAddr(mdev_p->port_id, flags & 0x0ff);

		/* for 10 bit addressing mode (if enabled) */
		if (flags & I2C_10_BIT_ADDR) {
			i2c_cfg->masterAddrBitMode = I2C_ADDR_BITS_10;
			I2C_SetSlaveAddr(mdev_p->port_id, flags & 0x3ff);
		}
	}

	i2c_cfg->restartEnable = ENABLE;

	/* Configure High Count and Low Count registers
	 * HCNT =  cpu_freq_MHz * HIGH_TIME(corresponding to I2C speed)
	 * LCNT =  cpu_freq_MHz * LOW_TIME(corresponding to I2C speed)
	 */
	int scl_high, scl_low;
	if (flags & I2C_CLK_400KHZ) {

		i2c_cfg->speedMode = I2C_SPEED_FAST;	/* 400KHz */

		/* check if user has tuned I2C clkcnt */
		if (is_clkcnt_set == SET) {
			scl_high = p_data->i2c_clkcnt->high_time;
			scl_low =  p_data->i2c_clkcnt->low_time;
		} else {
			scl_high = MIN_SCL_HIGH_TIME_FAST;
			scl_low = MIN_SCL_LOW_TIME_FAST;
		}
		if ((cpu_freq_MHz() * scl_high) / 1000 >
		    FAST_HCNT)
			FAST_HCNT = cpu_freq_MHz() * scl_high / 1000;

		if ((cpu_freq_MHz() * scl_low / 1000) > FAST_LCNT)
			FAST_LCNT = cpu_freq_MHz() * scl_low / 1000;

	} else {

		i2c_cfg->speedMode = I2C_SPEED_STANDARD; /* 100KHz Default */
		scl_high = p_data->i2c_clkcnt->high_time;
		if ((cpu_freq_MHz() * scl_high / 1000) >
		    STANDARD_HCNT)
			STANDARD_HCNT = cpu_freq_MHz() * scl_high / 1000;

		scl_low = p_data->i2c_clkcnt->low_time;
		if ((cpu_freq_MHz() * scl_low / 1000) >
		    STANDARD_LCNT)
			STANDARD_LCNT = cpu_freq_MHz() * scl_low / 1000;
	}

	/* configure registers required to set I2C_CLK speed before I2C_Init */

	I2C_SpeedClkCnt_Type clnt = { STANDARD_HCNT, STANDARD_LCNT,
		FAST_HCNT, FAST_LCNT, HS_HCNT, HS_LCNT
	};

	I2C_SetClkCount(mdev_p->port_id, &clnt);

	/* I2C init */
	I2C_Init(mdev_p->port_id, i2c_cfg);

	if (mode == I2C_SLAVE) {
		/* in slave mode, create a semaphore to wait on read
		 * request from master device */
		char sem_name[SEMAPHORE_NAME_LEN];
		snprintf(sem_name, sizeof(sem_name), "i2c-write-sem%d",
			 mdev_p->port_id);
		int ret = os_semaphore_create(&write_slave_sem[mdev_p->port_id],
					      sem_name);
		if (ret == -WM_FAIL) {
			i2c_e("Failed to create semaphore");
			return NULL;
		}
		os_semaphore_get(&write_slave_sem[mdev_p->port_id],
				 OS_WAIT_FOREVER);

		/* Fifo config */
		I2C_FIFOConfig(mdev_p->port_id, &i2cFifoCfg);
		/* if rx_buffer is not set by the user set it to default size */
		if (GET_RX_BUF_SIZE(p_data) == 0) {
			SET_RX_BUF_SIZE(p_data, I2C_RX_BUF_SIZE);
		}
		/* init ring buffer */
		if (rx_buf_init(p_data) != WM_SUCCESS) {
			i2c_e("Unable to allocate rx buffer");
			return NULL;
		}

		/* Mask all interrupts */
		I2C_IntMask(mdev_p->port_id, I2C_INT_ALL, MASK);
		/* Unmask I2C_INT_RD_REQ interrupt */
		I2C_IntMask(mdev_p->port_id, I2C_INT_RD_REQ, UNMASK);

		/* Register I2C IRQ call back function */
		install_int_callback(I2C_INT_BASE + mdev_p->port_id,
				     I2C_INT_RD_REQ,
				     i2c_write_irq_handler[mdev_p->port_id]);

		/* For PIO mode (non-DMA mode) in slave device
		 * install callback for RX_FULL */
		if (!dma_enabled) {
			install_int_callback(I2C_INT_BASE + mdev_p->port_id,
					     I2C_INT_RX_FULL,
					     i2c_read_irq_handler
					     [mdev_p->port_id]);
		}
		/* Enable i2c module interrupt */
		NVIC_EnableIRQ(I2C_IRQn_BASE + mdev_p->port_id);
		NVIC_SetPriority(I2C_IRQn_BASE + mdev_p->port_id, 0xf);
	}

	if (dma_enabled) {
		char i2c_tx_sem_name[SEMAPHORE_NAME_LEN];
		snprintf(i2c_tx_sem_name, sizeof(i2c_tx_sem_name),
			 "i2c-dma-tx-sem%d", mdev_p->port_id);
		int ret = os_semaphore_create(&i2c_dma_tx_sem[mdev_p->port_id],
					      i2c_tx_sem_name);

		if (ret) {
			i2c_e("Failed to initialize semaphore");
			return NULL;
		}
		os_semaphore_get(&i2c_dma_tx_sem[mdev_p->port_id],
				 OS_WAIT_FOREVER);

		char i2c_rx_sem_name[SEMAPHORE_NAME_LEN];
		snprintf(i2c_rx_sem_name, sizeof(i2c_rx_sem_name),
			 "i2c-dma-rx-sem%d", mdev_p->port_id);
		ret = os_semaphore_create(&i2c_dma_rx_sem
					  [mdev_p->port_id], i2c_rx_sem_name);

		if (ret) {
			i2c_e("Failed to initialize semaphore");
			return NULL;
		}
		os_semaphore_get(&i2c_dma_rx_sem[mdev_p->port_id],
				 OS_WAIT_FOREVER);

	}

	/* Enable i2c */
	I2C_Enable(mdev_p->port_id);

	return mdev_p;
}

int i2c_drv_close(mdev_t *dev)
{
	i2c_entry();
	uint32_t status;
	mdev_i2c_priv_data_t *i2c_data_p =
	    (mdev_i2c_priv_data_t *) dev->private_data;

	if (mdev_get_handle(mdev_i2c_name[dev->port_id]) == NULL) {
		i2c_w("trying to close unregistered device");
		return -WM_FAIL;
	}

	while (1) {
		i2c_drv_get_status_bitmap(dev, &status);
		if (status != I2C_ACTIVE)
			break;
		os_thread_relinquish();
	}

	I2C_Disable(dev->port_id);
	NVIC_DisableIRQ(I2C_IRQn_BASE + dev->port_id);
	install_int_callback(I2C_INT_BASE + dev->port_id, I2C_INT_RD_REQ, 0);
	install_int_callback(I2C_INT_BASE + dev->port_id, I2C_INT_RX_FULL, 0);

	if (i2c_data_p->rx_ringbuf.buf_p) {
		os_mem_free(i2c_data_p->rx_ringbuf.buf_p);
		i2c_data_p->rx_ringbuf.buf_p = NULL;
		i2c_data_p->rx_ringbuf.buf_size = 0;
	}

	return WM_SUCCESS;
}

static void i2c_drv_mdev_init(uint8_t port_num)
{
	mdev_t *mdev_p;
	mdev_i2c_priv_data_t *priv_data_p;

	mdev_p = &mdev_i2c[port_num];

	mdev_p->name = mdev_i2c_name[port_num];
	mdev_p->port_id = port_num;
	mdev_p->private_data = (uint32_t) &i2cdev_priv_data_[port_num];
	priv_data_p = (mdev_i2c_priv_data_t *) &i2cdev_priv_data_[port_num];
	priv_data_p->i2c_cfg = (I2C_CFG_Type *) &i2cdev_data[port_num];
	priv_data_p->tx_buf.buf_size = 0;
	priv_data_p->tx_buf.buf_p = NULL;
	priv_data_p->rx_ringbuf.buf_size = 0;
	/* clock configuration for I2C port */
	priv_data_p->i2c_clkcnt = (i2c_drv_clkcnt *) &i2cdev_clk_data[port_num];
	priv_data_p->i2c_clkcnt->high_time = MIN_SCL_HIGH_TIME_STANDARD;
	priv_data_p->i2c_clkcnt->low_time = MIN_SCL_LOW_TIME_STANDARD;
}

void i2c_drv_set_clkcnt(I2C_ID_Type i2c_id, int hightime, int lowtime)
{
	mdev_i2c_priv_data_t *priv_data_p;
	priv_data_p = (mdev_i2c_priv_data_t *) &i2cdev_priv_data_[i2c_id];

	priv_data_p->i2c_clkcnt->high_time = hightime;
	priv_data_p->i2c_clkcnt->low_time = lowtime;
	is_clkcnt_set = SET;
}

int i2c_drv_rxbuf_size(I2C_ID_Type i2c_id, uint32_t size)
{

	mdev_t *mdev_p;
	mdev_i2c_priv_data_t *i2c_data_p;

	if (size < 0)
		return -WM_FAIL;

	/*check i2c_id is initialized */
	mdev_p = mdev_get_handle(mdev_i2c_name[i2c_id]);
	if (!mdev_p) {
		i2c_e("Unable to open device %s", mdev_i2c_name[i2c_id]);
		return -WM_FAIL;
	}
	i2c_data_p = (mdev_i2c_priv_data_t *) mdev_p->private_data;
	SET_RX_BUF_SIZE(i2c_data_p, size);
	return WM_SUCCESS;
}

void i2c_drv_timeout(I2C_ID_Type id, uint32_t txtout, uint32_t rxtout)
{
	mdev_t *mdev_p = mdev_get_handle(mdev_i2c_name[id]);

	if (!mdev_p) {
		i2c_e("Unable to open device %s", mdev_i2c_name[id]);
		return;
	}
	tx_tout[id] = txtout;
	rx_tout[id] = rxtout;
}

int i2c_drv_init(I2C_ID_Type id)
{
	int ret;

	if (mdev_get_handle(mdev_i2c_name[id]) != NULL)
		return WM_SUCCESS;

	ret = os_mutex_create(&i2c_mutex[id], mdev_i2c_name[id],
			      OS_MUTEX_INHERIT);
	if (ret == -WM_FAIL) {
		i2c_e("Failed to create mutex.");
		return -WM_FAIL;
	}

	/* Initialize the i2c port's mdev structure */
	i2c_drv_mdev_init(id);

	/* Set default tx/rx timeout */
	tx_tout[id] = 0;
	rx_tout[id] = 0;

	/* Enable I2Cx Clock */
	switch (id) {
	case 0:
		CLK_ModuleClkEnable(CLK_I2C0);
		break;
	case 1:
		CLK_ModuleClkEnable(CLK_I2C1);
		break;
	case 2:
		CLK_ModuleClkEnable(CLK_I2C2);
		break;
	}

	/* Register the I2C device driver for this port */
	mdev_register(&mdev_i2c[id]);

	fill_cb_array();

	return WM_SUCCESS;
}
