/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __configs_chromeos_daisy_h__
#define __configs_chromeos_daisy_h__

#include <configs/smdk5250.h>

#undef CONFIG_DEFAULT_DEVICE_TREE
#define CONFIG_DEFAULT_DEVICE_TREE      exynos5250-snow

/* Generally verified boot needs more heap space */
#undef CONFIG_SYS_MALLOC_LEN
#define CONFIG_SYS_MALLOC_LEN	(32 << 20)

#define CONFIG_INITRD_ADDRESS 0x42000000

#include <configs/chromeos.h>

#define CONFIG_CHROMEOS_USB

/* Support vboot flag reading from GPIO hardwrae */
#define CONFIG_CHROMEOS_GPIO_FLAG

/* Support vboot flag reading from EC */
#define CONFIG_CHROMEOS_MKBP_FLAG

/* Use the default arch_phys_memset implementation */
#define CONFIG_PHYSMEM

/* Adjust the display resolution. */
#undef MAIN_VRESOL_VAL
#undef MAIN_HRESOL_VAL
#define MAIN_VRESOL_VAL 0x300
#define MAIN_HRESOL_VAL 0x556
#undef LCD_XRES
#undef LCD_YRES
#define LCD_XRES 1366
#define LCD_YRES 768
#define CONFIG_SYS_WHITE_ON_BLACK

/*
 * Extra bootargs used for direct booting, but not for vboot.
 * - console of the board
 * - debug and earlyprintk: easier to debug; they could be removed later
 */
#define CONFIG_DIRECT_BOOTARGS \
	"console=tty debug earlyprintk"

/* Standard input, output and error device of U-Boot console. */
#define CONFIG_STD_DEVICES_SETTINGS 	EXYNOS_DEVICE_SETTINGS

#define CONFIG_CHROMEOS_SD_TO_SPI \
	"sd_to_spi=echo Flashing U-Boot from SD card to SPI flash; " \
	"if mmc dev 1 && " \
	"mmc rescan && " \
	"mmc read 40008000 1 1000 && " \
	"sf probe 1:0 && " \
	"sf update 40008000 0 80000; then " \
	"echo Flash completed; else " \
	"echo Flash failed; " \
	"fi\0"

/* Replace default CONFIG_EXTRA_ENV_SETTINGS */
#ifdef CONFIG_EXTRA_ENV_SETTINGS
#undef CONFIG_EXTRA_ENV_SETTINGS
#endif
#define CONFIG_EXTRA_ENV_SETTINGS \
	EXYNOS_DEVICE_SETTINGS \
	"stdin=mkbp-keyb\0" \
	"stdout=lcd\0" \
	"stderr=lcd\0" \
	"dev_extras=daisy\0" \
	"loadaddr=0x40007000\0" \
	"rdaddr=0x4f000000\0" \
	"kerneladdr=0x40007000\0" \
	"ramdiskaddr=0x4f000000\0" \
	"dtbaddr=0x4e800000\0" \
	"console=tty\0" \
	"mmcdev=1\0" \
	"bootenv=uEnv.txt\0" \
	"loadbootenv=ext2load mmc ${mmcdev}:2 ${loadaddr} ${bootenv}\0" \
	"importbootenv=echo Importing environment ...; " \
		"env import -t $loadaddr $filesize\0" \
        "loadbootscript=ext2load mmc ${mmcdev}:2 ${loadaddr} boot.scr\0" \
	"loadbootenv_usb=ext2load usb ${usbdev}:2 ${loadaddr} ${bootenv}\0" \
        "loadbootscript_usb=ext2load usb ${usbdev}:2 ${loadaddr} boot.scr\0" \
        "bootscript=echo Running bootscript ...; " \
                "source ${loadaddr}\0"

/* Replace default CONFIG_BOOTCOMMAND */
#ifdef CONFIG_BOOTCOMMAND
#undef CONFIG_BOOTCOMMAND
#endif
#define CONFIG_BOOTCOMMAND \
	"mmc dev ${mmcdev};              " \
	"if mmc rescan ${mmcdev}; then " \
		"echo SD/MMC found on device ${mmcdev};" \
		"if run loadbootenv; then " \
			"echo Loaded environment from ${bootenv};" \
			"run importbootenv;" \
		"fi;" \
		"if test -n $uenvcmd; then " \
			"echo Running uenvcmd ...;" \
			"run uenvcmd;" \
		"fi;" \
		"if run loadbootscript; then " \
			"run bootscript; " \
		"fi; " \
	"fi;" \
	"usb start;" \
		"echo Tring USB on device ${usbdev};" \
		"if run loadbootenv_usb; then " \
			"echo Loaded environment from ${bootenv};" \
			"run importbootenv;" \
		"fi;" \
		"if test -n $uenvcmd; then " \
			"echo Running uenvcmd ...;" \
			"run uenvcmd;" \
		"fi;" \
		"if run loadbootscript_usb; then " \
			"run bootscript; " \
		"fi; "

#define CONFIG_PRE_CONSOLE_BUFFER
#define CONFIG_PRE_CON_BUF_SZ 0x100000
#define CONFIG_PRE_CON_BUF_ADDR 0x41f00000

#define DEBUG
#undef DEBUG
#undef CONFIG_SPL
#undef CONFIG_SYS_THUMB_BUILD
#undef CONFIG_SYS_ARM_CACHE_WRITETHROUGH
#undef CONFIG_SILENT_CONSOLE

/* disable vboot */
#undef CONFIG_CHROMEOS
#undef CONFIG_OF_UPDATE_FDT_BEFORE_BOOT

#endif /* __configs_chromeos_daisy_h__ */
