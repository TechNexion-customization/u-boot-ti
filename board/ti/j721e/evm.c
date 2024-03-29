// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for J721E EVM
 *
 * Copyright (C) 2018-2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla <lokeshvutla@ti.com>
 *
 */

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <generic-phy.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <net.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hardware.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <spl.h>
#include <asm/arch/sys_proto.h>
#include <dm.h>
#include <dm/uclass-internal.h>

#include "../common/board_detect.h"

#define board_is_j721e_som()	(board_ti_k3_is("J721EX-PM1-SOM") || \
				 board_ti_k3_is("J721EX-PM2-SOM"))

#define board_is_j721e_sk()	 (board_ti_k3_is("J721EX-EAIK") || \
				  board_ti_k3_is("J721EX-SK"))

#define board_is_j721e_pm1_som() board_ti_k3_is("J721EX-PM1-SOM")
#define board_is_j721e_pm2_som() board_ti_k3_is("J721EX-PM2-SOM")

#define board_is_j7200_som()	(board_ti_k3_is("J7200X-PM1-SOM") || \
				 board_ti_k3_is("J7200X-PM2-SOM"))

/* Max number of MAC addresses that are parsed/processed per daughter card */
#define DAUGHTER_CARD_NO_OF_MAC_ADDR	8

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
#ifdef CONFIG_PHYS_64BIT
	gd->ram_size = 0x100000000;
#else
	gd->ram_size = 0x80000000;
#endif

	return 0;
}

ulong board_get_usable_ram_top(ulong total_size)
{
#ifdef CONFIG_PHYS_64BIT
	/* Limit RAM used by U-Boot to the DDR low region */
	if (gd->ram_top > 0x100000000)
		return 0x100000000;
#endif

	return gd->ram_top;
}

int dram_init_banksize(void)
{
	/* Bank 0 declares the memory available in the DDR low region */
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = 0x80000000;
	gd->ram_size = 0x80000000;

#ifdef CONFIG_PHYS_64BIT
	/* Bank 1 declares the memory available in the DDR high region */
	gd->bd->bi_dram[1].start = CONFIG_SYS_SDRAM_BASE1;
	gd->bd->bi_dram[1].size = 0x80000000;
	gd->ram_size = 0x100000000;
#endif

	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	bool eeprom_read = board_ti_was_eeprom_read();

	if (board_is_j721e_pm1_som()) {
		/* Loading for pm1 board a72 spl/u-boot */
		if (!strcmp(name, "k3-j721e-tps65917-proc-board"))
			return 0;
	} else if (!eeprom_read || board_is_j721e_pm2_som()) {
		/* Loading for pm2 board a72 spl/u-boot */
		if (!strcmp(name, "k3-j721e-common-proc-board") ||
		    !strcmp(name, "k3-j721e-r5-common-proc-board"))
			return 0;
	} else if (board_is_j721e_sk()) {
		if (!strcmp(name, "k3-j721e-sk") ||
		    !strcmp(name, "k3-j721e-r5-sk"))
			return 0;
	}

	return -1;
}
#endif

#if CONFIG_IS_ENABLED(DM_GPIO) && CONFIG_IS_ENABLED(OF_LIBFDT)
/* Returns 1, if onboard mux is set to hyperflash */
static void __maybe_unused detect_enable_hyperflash(void *blob)
{
	struct gpio_desc desc = {0};
	char *hypermux_sel_gpio = (board_is_j721e_som()) ? "8" : "6";

	if (dm_gpio_lookup_name(hypermux_sel_gpio, &desc))
		return;

	if (dm_gpio_request(&desc, hypermux_sel_gpio))
		return;

	if (dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN))
		return;

	if (dm_gpio_get_value(&desc)) {
		int offset;

		do_fixup_by_compat(blob, "ti,am654-hbmc", "status",
				   "okay", sizeof("okay"), 0);
		offset = fdt_node_offset_by_compatible(blob, -1,
						       "ti,am654-ospi");
		fdt_setprop(blob, offset, "status", "disabled",
			    sizeof("disabled"));
	}
}
#endif

#if defined(CONFIG_SPL_BUILD) && (defined(CONFIG_TARGET_J7200_A72_EVM) || defined(CONFIG_TARGET_J7200_R5_EVM) || \
					defined(CONFIG_TARGET_J721E_A72_EVM) || defined(CONFIG_TARGET_J721E_R5_EVM))
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	detect_enable_hyperflash(spl_image->fdt_addr);
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;

	ret = fdt_fixup_msmc_ram(blob, "/bus@100000", "sram@70000000");
	if (ret < 0)
		ret = fdt_fixup_msmc_ram(blob, "/interconnect@100000",
					 "sram@70000000");
	if (ret)
		printf("%s: fixing up msmc ram failed %d\n", __func__, ret);

	detect_enable_hyperflash(blob);

	return ret;
}
#endif

#ifdef CONFIG_TI_I2C_BOARD_DETECT
int do_board_detect(void)
{
	int ret;

	if (board_ti_was_eeprom_read())
		return 0;

	ret = ti_i2c_eeprom_am6_get_base(CONFIG_EEPROM_BUS_ADDRESS,
					 CONFIG_EEPROM_CHIP_ADDRESS);
	if (ret) {
		printf("EEPROM not available at 0x%02x, trying to read at 0x%02x\n",
		       CONFIG_EEPROM_CHIP_ADDRESS, CONFIG_EEPROM_CHIP_ADDRESS + 1);
		ret = ti_i2c_eeprom_am6_get_base(CONFIG_EEPROM_BUS_ADDRESS,
						 CONFIG_EEPROM_CHIP_ADDRESS + 1);
		if (ret)
			pr_err("Reading on-board EEPROM at 0x%02x failed %d\n",
			       CONFIG_EEPROM_CHIP_ADDRESS + 1, ret);
	}

	return ret;
}

int checkboard(void)
{
	struct ti_am6_eeprom *ep = TI_AM6_EEPROM_DATA;

	if (do_board_detect())
		/* EEPROM not populated */
		printf("Board: %s rev %s\n", "J721EX-PM1-SOM", "E2");
	else
		printf("Board: %s rev %s\n", ep->name, ep->version);

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
static void setup_board_eeprom_env(void)
{
	char *name = "J721EX-PM2-SOM";

	if (do_board_detect())
		goto invalid_eeprom;

	if (board_ti_k3_is("J721EX-PM1-SOM"))
		name = "J721EX-PM1-SOM";
	else if (board_ti_k3_is("J721EX-PM2-SOM"))
		name = "J721EX-PM2-SOM";
	else if (board_is_j721e_sk())
		name = "j721e-sk";
	else if (board_is_j7200_som())
		name = "j7200";
	else
		printf("Unidentified board claims %s in eeprom header\n",
		       board_ti_get_name());

invalid_eeprom:
	set_board_info_env_am6(name);
}

static void setup_serial(void)
{
	struct ti_am6_eeprom *ep = TI_AM6_EEPROM_DATA;
	unsigned long board_serial;
	char *endp;
	char serial_string[17] = { 0 };

	if (env_get("serial#"))
		return;

	board_serial = simple_strtoul(ep->serial, &endp, 16);
	if (*endp != '\0') {
		pr_err("Error: Can't set serial# to %s\n", ep->serial);
		return;
	}

	snprintf(serial_string, sizeof(serial_string), "%016lx", board_serial);
	env_set("serial#", serial_string);
}
#endif

/*
 * Declaration of daughtercards to probe. Note that when adding more
 * cards they should be grouped by the 'i2c_addr' field to allow for a
 * more efficient probing process.
 */
static const struct {
	u8 i2c_addr;		/* I2C address of card EEPROM */
	char *card_name;	/* EEPROM-programmed card name */
	char *dtbo_name;	/* Device tree overlay to apply */
	u8 eth_offset;		/* ethXaddr MAC address index offset */
} ext_cards[] = {
	{
		0x51,
		"J7X-BASE-CPB",
		"",		/* No dtbo for this board */
		0,
	},
	{
		0x52,
		"J7X-INFOTAN-EXP",
		"k3-j721e-common-proc-board-infotainment.dtbo",
		0,
	},
	{
		0x52,
		"J7X-GESI-EXP",
		"",		/* No dtbo for this board */
		5,		/* Start populating from eth5addr */
	},
	{
		0x54,
		"J7X-VSC8514-ETH",
		"",		/* No dtbo for this board */
		1,		/* Start populating from eth1addr */
	},
};

static bool daughter_card_detect_flags[ARRAY_SIZE(ext_cards)];

const char *board_fit_get_additionnal_images(int index, const char *type)
{
	int i, j;

	if (strcmp(type, FIT_FDT_PROP))
		return NULL;

	j = 0;
	for (i = 0; i < ARRAY_SIZE(ext_cards); i++) {
		if (daughter_card_detect_flags[i]) {
			if (j == index) {
				/*
				 * Return dtbo name only if populated,
				 * otherwise stop parsing here.
				 */
				if (strlen(ext_cards[i].dtbo_name))
					return ext_cards[i].dtbo_name;
				else
					return NULL;
			};

			j++;
		}
	}

	return NULL;
}

static int probe_daughtercards(void)
{
	char mac_addr[DAUGHTER_CARD_NO_OF_MAC_ADDR][TI_EEPROM_HDR_ETH_ALEN];
	bool eeprom_read_success;
	struct ti_am6_eeprom ep;
	u8 previous_i2c_addr;
	u8 mac_addr_cnt;
	int i;
	int ret;

	/* Mark previous I2C address variable as not populated */
	previous_i2c_addr = 0xff;

	/* No EEPROM data was read yet */
	eeprom_read_success = false;

	/* Iterate through list of daughtercards */
	for (i = 0; i < ARRAY_SIZE(ext_cards); i++) {
		/* Obtain card-specific I2C address */
		u8 i2c_addr = ext_cards[i].i2c_addr;

		/* Read card EEPROM if not already read previously */
		if (i2c_addr != previous_i2c_addr) {
			/* Store I2C address so we can avoid reading twice */
			previous_i2c_addr = i2c_addr;

			/* Get and parse the daughter card EEPROM record */
			ret = ti_i2c_eeprom_am6_get(CONFIG_EEPROM_BUS_ADDRESS,
						    i2c_addr,
						    &ep,
						    (char **)mac_addr,
						    DAUGHTER_CARD_NO_OF_MAC_ADDR,
						    &mac_addr_cnt);
			if (ret) {
				debug("%s: No daughtercard EEPROM at 0x%02x found %d\n",
				      __func__, i2c_addr, ret);
				eeprom_read_success = false;
				/* Skip to the next daughtercard to probe */
				continue;
			}

			/* EEPROM read successful, okay to further process. */
			eeprom_read_success = true;
		}

		/* Only continue processing if EEPROM data was read */
		if (!eeprom_read_success)
			continue;

		/* Only process the parsed data if we found a match */
		if (strncmp(ep.name, ext_cards[i].card_name, sizeof(ep.name)))
			continue;

		printf("Detected: %s rev %s\n", ep.name, ep.version);
		daughter_card_detect_flags[i] = true;

#ifndef CONFIG_SPL_BUILD
		int j;
		/*
		 * Populate any MAC addresses from daughtercard into the U-Boot
		 * environment, starting with a card-specific offset so we can
		 * have multiple ext_cards contribute to the MAC pool in a well-
		 * defined manner.
		 */
		for (j = 0; j < mac_addr_cnt; j++) {
			if (!is_valid_ethaddr((u8 *)mac_addr[j]))
				continue;

			eth_env_set_enetaddr_by_index("eth",
						      ext_cards[i].eth_offset + j,
						      (uchar *)mac_addr[j]);
		}
#endif
	}
#ifndef CONFIG_SPL_BUILD
	char name_overlays[1024] = { 0 };

	for (i = 0; i < ARRAY_SIZE(ext_cards); i++) {
		if (!daughter_card_detect_flags[i])
			continue;

		/* Skip if no overlays are to be added */
		if (!strlen(ext_cards[i].dtbo_name))
			continue;

		/*
		 * Make sure we are not running out of buffer space by checking
		 * if we can fit the new overlay, a trailing space to be used
		 * as a separator, plus the terminating zero.
		 */
		if (strlen(name_overlays) + strlen(ext_cards[i].dtbo_name) + 2 >
		    sizeof(name_overlays))
			return -ENOMEM;

		/* Append to our list of overlays */
		strcat(name_overlays, ext_cards[i].dtbo_name);
		strcat(name_overlays, " ");
	}

	/* Apply device tree overlay(s) to the U-Boot environment, if any */
	if (strlen(name_overlays))
		return env_set("name_overlays", name_overlays);
#endif

	return 0;
}
#endif

void configure_serdes_torrent(void)
{
	struct udevice *dev;
	struct phy serdes;
	int ret;

	if (!IS_ENABLED(CONFIG_PHY_CADENCE_TORRENT))
		return;

	ret = uclass_get_device_by_driver(UCLASS_PHY,
					  DM_GET_DRIVER(torrent_phy_provider),
					  &dev);
	if (ret) {
		printf("Torrent init failed:%d\n", ret);
		return;
	}

	serdes.dev = dev;
	serdes.id = 0;

	ret = generic_phy_init(&serdes);
	if (ret) {
		printf("phy_init failed!!: %d\n", ret);
		return;
	}

	ret = generic_phy_power_on(&serdes);
	if (ret) {
		printf("phy_power_on failed!!: %d\n", ret);
		return;
	}
}

void configure_serdes_sierra(void)
{
	struct udevice *dev, *link_dev;
	struct phy link;
	int ret, count, i;
	int link_count = 0;

	if (!IS_ENABLED(CONFIG_PHY_CADENCE_SIERRA))
		return;

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_GET_DRIVER(sierra_phy_provider),
					  &dev);
	if (ret) {
		printf("Sierra init failed:%d\n", ret);
		return;
	}

	count = device_get_child_count(dev);
	for (i = 0; i < count; i++) {
		ret = device_get_child(dev, i, &link_dev);
		if (ret) {
			printf("probe of sierra child node %d failed: %d\n", i, ret);
			return;
		}
		if (link_dev->driver->id == UCLASS_PHY) {
			link.dev = link_dev;
			link.id = link_count++;

			ret = generic_phy_power_on(&link);
			if (ret) {
				printf("phy_power_on failed!!: %d\n", ret);
				return;
			}
		}
	}
}

#define __REG(x) (*((volatile u32 *)(x)))
static void set_gpio_iomux_sgmii_reset()
{
	/* Configure pinmux of SGMII reset pin as GPIO */

	/* 4xSGMII PHY on SERDES4 */
	/* (AF27) PRG0_PRU1_GPO12.GPIO0_75 */
	__REG(0x11C130) = 0x10007;
}

static void set_gpio_value_sgmii_reset(int value)
{
	/* 4xSGMII PHY on SERDES4 */
	/* (AF27) PRG0_PRU1_GPO12.GPIO0_75 */
	/* configure GPIO as output*/
	__REG(0x600060) &=  ~(0x1 << (75%16));
	/* set value */
	if (value)
		__REG(0x600064) |= 0x1 << (75%16);
	else
		__REG(0x600064) &= ~(0x1 << (75%16));
}

static void reset_sgmii_phy_chip(void)
{
	printf("----> reset_sgmii_phy_chip \r\n");
	/* For a complete PHY reset of Realtek RTL8211FS, this pin must be
	   asserted low for at least 10ms for the internal regulator.
	   Wait for at least 72ms* (for internal circuits settling time)
	   before accessing the PHY register. */

	/* 4xSGMII PHY on SERDES4 */
	/* (AF27) PRG0_PRU1_GPO12.GPIO0_75 */
	set_gpio_iomux_sgmii_reset();
	set_gpio_value_sgmii_reset(0);
	udelay(40000);
	set_gpio_value_sgmii_reset(1);

	/* wait 100ms for of PHY reset completion */
	udelay(100000);
#if 0
	struct gpio_desc desc = {0};
	int ret;

	ret = dm_gpio_lookup_name("75", &desc);
	if (ret) {
		printf("error getting GPIO lookup name: %d\n", ret);
		return ret;
	}
	ret = dm_gpio_request(&desc, "75");
	if (ret) {
		printf("error requesting GPIO: %d\n", ret);
		goto err_free_gpio;
	}
	ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	if (ret) {
		printf("error setting direction flag of GPIO: %d\n", ret);
		goto err_free_gpio;
	}
	ret = dm_gpio_set_value(&desc, 1);
	if (ret < 0)
		printf("error getting value of GPIO: %d\n", ret);

err_free_gpio:
	dm_gpio_free(desc.dev, &desc);
	return ret;
#endif
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	if (IS_ENABLED(CONFIG_TI_I2C_BOARD_DETECT)) {
		setup_board_eeprom_env();
		setup_serial();

		/* Check for and probe any plugged-in daughtercards */
		if (!board_is_j721e_sk())
			probe_daughtercards();
	}

	reset_sgmii_phy_chip();

#if defined(CONFIG_TARGET_J7200_A72_EVM) || defined(CONFIG_TARGET_J7200_R5_EVM)
	configure_serdes_torrent();
#endif

#if defined(CONFIG_TARGET_J721E_A72_EVM) || defined(CONFIG_TARGET_J721E_R5_EVM)
	configure_serdes_sierra();
#endif

	return 0;
}
#endif

static int __maybe_unused detect_SW3_1_state(void)
{
	if (IS_ENABLED(CONFIG_TARGET_J7200_A72_EVM)) {
		struct gpio_desc desc = {0};
		int ret;

		ret = dm_gpio_lookup_name("6", &desc);
		if (ret) {
			printf("error getting GPIO lookup name: %d\n", ret);
			return ret;
		}

		ret = dm_gpio_request(&desc, "6");
		if (ret) {
			printf("error requesting GPIO: %d\n", ret);
			goto err_free_gpio;
		}

		ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN);
		if (ret) {
			printf("error setting direction flag of GPIO: %d\n", ret);
			goto err_free_gpio;
		}

		ret = dm_gpio_get_value(&desc);
		if (ret < 0)
			printf("error getting value of GPIO: %d\n", ret);

err_free_gpio:
		dm_gpio_free(desc.dev, &desc);
		return ret;
	}
}

void spl_board_init(void)
{
#if defined(CONFIG_ESM_K3) || defined(CONFIG_ESM_PMIC)
	struct udevice *dev;
	int ret;
#endif

	if ((IS_ENABLED(CONFIG_TARGET_J721E_A72_EVM) ||
	     IS_ENABLED(CONFIG_TARGET_J7200_A72_EVM)) &&
	    IS_ENABLED(CONFIG_TI_I2C_BOARD_DETECT)) {
		if (!board_is_j721e_sk())
			probe_daughtercards();
	}


#ifdef CONFIG_ESM_K3
	if (board_ti_k3_is("J721EX-PM2-SOM")) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_GET_DRIVER(k3_esm), &dev);
		if (ret)
			printf("ESM init failed: %d\n", ret);
	}
#endif

#ifdef CONFIG_ESM_PMIC
	if (board_ti_k3_is("J721EX-PM2-SOM")) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_GET_DRIVER(pmic_esm),
						  &dev);
		if (ret)
			printf("ESM PMIC init failed: %d\n", ret);
	}
#endif

	if (IS_ENABLED(CONFIG_TARGET_J7200_A72_EVM) &&
	    IS_ENABLED(CONFIG_HBMC_AM654)) {
		struct udevice *dev;
		int ret;

		ret = detect_SW3_1_state();
		if (ret == 1) {
			ret = uclass_get_device_by_driver(UCLASS_MTD,
							  DM_GET_DRIVER(hbmc_am654),
							  &dev);
			if (ret)
				debug("Failed to probe hyperflash\n");
		}
	}
}
