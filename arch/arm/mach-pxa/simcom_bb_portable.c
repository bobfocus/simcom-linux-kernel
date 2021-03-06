/*
 * linux/arch/arm/mach-pxa/simcom_bb_development.c
 *
 * Combitech SimCoM Module support, based on cm-x2xx.c
 *
 * Copyright (C) 2009 Combitech AB
 * David Kiland <david.kiland(at)combitech.se>
 * Tobias Knutsson <tobias.knutsson(at)combitech.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/pm.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/sysdev.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/spi.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio.h>
#include <linux/dm9000.h>
#include <linux/i2c/pca953x.h>
#include <linux/spi/ads7846.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <mach/pxa2xx-regs.h>
#include <mach/mfp-pxa27x.h>
#include <mach/pxafb.h>
#include <mach/ohci.h>
#include <mach/pxa2xx_spi.h>
#include <mach/mmc.h>
#include <mach/bitfield.h>

#include <plat/i2c.h>

#include "generic.h"
#include "devices.h"

/* SimCom device physical addresses */
#define SIMCOM_CS1_PHYS		(PXA_CS1_PHYS)

/* GPIO related definitions */
#define SIMCOM_ETHIRQ		IRQ_GPIO(20)
#define SIMCOM_TSIRQ		IRQ_GPIO(17)
#define SIMCOM_MMCDETECT	(12)

#define DM9000_PHYS_BASE	(PXA_CS2_PHYS)
#define NOR_PHYS_BASE		(PXA_CS0_PHYS)
#define NAND_PHYS_BASE		(PXA_CS1_PHYS)



static unsigned long simcom_pin_config[] = {

	/* BTUART */
	GPIO42_BTUART_RXD,
	GPIO43_BTUART_TXD,
	GPIO44_BTUART_CTS,
	GPIO45_BTUART_RTS,

	/* STUART */
	GPIO46_STUART_RXD,
	GPIO47_STUART_TXD,

	/* LCD */
	GPIO58_LCD_LDD_0,
	GPIO59_LCD_LDD_1,
	GPIO60_LCD_LDD_2,
	GPIO61_LCD_LDD_3,
	GPIO62_LCD_LDD_4,
	GPIO63_LCD_LDD_5,
	GPIO64_LCD_LDD_6,
	GPIO65_LCD_LDD_7,
	GPIO66_LCD_LDD_8,
	GPIO67_LCD_LDD_9,
	GPIO68_LCD_LDD_10,
	GPIO69_LCD_LDD_11,
	GPIO70_LCD_LDD_12,
	GPIO71_LCD_LDD_13,
	GPIO72_LCD_LDD_14,
	GPIO73_LCD_LDD_15,
	GPIO86_LCD_LDD_16,
	GPIO87_LCD_LDD_17,
	GPIO74_LCD_FCLK,
	GPIO75_LCD_LCLK,
	GPIO76_LCD_PCLK,
	GPIO77_LCD_BIAS,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,

	/* I2S */
	GPIO28_I2S_BITCLK_OUT,
	GPIO29_I2S_SDATA_IN,
	GPIO30_I2S_SDATA_OUT,
	GPIO31_I2S_SYNC,
	GPIO113_I2S_SYSCLK,

	/* SSP1 */
	GPIO23_SSP1_SCLK,
	GPIO25_SSP1_TXD,
	GPIO26_SSP1_RXD,

	/* MMC Card */
	GPIO32_MMC_CLK,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO112_MMC_CMD,

	/* SDRAM and local bus */
	GPIO15_nCS_1,
	GPIO78_nCS_2,
	GPIO79_nCS_3,
	GPIO80_nCS_4,
	GPIO33_nCS_5,
	GPIO48_nPOE,
	GPIO49_nPWE,
	GPIO18_RDY,

	/* DM9000 */
	GPIO21_nSDCS_3,

	GPIO89_USBH1_PEN,
	GPIO120_USBH2_PEN,
};




static struct mtd_partition simcom_nand_partitions[] = {
	{
		.name		= "rootfs",
		.offset		= 0,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data simcom_nand_data = {
	.width		= 4,
	.parts		= simcom_nand_partitions,
	.nr_parts	= ARRAY_SIZE(simcom_nand_partitions),
};

static struct resource simcom_nand_resource = {
	.start		= NAND_PHYS_BASE,
	.end		= NAND_PHYS_BASE + SZ_128M - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device simcom_nand_device = {
	.name		= "simcom-nand",
	.id		= 0,
	.dev		= {
		.platform_data	= &simcom_nand_data,
	},
	.num_resources	= 1,
	.resource	= &simcom_nand_resource,
};
static void __init simcom_init_nand(void)
{
	platform_device_register(&simcom_nand_device);
}



static struct resource simcom_dm9000_resource[] = {
	[0] = {
		.start = DM9000_PHYS_BASE,
		.end   = DM9000_PHYS_BASE + 3,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DM9000_PHYS_BASE + 4,
		.end   = DM9000_PHYS_BASE + 7,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = SIMCOM_ETHIRQ,
		.end   = SIMCOM_ETHIRQ,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	}
};

static struct platform_device simcom_dm9000_device = {
	.name		= "dm9000",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(simcom_dm9000_resource),
	.resource	= simcom_dm9000_resource,
};

static void __init simcom_init_dm9000(void)
{
	platform_device_register(&simcom_dm9000_device);
}



/* USB OHCI controller */
#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
static struct pxaohci_platform_data simcom_ohci_platform_data = {
	.port_mode	= PMM_PERPORT_MODE,
	.flags		= ENABLE_PORT1 | ENABLE_PORT2,
};



static struct ads7846_platform_data ads7846_info = {
	.model				= 7846,
	.vref_delay_usecs	= 100,
	.pressure_max     = 512,
	.debounce_max     = 10,
	.debounce_tol     = 3,
	.debounce_rep     = 1,
	.gpio_pendown		= 17,
};


static struct pxa2xx_spi_chip ads7846_chip = {
	.gpio_cs = 83,
};

static struct pxa2xx_spi_chip mcp3002_chip = {
	.gpio_cs = 52,
};

static struct spi_board_info simcom_spi_devices[] __initdata = {
	{
		.modalias			= "ads7846",
		.max_speed_hz		= 1200000,
		.bus_num			= 1,
		.mode				= SPI_MODE_0,
		.chip_select		= 0,
		.irq				= SIMCOM_TSIRQ,
		.controller_data 	= &ads7846_chip,
		.platform_data		= &ads7846_info,
	},
	{
		.modalias			= "mcp3002",
		.max_speed_hz		= 1200000,
		.bus_num			= 1,
		.mode				= SPI_MODE_0,
		.chip_select		= 1,
		.controller_data 	= &mcp3002_chip,
	},
};


static struct platform_pwm_backlight_data simcom_backlight_data = {
	.pwm_id		= 11,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
};

static struct platform_device simcom_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.parent = &pxa27x_device_pwm1.dev,
		.platform_data	= &simcom_backlight_data,
	},
};


static struct pxamci_platform_data simcom_mci_platform_data = {
	.ocr_mask			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.gpio_card_detect	= SIMCOM_MMCDETECT,
	.gpio_card_ro = -1,
};


static struct pxafb_mode_info generic_tft_320x240_mode = {
	.pixclock	= 35700,
	.bpp		= 16,
	.xres		= 320,
	.yres		= 240,
	.hsync_len	= 1,
	.vsync_len	= 1,
	.left_margin	= 70,
	.upper_margin	= 13,
	.right_margin	= 1,
	.lower_margin	= 1,
	.sync		= 0,
	.cmap_greyscale = 0,
};

static struct pxafb_mach_info generic_tft_320x240 = {
	.modes		= &generic_tft_320x240_mode,
	.num_modes	= 1,
	.lccr0		= (LCCR0_PAS),
	.lccr3		= (LCCR3_PixClkDiv(0x01) |
			   LCCR3_Acb(0xff) |
			   LCCR3_PCP),

	.cmap_inverse	= 0,
	.cmap_static	= 0,
	.lcd_conn	= LCD_COLOR_TFT_18BPP,
};


static struct pxafb_mach_info *simcom_display = &generic_tft_640x480;//&generic_crt_800x600;
static void __init simcom_init_display(void)
{
	set_pxa_fb_info(simcom_display);
}
#else
static inline void simcom_init_display(void) {}
#endif // PXAFB

/* Development baseboard */
#if defined(CONFIG_SIMCOM_BB_DEV)
static struct pca953x_platform_data gpio_exp = {
		.gpio_base	= 128,
		.invert = 0,
};
static struct i2c_board_info simcom_bb_dev_i2c_info[] = {
	{	/* I2C Switch */
		I2C_BOARD_INFO("pca9546a", 0x70),
	},
	{	/* GPIO Expander */
		I2C_BOARD_INFO("pca9539", 0x74),
		.platform_data = &gpio_exp,
	},
	{	/* Audio CODEC */
		I2C_BOARD_INFO("tlv320aic3x", 0x18),
	},
};


static struct pxafb_mach_info *simcom_display = &generic_tft_320x240;//&generic_crt_800x600;


static void __init simcom_init(void)
{
	simcom_init_nand();
	simcom_init_dm9000();

	/* Initialize Display */	
	//platform_device_register(&simcom_backlight_device);
	set_pxa_fb_info(simcom_display);
	/* Initialize card interface */
	pxa_set_mci_info(&simcom_mci_platform_data);

	/* Initialize SPI interfaces */
	pxa2xx_set_spi_info(1, &simcom_spi_port1_info);
	spi_register_board_info(ARRAY_AND_SIZE(simcom_spi_devices));

	pxa2xx_mfp_config(ARRAY_AND_SIZE(simcom_pin_config));
}

static void __init simcom_init_irq(void)
{
	pxa27x_init_irq();
}

static void __init simcom_map_io(void)
{
	pxa_map_io();
}


MACHINE_START(SIMCOM, "Combitech SimCoM Portable")
	.boot_params	= 0xa0000100,
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= simcom_map_io,
	.init_irq	= simcom_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= simcom_init,
MACHINE_END
