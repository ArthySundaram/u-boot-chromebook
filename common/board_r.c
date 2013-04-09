/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2002-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm-generic/sections.h>

/* TODO: can we just include all these headers whether needed or not? */
#ifdef CONFIG_ADDR_MAP
#include <asm/mmu.h>
#endif
#if defined(CONFIG_CMD_BEDBUG)
#include <bedbug/type.h>
#endif
#ifdef CONFIG_HAS_DATAFLASH
#include <dataflash.h>
#endif
#include <environment.h>
#if defined(CONFIG_CMD_IDE)
#include <ide.h>
#endif
#include <initcall.h>
#ifdef CONFIG_PS2KBD
#include <keyboard.h>
#endif
#if defined(CONFIG_CMD_KGDB)
#include <kgdb.h>
#endif
#include <logbuff.h>
#include <malloc.h>
#ifdef CONFIG_BITBANGMII
#include <miiphy.h>
#endif
#include <mmc.h>
#include <nand.h>
#include <onenand_uboot.h>
#include <scsi.h>
#include <serial.h>
#include <spi.h>
#include <stdio_dev.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

ulong monitor_flash_len;

/* TODO: Move this prototype to a header? */
#ifdef CONFIG_SYS_UPDATE_FLASH_SIZE
extern int update_flash_size(int flash_size);
#endif

int __board_flash_wp_on(void)
{
	/*
	 * Most flashes can't be detected when write protection is enabled,
	 * so provide a way to let U-Boot gracefully ignore write protected
	 * devices.
	 */
	return 0;
}

int board_flash_wp_on(void)
	__attribute__ ((weak, alias("__board_flash_wp_on")));

void __cpu_secondary_init_r(void)
{
}

void cpu_secondary_init_r(void)
	__attribute__ ((weak, alias("__cpu_secondary_init_r")));

static int initr_secondary_cpu(void)
{
	/*
	 * after non-volatile devices & environment is setup and cpu code have
	 * another round to deal with any initialization that might require
	 * full access to the environment or loading of some image (firmware)
	 * from a non-volatile device
	 */
	/* TODO: maybe define this for all archs? */
	cpu_secondary_init_r();

	return 0;
}

static int initr_reloc(void)
{
	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */
	return 0;
}

/*
 * Some of these functions are needed purely because the functions they
 * call return void. If we change them to return 0, these stubs can go away.
 */
#ifdef CONFIG_ARM
static int initr_caches(void)
{
	/* TODO: sort out x86 code here */
	/* Enable caches */
	enable_caches();
	return 0;
}
#endif

static int initr_reloc_global_data(void)
{
#ifdef CONFIG_SYS_SYM_OFFSETS
	monitor_flash_len = _end_ofs;
#else
	monitor_flash_len = (ulong)&__init_end - gd->dest_addr;
#endif
#if defined(CONFIG_MPC85xx) || defined(CONFIG_MPC86xx)
	/*
	 * The gd->cpu pointer is set to an address in flash before relocation.
	 * We need to update it to point to the same CPU entry in RAM.
	 * TODO: why not just add gd->reloc_ofs?
	 */
	gd->cpu += gd->dest_addr - CONFIG_SYS_MONITOR_BASE;
#endif
#ifdef CONFIG_SYS_EXTRA_ENV_RELOC
	/*
	 * Some systems need to relocate the env_addr pointer early because the
	 * location it points to will get invalidated before env_relocate is
	 * called.  One example is on systems that might use a L2 or L3 cache
	 * in SRAM mode and initialize that cache from SRAM mode back to being
	 * a cache in cpu_init_r.
	 */
	gd->env_addr += gd->dest_addr - CONFIG_SYS_MONITOR_BASE;
#endif
	return 0;
}

#ifdef CONFIG_SERIAL_MULTI
static int initr_serial(void)
{
	serial_initialize();
	return 0;
}
#endif

#ifdef CONFIG_PPC
static int initr_trap(void)
{
	/*
	 * Setup trap handlers
	 */
	trap_init(gd->dest_addr);

	return 0;
}
#endif

#ifdef CONFIG_ADDR_MAP
static int initr_addr_map(void)
{
	init_addr_map();

	return 0;
}
#endif

#ifdef CONFIG_LOGBUFFER
unsigned long logbuffer_base(void)
{
	return gd->ram_top - LOGBUFF_LEN;
}

static int initr_logbuffer(void)
{
	logbuff_init_ptrs();
	return 0;
}
#endif

#ifdef CONFIG_POST
static int initr_post_backlog(void)
{
	post_output_backlog();
	return 0;
}
#endif

#ifdef CONFIG_SYS_DELAYED_ICACHE
static int initr_icache_enable(void)
{
	return 0;
}
#endif

#if defined(CONFIG_SYS_INIT_RAM_LOCK) && defined(CONFIG_E500)
static int initr_unlock_ram_in_cache(void)
{
	unlock_ram_in_cache();	/* it's time to unlock D-cache in e500 */
	return 0;
}
#endif

#ifdef CONFIG_PCI
static int initr_pci(void)
{
	pci_init();

	return 0;
}
#endif

#ifdef CONFIG_WINBOND_83C553
static int initr_w83c553f(void)
{
	/*
	 * Initialise the ISA bridge
	 */
	initialise_w83c553f();
	return 0;
}
#endif

static int initr_barrier(void)
{
#ifdef CONFIG_PPC
	/* TODO: Can we not use dmb() macros for this? */
	asm("sync ; isync");
#endif
	return 0;
}

static int initr_malloc(void)
{
	ulong malloc_start;

	/* The malloc area is immediately below the monitor copy in DRAM */
	malloc_start = gd->dest_addr - TOTAL_MALLOC_LEN;
	mem_malloc_init(malloc_start, TOTAL_MALLOC_LEN);
	return 0;
}

static int initr_announce(void)
{
	debug("Now running in RAM - U-Boot at: %08lx\n", gd->dest_addr);
	return 0;
}

#if !defined(CONFIG_SYS_NO_FLASH)
static int initr_flash(void)
{
	ulong flash_size = 0;
	bd_t *bd = gd->bd;
	int ok;

	puts("Flash: ");

	if (board_flash_wp_on()) {
		printf("Uninitialized - Write Protect On\n");
		/* Since WP is on, we can't find real size.  Set to 0 */
		ok = 1;
	} else {
		flash_size = flash_init();
		ok = flash_size > 0;
	}
	if (!ok) {
		puts("*** failed ***\n");
#ifdef CONFIG_PPC
		/* Why does PPC do this? */
		hang();
#endif
		return -1;
	}
	print_size(flash_size, "");
#ifdef CONFIG_SYS_FLASH_CHECKSUM
	{
		char *s;

		s = getenv("flashchecksum");

		/*
		* Compute and print flash CRC if flashchecksum is set to 'y'
		*
		* NOTE: Maybe we should add some WATCHDOG_RESET()? XXX
		*/
		if (s && (*s == 'y')) {
			printf("  CRC: %08X", crc32(0,
				(const unsigned char *) CONFIG_SYS_FLASH_BASE,
				flash_size));
		}
	}
#endif /* CONFIG_SYS_FLASH_CHECKSUM */
	putc('\n');

	/* update start of FLASH memory    */
#ifdef CONFIG_SYS_FLASH_BASE
	bd->bi_flashstart = CONFIG_SYS_FLASH_BASE;
#endif
	/* size of FLASH memory (final value) */
	bd->bi_flashsize = flash_size;

#if defined(CONFIG_SYS_UPDATE_FLASH_SIZE)
	/* Make a update of the Memctrl. */
	update_flash_size(flash_size);
#endif


#if defined(CONFIG_OXC) || defined(CONFIG_RMU)
	/* flash mapped at end of memory map */
	bd->bi_flashoffset = CONFIG_SYS_TEXT_BASE + flash_size;
#elif CONFIG_SYS_MONITOR_BASE == CONFIG_SYS_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for monitor */
#endif
	return 0;
}
#endif

#ifdef CONFIG_PPC
static int initr_spi(void)
{
	/* PPC does this here */
#ifdef CONFIG_SPI
#if !defined(CONFIG_ENV_IS_IN_EEPROM)
	spi_init_f();
#endif
	spi_init_r();
#endif
	return 0;
}
#endif

#ifdef CONFIG_CMD_NAND
/* go init the NAND */
int initr_nand(void)
{
	puts("NAND:  ");
	nand_init();
	return 0;
}
#endif

#if defined(CONFIG_CMD_ONENAND)
/* go init the NAND */
int initr_onenand(void)
{
	puts("NAND:  ");
	onenand_init();
	return 0;
}
#endif

#ifdef CONFIG_GENERIC_MMC
int initr_mmc(void)
{
	puts("MMC:   ");
	mmc_initialize(gd->bd);
	return 0;
}
#endif

#ifdef CONFIG_HAS_DATAFLASH
/* Ick - why are these not in a header file? */
extern int AT91F_DataflashInit(void);
extern void dataflash_print_info(void);

int initr_dataflash(void)
{
	AT91F_DataflashInit();
	dataflash_print_info();
	return 0;
}
#endif

static int initr_env(void)
{
	/* initialize environment */
	env_relocate();

	/* IP Address */
	gd->bd->bi_ip_addr = getenv_IPaddr("ipaddr");

	/* Initialize from environment */
	load_addr = getenv_ulong("loadaddr", 16, load_addr);
#if defined(CONFIG_CMD_NET)
	{
		char *s = getenv("bootfile");

		if (s != NULL)
			copy_filename(BootFile, s, sizeof(BootFile));
	}
#endif
#if defined(CONFIG_SYS_EXTBDINFO)
#if defined(CONFIG_405GP) || defined(CONFIG_405EP)
#if defined(CONFIG_I2CFAST)
	/*
	 * set bi_iic_fast for linux taking environment variable
	 * "i2cfast" into account
	 */
	{
		char *s = getenv("i2cfast");

		if (s && ((*s == 'y') || (*s == 'Y'))) {
			gd->bd->bi_iic_fast[0] = 1;
			gd->bd->bi_iic_fast[1] = 1;
		}
	}
#endif /* CONFIG_I2CFAST */
#endif /* CONFIG_405GP, CONFIG_405EP */
#endif /* CONFIG_SYS_EXTBDINFO */
	return 0;
}

#ifdef	CONFIG_HERMES
static int initr_hermes(void)
{
	if ((gd->board_type >> 16) == 2)
		gd->bd->bi_ethspeed = gd->board_type & 0xFFFF;
	else
		gd->bd->bi_ethspeed = 0xFFFF;
	return 0;
}

static int initr_hermes_start(void)
{
	if (gd->bd->bi_ethspeed != 0xFFFF)
		hermes_start_lxt980((int) gd->bd->bi_ethspeed);
	return 0;
}
#endif

#ifdef CONFIG_SC3
/* TODO: with new initcalls, move this into the driver */
extern void sc3_read_eeprom(void);

static int initr_sc3_read_eeprom(void)
{
	sc3_read_eeprom();
	return 0;
}
#endif

static int initr_jumptable(void)
{
	jumptable_init();
	return 0;
}

#if defined(CONFIG_API)
static int initr_api(void)
{
	/* Initialize API */
	api_init();
	return 0;
}
#endif

/* enable exceptions */
#ifdef CONFIG_ARM
static int initr_enable_interrupts(void)
{
	enable_interrupts();
	return 0;
}
#endif

#ifdef CONFIG_CMD_NET
static int initr_ethaddr(void)
{
	bd_t *bd = gd->bd;

#if defined(CONFIG_DRIVER_SMC91111) || defined(CONFIG_DRIVER_LAN91C96)
	/* Perform network card initialisation if necessary */
	/* XXX: this needs to be moved to board init */
	/* YYY: with initcalls the drivers can do this directly */
	/* ZZZ: and anyway why not do it when we call eth_init()? */
	if (getenv("ethaddr")) {
		uchar enetaddr[6];
		eth_getenv_enetaddr("ethaddr", enetaddr);
		smc_set_mac_addr(enetaddr);
	}
#endif /* CONFIG_DRIVER_SMC91111 || CONFIG_DRIVER_LAN91C96 */
	/* kept around for legacy kernels only ... ignore the next section */
	eth_getenv_enetaddr("ethaddr", bd->bi_enetaddr);
#ifdef CONFIG_HAS_ETH1
	eth_getenv_enetaddr("eth1addr", bd->bi_enet1addr);
#endif
#ifdef CONFIG_HAS_ETH2
	eth_getenv_enetaddr("eth2addr", bd->bi_enet2addr);
#endif
#ifdef CONFIG_HAS_ETH3
	eth_getenv_enetaddr("eth3addr", bd->bi_enet3addr);
#endif
#ifdef CONFIG_HAS_ETH4
	eth_getenv_enetaddr("eth4addr", bd->bi_enet4addr);
#endif
#ifdef CONFIG_HAS_ETH5
	eth_getenv_enetaddr("eth5addr", bd->bi_enet5addr);
#endif
	return 0;
}
#endif /* CONFIG_CMD_NET */

#ifdef CONFIG_CMD_KGDB
static int initr_kgdb(void)
{
	puts("KGDB:  ");
	kgdb_init();
	return 0;
}
#endif

#if defined(CONFIG_STATUS_LED) && defined(STATUS_LED_BOOT)
static int initr_status_led(void)
{
	status_led_set(STATUS_LED_BOOT, STATUS_LED_BLINKING);

	return 0;
}
#endif

#if defined(CONFIG_CMD_SCSI)
static int initr_scsi(void)
{
	puts("SCSI:  ");
	scsi_init();

	return 0;
}
#endif /* CONFIG_CMD_NET */

#if defined(CONFIG_CMD_DOC)
static int initr_doc(void)
{
	puts("DOC:   ");
	doc_init();
}
#endif

#ifdef CONFIG_BITBANGMII
static int initr_bbmii(void)
{
	bb_miiphy_init();
	return 0;
}
#endif

#ifdef CONFIG_CMD_NET
static int initr_net(void)
{
	puts("Net:   ");
	eth_initialize(gd->bd);
#if defined(CONFIG_RESET_PHY_R)
	debug("Reset Ethernet PHY\n");
	reset_phy();
#endif
	return 0;
}
#endif

#ifdef CONFIG_POST
static int initr_post(void)
{
	post_run(NULL, POST_RAM | post_bootmode_get(0));
	return 0;
}
#endif

#if defined(CONFIG_CMD_PCMCIA) && !defined(CONFIG_CMD_IDE)
static int initr_pcmcia(void)
{
	puts("PCMCIA:");
	pcmcia_init();
	return 0;
}
#endif

#if defined(CONFIG_CMD_IDE)
static int initr_ide(void)
{
#ifdef	CONFIG_IDE_8xx_PCCARD
	puts("PCMCIA:");
#else
	puts("IDE:   ");
#endif
#if defined(CONFIG_START_IDE)
	if (board_start_ide())
		ide_init();
#else
	ide_init();
#endif
	return 0;
}
#endif

#if defined(CONFIG_PRAM) || defined(CONFIG_LOGBUFFER)
/*
 * Export available size of memory for Linux, taking into account the
 * protected RAM at top of memory
 */
int initr_mem(void)
{
	ulong pram = 0;
	char memsz[32];

# ifdef CONFIG_PRAM
	pram = getenv_ulong("pram", 10, CONFIG_PRAM);
# endif
# if defined(CONFIG_LOGBUFFER) && !defined(CONFIG_ALT_LB_ADDR)
	/* Also take the logbuffer into account (pram is in kB) */
	pram += (LOGBUFF_LEN + LOGBUFF_OVERHEAD) / 1024;
# endif
	sprintf(memsz, "%ldk", (gd->ram_size / 1024) - pram);
	setenv("mem", memsz);

	return 0;
}
#endif

#ifdef CONFIG_CMD_BEDBUG
static int initr_bedbug(void)
{
	bedbug_init();

	return 0;
}
#endif

#ifdef CONFIG_PS2KBD
static int initr_kbd(void)
{
	puts("PS/2:  ");
	kbd_init();
	return 0;
}
#endif

#ifdef CONFIG_MODEM_SUPPORT
static int initr_modem(void)
{
	/* TODO: with new initcalls, move this into the driver */
	extern int do_mdm_init;

	do_mdm_init = gd->do_mdm_init;
	return 0;
}
#endif

static int run_main_loop(void)
{
	/* main_loop() can return to retry autoboot, if so just run it again */
	for (;;)
		main_loop();
	return 0;
}

/*
 * Over time we hope to remove these functions with code fragments and
 * stub funtcions, and instead call the relevant function directly.
 *
 * We also hope to remove most of the driver-related init and do it if/when
 * the driver is later used.
 *
 * TODO: perhaps reset the watchdog in the initcall function after each call?
 */
init_fnc_t init_sequence_r[] = {
	initr_reloc,
	/* TODO: could x86/PPC have this also perhaps? */
#ifdef CONFIG_ARM
	initr_caches,
	board_init,	/* Setup chipselects */
#endif
	initr_reloc_global_data,
#ifdef CONFIG_SERIAL_MULTI
	initr_serial,
#endif
	initr_announce,
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_PPC
	initr_trap,
#endif
#ifdef CONFIG_ADDR_MAP
	initr_addr_map,
#endif
#if defined(CONFIG_BOARD_EARLY_INIT_R)
	board_early_init_r,
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_LOGBUFFER
	initr_logbuffer,
#endif
#ifdef CONFIG_POST
	initr_post_backlog,
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_SYS_DELAYED_ICACHE
	initr_icache_enable,
#endif
#if defined(CONFIG_SYS_INIT_RAM_LOCK) && defined(CONFIG_E500)
	initr_unlock_ram_in_cache,
#endif
#if defined(CONFIG_PCI) && defined(CONFIG_SYS_EARLY_PCI_INIT)
	/*
	 * Do early PCI configuration _before_ the flash gets initialised,
	 * because PCU ressources are crucial for flash access on some boards.
	 */
	initr_pci,
#endif
#ifdef CONFIG_WINBOND_83C553
	initr_w83c553f,
#endif
	initr_barrier,
	initr_malloc,
#ifndef CONFIG_SYS_NO_FLASH
	initr_flash,
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_PPC
	/* initialize higher level parts of CPU like time base and timers */
	cpu_init_r,
	initr_spi,
#endif
#ifdef CONFIG_CMD_NAND
	initr_nand,
#endif
#ifdef CONFIG_CMD_ONENAND
	initr_onenand,
#endif
#ifdef CONFIG_GENERIC_MMC
	initr_mmc,
#endif
#ifdef CONFIG_HAS_DATAFLASH
	initr_dataflash,
#endif
	initr_env,
	INIT_FUNC_WATCHDOG_RESET
	initr_secondary_cpu,
#ifdef CONFIG_SC3
	initr_sc3_read_eeprom,
#endif
#ifdef	CONFIG_HERMES
	initr_hermes,
#endif
#if defined(CONFIG_ID_EEPROM) || defined(CONFIG_SYS_I2C_MAC_OFFSET)
	mac_read_from_eeprom,
#endif
	INIT_FUNC_WATCHDOG_RESET
#if defined(CONFIG_PCI) && !defined(CONFIG_SYS_EARLY_PCI_INIT)
	/*
	 * Do pci configuration
	 */
	initr_pci,
#endif
	stdio_init,
	initr_jumptable,
#ifdef CONFIG_API
	initr_api,
#endif
	console_init_r,		/* fully init console as a device */
#ifdef CONFIG_ARCH_MISC_INIT
	arch_misc_init,		/* miscellaneous arch-dependent init */
#endif
#ifdef CONFIG_MISC_INIT_R
	misc_init_r,		/* miscellaneous platform-dependent init */
#endif
#ifdef CONFIG_HERMES
	initr_hermes_start,
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_CMD_KGDB
	initr_kgdb,
#endif
	interrupt_init,
#ifdef CONFIG_ARM
	initr_enable_interrupts,
#endif
#if defined(CONFIG_STATUS_LED) && defined(STATUS_LED_BOOT)
	initr_status_led,
#endif
	/* PPC has a udelay(20) here dating from 2002. Why? */
#ifdef CONFIG_CMD_NET
	initr_ethaddr,
#endif
#ifdef CONFIG_BOARD_LATE_INIT
	board_late_init,
#endif
#ifdef CONFIG_CMD_SCSI
	INIT_FUNC_WATCHDOG_RESET
	initr_scsi,
#endif
#ifdef CONFIG_CMD_DOC
	INIT_FUNC_WATCHDOG_RESET
	initr_doc,
#endif
#ifdef CONFIG_BITBANGMII
	initr_bbmii,
#endif
#ifdef CONFIG_CMD_NET
	INIT_FUNC_WATCHDOG_RESET
	initr_net,
#endif
#ifdef CONFIG_POST
	initr_post,
#endif
#if defined(CONFIG_CMD_PCMCIA) && !defined(CONFIG_CMD_IDE)
	initr_pcmcia,
#endif
#if defined(CONFIG_CMD_IDE)
	initr_ide,
#endif
#ifdef CONFIG_LAST_STAGE_INIT
	INIT_FUNC_WATCHDOG_RESET
	/*
	 * Some parts can be only initialized if all others (like
	 * Interrupts) are up and running (i.e. the PC-style ISA
	 * keyboard).
	 */
	last_stage_init,
#endif
#ifdef CONFIG_CMD_BEDBUG
	INIT_FUNC_WATCHDOG_RESET
	initr_bedbug,
#endif
#if defined(CONFIG_PRAM) || defined(CONFIG_LOGBUFFER)
	initr_mem,
#endif
#ifdef CONFIG_PS2KBD
	initr_kbd,
#endif
#ifdef CONFIG_MODEM_SUPPORT
	initr_modem,
#endif
	run_main_loop,
};

void board_init_r(gd_t *new_gd, ulong dest_addr)
{
	gd = new_gd;
	if (initcall_run_list(init_sequence_r))
		hang();

	/* NOTREACHED - run_main_loop() does not return */
	while (1)
		;
}
