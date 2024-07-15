#define DT_DRV_COMPAT ti_omap_i2c

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(omap_i2c);

struct i2c_omap_cfg {
	mm_reg_t base;
	int irq;
	int reg_shift;
	uint32_t speed;
};

struct i2c_omap_data {
	struct device *dev;
	struct k_sem cmd_complete;
	uint16_t cmd_err;
	uint16_t iestate;
	uint16_t scllstate;
	uint16_t sclhstate;
	uint16_t pscstate;
	uint8_t *buf;
	uint8_t *regs;
	size_t buf_len;
	uint32_t flags;
	uint16_t westate;
	uint8_t fifo_size;
	unsigned receiver: 1;
	unsigned b_hw: 1; /* bad h/w fixes */
	unsigned bb_valid: 1;
	uint32_t latency;
	uint8_t threshold;
	void (*set_mpu_wkup_lat)(struct device *dev, uint32_t latency);
};

#define OMAP_I2C_FLAG_SIMPLE_CLOCK        BIT(1)
#define OMAP_I2C_FLAG_FORCE_19200_INT_CLK BIT(6)

/* I2C Interrupt Enable Register (OMAP_I2C_IE): */
#define OMAP_I2C_IE_XDR  (1 << 14) /* TX Buffer drain int enable */
#define OMAP_I2C_IE_RDR  (1 << 13) /* RX Buffer drain int enable */
#define OMAP_I2C_IE_XRDY (1 << 4)  /* TX data ready int enable */
#define OMAP_I2C_IE_RRDY (1 << 3)  /* RX data ready int enable */
#define OMAP_I2C_IE_ARDY (1 << 2)  /* Access ready int enable */
#define OMAP_I2C_IE_NACK (1 << 1)  /* No ack interrupt enable */
#define OMAP_I2C_IE_AL   (1 << 0)  /* Arbitration lost int ena */

/* I2C Status Register (OMAP_I2C_STAT): */
#define OMAP_I2C_STAT_XDR  (1 << 14) /* TX Buffer draining */
#define OMAP_I2C_STAT_RDR  (1 << 13) /* RX Buffer draining */
#define OMAP_I2C_STAT_BB   (1 << 12) /* Bus busy */
#define OMAP_I2C_STAT_ROVR (1 << 11) /* Receive overrun */
#define OMAP_I2C_STAT_XUDF (1 << 10) /* Transmit underflow */
#define OMAP_I2C_STAT_AAS  (1 << 9)  /* Address as slave */
#define OMAP_I2C_STAT_BF   (1 << 8)  /* Bus Free */
#define OMAP_I2C_STAT_XRDY (1 << 4)  /* Transmit data ready */
#define OMAP_I2C_STAT_RRDY (1 << 3)  /* Receive data ready */
#define OMAP_I2C_STAT_ARDY (1 << 2)  /* Register access ready */
#define OMAP_I2C_STAT_NACK (1 << 1)  /* No ack interrupt enable */
#define OMAP_I2C_STAT_AL   (1 << 0)  /* Arbitration lost int ena */

/* I2C WE wakeup enable register */
#define OMAP_I2C_WE_XDR_WE  (1 << 14) /* TX drain wakup */
#define OMAP_I2C_WE_RDR_WE  (1 << 13) /* RX drain wakeup */
#define OMAP_I2C_WE_AAS_WE  (1 << 9)  /* Address as slave wakeup*/
#define OMAP_I2C_WE_BF_WE   (1 << 8)  /* Bus free wakeup */
#define OMAP_I2C_WE_STC_WE  (1 << 6)  /* Start condition wakeup */
#define OMAP_I2C_WE_GC_WE   (1 << 5)  /* General call wakeup */
#define OMAP_I2C_WE_DRDY_WE (1 << 3)  /* TX/RX data ready wakeup */
#define OMAP_I2C_WE_ARDY_WE (1 << 2)  /* Reg access ready wakeup */
#define OMAP_I2C_WE_NACK_WE (1 << 1)  /* No acknowledgment wakeup */
#define OMAP_I2C_WE_AL_WE   (1 << 0)  /* Arbitration lost wakeup */

#define OMAP_I2C_WE_ALL                                                                            \
	(OMAP_I2C_WE_XDR_WE | OMAP_I2C_WE_RDR_WE | OMAP_I2C_WE_AAS_WE | OMAP_I2C_WE_BF_WE |        \
	 OMAP_I2C_WE_STC_WE | OMAP_I2C_WE_GC_WE | OMAP_I2C_WE_DRDY_WE | OMAP_I2C_WE_ARDY_WE |      \
	 OMAP_I2C_WE_NACK_WE | OMAP_I2C_WE_AL_WE)

/* timeout waiting for the controller to respond */
#define OMAP_I2C_TIMEOUT K_MSEC(1000)

/* I2C Buffer Configuration Register (OMAP_I2C_BUF): */
#define OMAP_I2C_BUF_RDMA_EN   (1 << 15) /* RX DMA channel enable */
#define OMAP_I2C_BUF_RXFIF_CLR (1 << 14) /* RX FIFO Clear */
#define OMAP_I2C_BUF_XDMA_EN   (1 << 7)  /* TX DMA channel enable */
#define OMAP_I2C_BUF_TXFIF_CLR (1 << 6)  /* TX FIFO Clear */

/* I2C Configuration Register (OMAP_I2C_CON): */
#define OMAP_I2C_CON_EN        (1 << 15) /* I2C module enable */
#define OMAP_I2C_CON_BE        (1 << 14) /* Big endian mode */
#define OMAP_I2C_CON_OPMODE_HS (1 << 12) /* High Speed support */
#define OMAP_I2C_CON_STB       (1 << 11) /* Start byte mode (master) */
#define OMAP_I2C_CON_MST       (1 << 10) /* Master/slave mode */
#define OMAP_I2C_CON_TRX       (1 << 9)  /* TX/RX mode (master only) */
#define OMAP_I2C_CON_XA        (1 << 8)  /* Expand address */
#define OMAP_I2C_CON_RM        (1 << 2)  /* Repeat mode (master only) */
#define OMAP_I2C_CON_STP       (1 << 1)  /* Stop cond (master only) */
#define OMAP_I2C_CON_STT       (1 << 0)  /* Start condition (master) */

/* I2C SCL time value when Master */
#define OMAP_I2C_SCLL_HSSCLL 8
#define OMAP_I2C_SCLH_HSSCLH 8

/* I2C System Test Register (OMAP_I2C_SYSTEST): */
#define OMAP_I2C_SYSTEST_ST_EN       (1 << 15) /* System test enable */
#define OMAP_I2C_SYSTEST_FREE        (1 << 14) /* Free running mode */
#define OMAP_I2C_SYSTEST_TMODE_MASK  (3 << 12) /* Test mode select */
#define OMAP_I2C_SYSTEST_TMODE_SHIFT (12)      /* Test mode select */
/* Functional mode */
#define OMAP_I2C_SYSTEST_SCL_I_FUNC  (1 << 8) /* SCL line input value */
#define OMAP_I2C_SYSTEST_SCL_O_FUNC  (1 << 7) /* SCL line output value */
#define OMAP_I2C_SYSTEST_SDA_I_FUNC  (1 << 6) /* SDA line input value */
#define OMAP_I2C_SYSTEST_SDA_O_FUNC  (1 << 5) /* SDA line output value */
/* SDA/SCL IO mode */
#define OMAP_I2C_SYSTEST_SCL_I       (1 << 3) /* SCL line sense in */
#define OMAP_I2C_SYSTEST_SCL_O       (1 << 2) /* SCL line drive out */
#define OMAP_I2C_SYSTEST_SDA_I       (1 << 1) /* SDA line sense in */
#define OMAP_I2C_SYSTEST_SDA_O       (1 << 0) /* SDA line drive out */

/* OCP_SYSSTATUS bit definitions */
#define SYSS_RESETDONE_MASK (1 << 0)

/* struct omap_i2c_bus_platform_data .flags meanings */

#define OMAP_I2C_FLAG_NO_FIFO             BIT(0)
#define OMAP_I2C_FLAG_SIMPLE_CLOCK        BIT(1)
#define OMAP_I2C_FLAG_16BIT_DATA_REG      BIT(2)
#define OMAP_I2C_FLAG_ALWAYS_ARMXOR_CLK   BIT(5)
#define OMAP_I2C_FLAG_FORCE_19200_INT_CLK BIT(6)

/* OCP_SYSCONFIG bit definitions */
#define SYSC_CLOCKACTIVITY_MASK (0x3 << 8)
#define SYSC_SIDLEMODE_MASK     (0x3 << 3)
#define SYSC_ENAWAKEUP_MASK     (1 << 2)
#define SYSC_SOFTRESET_MASK     (1 << 1)
#define SYSC_AUTOIDLE_MASK      (1 << 0)

#define SYSC_IDLEMODE_SMART     0x2
#define SYSC_CLOCKACTIVITY_FCLK 0x2

/* Errata definitions */
#define I2C_OMAP_ERRATA_I207 (1 << 0)
#define I2C_OMAP_ERRATA_I462 (1 << 1)

#define OMAP_I2C_IP_V2_INTERRUPTS_MASK 0x6FFF

#define I2C_REVNB_LO        0x0
#define I2C_REVNB_HI        0x4
#define I2C_SYSC            0x10
#define I2C_EOI             0x20
#define I2C_IRQSTATUS_RAW   0x24
#define I2C_IRQSTATUS       0x28
#define I2C_IRQENABLE_SET   0x2c
#define I2C_IRQENABLE_CLR   0x30
#define I2C_WE              0x34
#define I2C_DMARXENABLE_SET 0x38
#define I2C_DMATXENABLE_SET 0x3c
#define I2C_DMARXENABLE_CLR 0x40
#define I2C_DMATXENABLE_CLR 0x44
#define I2C_DMARXWAKE_EN    0x48
#define I2C_DMATXWAKE_EN    0x4c
#define I2C_IE              0x84
#define I2C_STAT            0x88
#define I2C_SYSS            0x90
#define I2C_BUF             0x94
#define I2C_CNT             0x98
#define I2C_DATA            0x9c
#define I2C_CON             0xa4
#define I2C_OA              0xa8
#define I2C_SA              0xac
#define I2C_PSC             0xb0
#define I2C_SCLL            0xb4
#define I2C_SCLH            0xb8
#define I2C_SYSTEST         0xbc
#define I2C_BUFSTAT         0xc0
#define I2C_OA1             0xc4
#define I2C_OA2             0xc8
#define I2C_OA3             0xcc
#define I2C_ACTOA           0xd0
#define I2C_SBLOCK          0xd4

static int omap_i2c_xfer_data(const struct device *dev);

static inline void omap_i2c_write_reg(const struct i2c_omap_cfg *cfg, struct i2c_omap_data *data,
				      int reg, uint16_t val)
{
	sys_write16(val, cfg->base + (data->regs[reg] << cfg->reg_shift));
}

static inline uint16_t omap_i2c_read_reg(const struct i2c_omap_cfg *cfg, struct i2c_omap_data *data,
					 int reg)
{
	return sys_read16(cfg->base + (data->regs[reg] << cfg->reg_shift));
}

static void __omap_i2c_init(const struct device *dev)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;

	// Write to I2C_CON to reset I2C module
	omap_i2c_write_reg(cfg, data, I2C_CON, 0);

	// Setup clock prescaler to obtain approx 12MHz I2C module clock:
	omap_i2c_write_reg(cfg, data, I2C_PSC, data->pscstate);

	// SCL low and high time values
	omap_i2c_write_reg(cfg, data, I2C_SCLL, data->scllstate);
	omap_i2c_write_reg(cfg, data, I2C_SCLH, data->sclhstate);

	// Take the I2C module out of reset
	omap_i2c_write_reg(cfg, data, I2C_CON, OMAP_I2C_CON_EN);

	// Delay to allow the bus to settle
	k_busy_wait(1);

	// Conditionally write to IE register if iestate is set
	if (data->iestate) {
		omap_i2c_write_reg(cfg, data, I2C_IRQENABLE_SET, data->iestate);
	}
}

static int omap_i2c_reset(const struct device *dev)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;

	uint64_t timeout;
	uint16_t sysc;

	sysc = omap_i2c_read_reg(cfg, data, I2C_SYSC);
	// Disable I2C controller before soft reset
	omap_i2c_write_reg(cfg, data, I2C_CON,
			   omap_i2c_read_reg(cfg, data, I2C_CON) & ~(OMAP_I2C_CON_EN));

	timeout = k_uptime_get() + 1000;

	omap_i2c_write_reg(cfg, data, I2C_CON, OMAP_I2C_CON_EN);

	while (!(omap_i2c_read_reg(cfg, data, I2C_SYSS) & SYSS_RESETDONE_MASK)) {
		if (k_uptime_get() > timeout) {
			LOG_WRN("timeout waiting for controller reset");
			return -ETIMEDOUT;
		}
		k_msleep(1);
	}
	// SYSC register is cleared by the reset; rewrite it
	omap_i2c_write_reg(cfg, data, I2C_SYSC, sysc);
	// Schedule I2C-bus monitoring on the next transfer
	data->bb_valid = 0;
	return 0;
}

static int omap_i2c_init(const struct device *dev)
{
	struct i2c_omap_data *data = dev->data;
	uint16_t psc = 0, scll = 0, sclh = 0;
	unsigned long fclk_rate = 12000000;
	unsigned long internal_clk = 0;

	data->westate = OMAP_I2C_WE_ALL;

	// if (omap->flags & OMAP_I2C_FLAG_ALWAYS_ARMXOR_CLK) {
	// 	clk_dev = device_get_binding("CLOCK_CONTROL");
	// 	if (!clk_dev) {
	// 		LOG_ERR("Failed to get clock device binding");
	// 		return -ENODEV;
	// 	}

	// 	if (clock_control_get_rate(clk_dev, CLOCK_CONTROL_SUBSYS_BASE, &fclk_rate)) {
	// 		LOG_ERR("Could not get fck rate");
	// 		return -EIO;
	// 	}

	// 	if (fclk_rate > 12000000) {
	// 		psc = fclk_rate / 12000000;
	// 	}
	// }

	// if (!(omap->flags & OMAP_I2C_FLAG_SIMPLE_CLOCK)) {
	// 	if (omap->speed > 400 || omap->flags & OMAP_I2C_FLAG_FORCE_19200_INT_CLK) {
	// 		internal_clk = 19200;
	// 	} else if (omap->speed > 100) {
	// 		internal_clk = 9600;
	// 	} else {
	// 		internal_clk = 4000;
	// 	}

	// 	clk_dev = device_get_binding("CLOCK_CONTROL");
	// 	if (!clk_dev) {
	// 		LOG_ERR("Failed to get clock device binding");
	// 		return -ENODEV;
	// 	}

	// 	if (clock_control_get_rate(clk_dev, CLOCK_CONTROL_SUBSYS_BASE, &fclk_rate)) {
	// 		LOG_ERR("Could not get fck rate");
	// 		return -EIO;
	// 	}
	// 	fclk_rate /= 1000;

	// 	psc = fclk_rate / internal_clk;
	// 	psc = psc - 1;

	// 	if (omap->speed > 400) {
	// 		unsigned long scl;

	// 		scl = internal_clk / 400;
	// 		fsscll = scl - (scl / 3) - 7;
	// 		fssclh = (scl / 3) - 5;

	// 		scl = fclk_rate / omap->speed;
	// 		hsscll = scl - (scl / 3) - 7;
	// 		hssclh = (scl / 3) - 5;
	// 	} else if (omap->speed > 100) {
	// 		unsigned long scl;

	// 		scl = internal_clk / omap->speed;
	// 		fsscll = scl - (scl / 3) - 7;
	// 		fssclh = (scl / 3) - 5;
	// 	} else {
	// 		fsscll = internal_clk / (omap->speed * 2) - 7;
	// 		fssclh = internal_clk / (omap->speed * 2) - 5;
	// 	}
	// 	scll = (hsscll << OMAP_I2C_SCLL_HSSCLL) | fsscll;
	// 	sclh = (hssclh << OMAP_I2C_SCLH_HSSCLH) | fssclh;
	// } else {
	// 	fclk_rate /= (psc + 1) * 1000;
	// 	if (psc > 2) {
	// 		psc = 2;
	// 	}
	// 	scll = fclk_rate / (omap->speed * 2) - 7 + psc;
	// 	sclh = fclk_rate / (omap->speed * 2) - 7 + psc;
	// }

	// As there is no clock control module, we will be fixing it to 100 Kbps
	// For simplicity, always set the internal clock to 4000 kHz for 100 kbps operation
	internal_clk = 4000;

	// Prescaler Calculation and clock periods for 100 kbps
	psc = (fclk_rate / (internal_clk * 1000)) - 1;
	scll = internal_clk / (100 * 2) - 7;
	sclh = internal_clk / (100 * 2) - 5;

	data->iestate = (OMAP_I2C_IE_XRDY | OMAP_I2C_IE_RRDY | OMAP_I2C_IE_ARDY | OMAP_I2C_IE_NACK |
			 OMAP_I2C_IE_AL) |
			((data->fifo_size) ? (OMAP_I2C_IE_RDR | OMAP_I2C_IE_XDR) : 0);

	data->pscstate = psc;
	data->scllstate = scll;
	data->sclhstate = sclh;

	__omap_i2c_init(dev);

	return 0;
}

static int omap_i2c_transmit_data(const struct device *dev, uint8_t num_bytes, bool is_xdr)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	uint16_t w;

	while (num_bytes--) {
		w = *data->buf++;
		data->buf_len--;

		omap_i2c_write_reg(cfg, data, I2C_DATA, w);
	}

	return 0;
}

static void omap_i2c_receive_data(const struct device *dev, uint8_t num_bytes, bool is_rdr)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	uint16_t w;
	while (num_bytes--) {
		w = omap_i2c_read_reg(cfg, data, I2C_DATA);
		*data->buf++ = w >> 8;
		data->buf_len--;
	}
}

static void omap_i2c_resize_fifo(const struct device *dev, uint8_t size, bool is_rx)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	uint16_t buf;
	if (data->flags & OMAP_I2C_FLAG_NO_FIFO) {
		return;
	}

	// Set up notification threshold based on message size
	data->threshold = CLAMP(size, (uint8_t)1, data->fifo_size);
	buf = omap_i2c_read_reg(cfg, data, I2C_BUF);

	if (is_rx) {
		// Clear RX threshold
		buf &= ~(0x3f << 8);
		buf |= ((data->threshold - 1) << 8) | OMAP_I2C_BUF_RXFIF_CLR;
	} else {
		// Clear TX threshold
		buf &= ~0x3f;
		buf |= (data->threshold - 1) | OMAP_I2C_BUF_TXFIF_CLR;
	}
	omap_i2c_write_reg(cfg, data, I2C_BUF, buf);

	// Calculate wakeup latency constraint for MPU
	if (data->set_mpu_wkup_lat != NULL) {
		data->latency = (1000000 * data->threshold) / (1000 * cfg->speed / 8);
		data->set_mpu_wkup_lat(data->dev, data->latency);
	}
}

static void omap_i2c_wait(const struct device *dev)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	uint16_t stat;
	uint16_t mask = omap_i2c_read_reg(cfg, data, I2C_IE);
	int count = 0;

	do {
		stat = omap_i2c_read_reg(cfg, data, I2C_STAT);
		count++;
	} while (!(stat & mask) && count < 5);
}

static int omap_i2c_xfer_msg(const struct device *dev, struct i2c_msg *msg, int stop, bool polling,
			     uint16_t addr)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	unsigned long time_left;
	uint16_t w;
	int ret;

	LOG_DBG("addr: 0x%04x, len: %d, flags: 0x%x, stop: %d", addr, msg->len, msg->flags, stop);

	data->receiver = !!(msg->flags & I2C_MSG_READ);
	omap_i2c_resize_fifo(dev, msg->len, data->receiver);
	omap_i2c_write_reg(cfg, data, I2C_SA, addr);
	data->buf = msg->buf;
	data->buf_len = msg->len;
	/* make sure writes to data->buf_len are ordered */

	compiler_barrier();

	omap_i2c_write_reg(cfg, data, I2C_CNT, data->buf_len);

	/* Clear the FIFO Buffers */
	w = omap_i2c_read_reg(cfg, data, I2C_BUF);
	w |= OMAP_I2C_BUF_RXFIF_CLR | OMAP_I2C_BUF_TXFIF_CLR;
	omap_i2c_write_reg(cfg, data, I2C_BUF, w);

	if (!polling) {
		k_sem_reset(&data->cmd_complete);
	}

	data->cmd_err = 0;
	w = OMAP_I2C_CON_EN | OMAP_I2C_CON_MST | OMAP_I2C_CON_STT;

	if (cfg->speed > 400) {
		w |= OMAP_I2C_CON_OPMODE_HS;
	}
	if (msg->flags & I2C_MSG_STOP) {
		stop = 1;
	}
	if (!(msg->flags & I2C_MSG_READ)) {
		w |= OMAP_I2C_CON_TRX;
	}
	if (!data->b_hw && stop) {
		w |= OMAP_I2C_CON_STP;
	}
	omap_i2c_write_reg(cfg, data, I2C_CON, w);

	if (data->b_hw && stop) {
		uint64_t current_time = k_uptime_get(); // Get current uptime in milliseconds
		uint64_t delay = current_time + 1000;   // Add timeout duration in ms
		uint16_t con = omap_i2c_read_reg(cfg, data, I2C_CON);
		while (con & OMAP_I2C_CON_STT) {
			con = omap_i2c_read_reg(cfg, data, I2C_CON);
			// Let the users know about the bad state
			if (k_uptime_get() > delay) {
				LOG_ERR("controller timed out waiting for start condition to "
					"finish");
				return -ETIMEDOUT;
			}
			k_sleep(K_MSEC(1)); // Yield to avoid busy looping
		}
		w |= OMAP_I2C_CON_STP;
		w &= OMAP_I2C_CON_STT;
		omap_i2c_write_reg(cfg, data, I2C_CON, w);
	}

	if (!polling) {
		/* Use Zephyr's synchronization primitives to wait for completion */
		int result = k_sem_take(&data->cmd_complete, OMAP_I2C_TIMEOUT);

		/* Check if waiting was successful */
		if (result == -EAGAIN)
			;

		{
			/* Timeout occurred, handle it accordingly */
			time_left = -ETIMEDOUT; // or any other timeout handling
		}
	} else {
		do {
			omap_i2c_wait(dev);
			ret = omap_i2c_xfer_data(dev);
		} while (ret == -EAGAIN);
		{
			time_left = !ret;
		}
	}

	if (time_left == 0) {
		omap_i2c_reset(dev);
		__omap_i2c_init(dev);
		return -ETIMEDOUT;
	}

	if (likely(!data->cmd_err)) {
		return 0;
	}

	if (data->cmd_err & (OMAP_I2C_STAT_ROVR | OMAP_I2C_STAT_XUDF)) {
		omap_i2c_reset(dev);
		__omap_i2c_init(dev);
		return -EIO;
	}
	if (data->cmd_err & OMAP_I2C_STAT_AL) {
		return -EAGAIN;
	}

	if (data->cmd_err & OMAP_I2C_STAT_NACK) {
		if (msg->flags) {
			return 0;
		}

		w = omap_i2c_read_reg(cfg, data, I2C_CON);
		w |= OMAP_I2C_CON_STP;
		omap_i2c_write_reg(cfg, data, I2C_CON, w);
		return -EREMOTE;
	}
	return -EIO;
}
static inline void omap_i2c_ack_stat(const struct device *dev, uint16_t stat)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	omap_i2c_write_reg(cfg, data, I2C_STAT, stat);
}
static int omap_i2c_xfer_data(const struct device *dev)
{
	const struct i2c_omap_cfg *cfg = dev->config;
	struct i2c_omap_data *data = dev->data;
	uint16_t bits, stat;
	int err = 0, count = 0;

	do {
		bits = omap_i2c_read_reg(cfg, data, I2C_IE);
		stat = omap_i2c_read_reg(cfg, data, I2C_STAT);
		stat &= bits;
		/* If we're in receiver mode, ignore XDR/XRDY */
		if (data->receiver) {
			stat &= ~(OMAP_I2C_STAT_XDR | OMAP_I2C_STAT_XRDY);
		} else {
			stat &= ~(OMAP_I2C_STAT_RDR | OMAP_I2C_STAT_RRDY);
		}

		if (!stat) {
			/* my work here is done */
			err = -EAGAIN;
			break;
		}
		LOG_DBG("IRQ (ISR = 0x%04x)", stat);
		if (count++ == 100) {
			LOG_WRN("Too much work in one IRQ");
			break;
		}

		if (stat & OMAP_I2C_STAT_NACK) {
			err |= OMAP_I2C_STAT_NACK;
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_NACK);
		}

		if (stat & OMAP_I2C_STAT_AL) {
			LOG_ERR("Arbitration lost");
			err |= OMAP_I2C_STAT_AL;
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_AL);
		}

		if (stat & OMAP_I2C_STAT_ARDY) {
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ARDY);
		}

		if (stat & (OMAP_I2C_STAT_ARDY | OMAP_I2C_STAT_NACK | OMAP_I2C_STAT_AL)) {
			omap_i2c_ack_stat(dev, (OMAP_I2C_STAT_RRDY | OMAP_I2C_STAT_RDR |
						OMAP_I2C_STAT_XRDY | OMAP_I2C_STAT_XDR |
						OMAP_I2C_STAT_ARDY));
			break;
		}

		if (stat & OMAP_I2C_STAT_RDR) {
			uint8_t num_bytes = 1;

			if (data->fifo_size) {
				num_bytes = data->buf_len;
			}
			omap_i2c_receive_data(dev, num_bytes, true);
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RDR);
			continue;
		}

		if (stat & OMAP_I2C_STAT_RRDY) {
			uint8_t num_bytes = 1;

			if (data->threshold) {
				num_bytes = data->threshold;
			}

			omap_i2c_receive_data(dev, num_bytes, false);
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_RRDY);
			continue;
		}

		if (stat & OMAP_I2C_STAT_XDR) {
			uint8_t num_bytes = 1;
			int ret;

			if (data->fifo_size) {
				num_bytes = data->buf_len;
			}

			ret = omap_i2c_transmit_data(dev, num_bytes, true);
			if (ret < 0) {
				break;
			}

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XDR);
			continue;
		}

		if (stat & OMAP_I2C_STAT_XRDY) {
			uint8_t num_bytes = 1;
			int ret;

			if (data->threshold) {
				num_bytes = data->threshold;
			}

			ret = omap_i2c_transmit_data(dev, num_bytes, false);
			if (ret < 0) {
				break;
			}

			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XRDY);
			continue;
		}

		if (stat & OMAP_I2C_STAT_ROVR) {
			LOG_ERR("Receive overrun");
			err |= OMAP_I2C_STAT_ROVR;
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_ROVR);
			break;
		}

		if (stat & OMAP_I2C_STAT_XUDF) {
			LOG_ERR("Transmit underflow");
			err |= OMAP_I2C_STAT_XUDF;
			omap_i2c_ack_stat(dev, OMAP_I2C_STAT_XUDF);
			break;
		}
	} while (stat);

	return err;
}

static int omap_i2c_xfer_common(const struct device *dev, struct i2c_msg msg[], int num,
				bool polling, uint16_t addr)
{
	struct i2c_omap_data *data = dev->data;
	int i, r = 0;

	if (data->set_mpu_wkup_lat != NULL) {
		data->set_mpu_wkup_lat(data->dev, data->latency);
	}

	for (i = 0; i < num; i++) {
		r = omap_i2c_xfer_msg(dev, &msg[i], (i == (num - 1)), polling, addr);
		if (r != 0) {
			break;
		}
	}

	if (r == 0) {
		r = num;
	}

	if (data->set_mpu_wkup_lat != NULL) {
		data->set_mpu_wkup_lat(data->dev, -1);
	}

	return r;
}

static int omap_i2c_transfer_irq(const struct device *dev, struct i2c_msg msgs[], uint8_t num_msgs,
			     uint16_t addr)
{
	return omap_i2c_xfer_common(dev, msgs, num_msgs, false, addr);
}

static int omap_i2c_transfer_polling(const struct device *dev, struct i2c_msg msgs[], uint8_t num_msgs,
			     uint16_t addr)
{
	return omap_i2c_xfer_common(dev, msgs, num_msgs, true, addr);
}

static int __maybe_unused omap_i2c_get_scl(struct device *dev)
{
	struct i2c_omap_data *data = dev->data;
	struct i2c_omap_cfg *cfg = dev->config;
	uint32_t reg;

	reg = omap_i2c_read_reg(dev, I2C_SYSTEST);

	return reg & OMAP_I2C_SYSTEST_SCL_I_FUNC;
}

static int __maybe_unused omap_i2c_get_sda(struct device *dev)
{
	struct i2c_omap_data *data = dev->data;
	struct i2c_omap_cfg *cfg = dev->config;
	uint32_t reg;

	reg = omap_i2c_read_reg(dev, I2C_SYSTEST);

	return reg & OMAP_I2C_SYSTEST_SDA_I_FUNC;
}

static const struct i2c_driver_api omap_i2c_api = {
	.transfer = omap_i2c_transfer,
};

#define I2C_OMAP_INIT(inst)                                                                        \
	static const struct i2c_omap_cfg i2c_omap_cfg_##inst = {                                   \
		.base = DT_INST_REG_ADDR(inst),                                                    \
		.irq = DT_INST_IRQN(inst),                                                         \
		.reg_shift = DT_INST_PROP(inst, reg_shift),                                        \
		.speed = DT_INST_PROP(inst, clock_frequency),                                      \
	};                                                                                         \
                                                                                                   \
	static struct i2c_omap_data i2c_omap_data_##inst;                                          \
                                                                                                   \
	I2C_DEVICE_DT_INST_DEFINE(inst, omap_i2c_init, NULL, &i2c_omap_data_##inst,                \
				  &i2c_omap_cfg_##inst, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,     \
				  &omap_i2c_api);
DT_INST_FOREACH_STATUS_OKAY(I2C_OMAP_INIT)