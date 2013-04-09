/*
 * Copyright (C) 2012 Samsung Electronics
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

#include <asm/types.h>
#include <asm/arch-exynos/cpu.h>
#include <asm/arch-exynos/spl.h>

unsigned int exynos_get_uboot_size(void)
{
	/* Limit u-boot size to 256KB  */
	return 0x40000;
}

enum boot_mode exynos_get_boot_device(void)
{
	/* Only MMC boot is currently supported on Exynos4 */
	return BOOT_MODE_MMC;
}
