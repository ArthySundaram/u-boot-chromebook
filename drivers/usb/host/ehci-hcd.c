/*-
 * Copyright (c) 2007-2008, Juniper Networks, Inc.
 * Copyright (c) 2008, Excito Elektronik i Skåne AB
 * Copyright (c) 2008, Michael Trimarchi <trimarchimichael@yahoo.it>
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2 of
 * the License.
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
#include <asm/byteorder.h>
#include <usb.h>
#include <asm/io.h>
#include <malloc.h>
#include <watchdog.h>
#ifdef CONFIG_USB_KEYBOARD
#include <stdio_dev.h>
extern unsigned char new[];
#endif

#include "ehci.h"

static struct ehci_ctrl {
	struct ehci_hccr *hccr;	/* R/O registers, not need for volatile */
	volatile struct ehci_hcor *hcor;
	int rootdev;
	uint16_t portreset;
	struct QH qh_list __attribute__((aligned(ARCH_DMA_MINALIGN)));
	struct QH qh_pool __attribute__((aligned(ARCH_DMA_MINALIGN)));
	struct qTD td_pool[3] __attribute__((aligned(ARCH_DMA_MINALIGN)));
	struct QH periodic_queue __attribute__((aligned(ARCH_DMA_MINALIGN)));
	uint32_t *periodic_list;
	int ntds;
} ehcic[CONFIG_USB_MAX_CONTROLLER_COUNT];


static struct descriptor {
	struct usb_hub_descriptor hub;
	struct usb_device_descriptor device;
	struct usb_linux_config_descriptor config;
	struct usb_linux_interface_descriptor interface;
	struct usb_endpoint_descriptor endpoint;
}  __attribute__ ((packed)) descriptor = {
	{
		0x8,		/* bDescLength */
		0x29,		/* bDescriptorType: hub descriptor */
		2,		/* bNrPorts -- runtime modified */
		0,		/* wHubCharacteristics */
		10,		/* bPwrOn2PwrGood */
		0,		/* bHubCntrCurrent */
		{},		/* Device removable */
		{}		/* at most 7 ports! XXX */
	},
	{
		0x12,		/* bLength */
		1,		/* bDescriptorType: UDESC_DEVICE */
		cpu_to_le16(0x0200), /* bcdUSB: v2.0 */
		9,		/* bDeviceClass: UDCLASS_HUB */
		0,		/* bDeviceSubClass: UDSUBCLASS_HUB */
		1,		/* bDeviceProtocol: UDPROTO_HSHUBSTT */
		64,		/* bMaxPacketSize: 64 bytes */
		0x0000,		/* idVendor */
		0x0000,		/* idProduct */
		cpu_to_le16(0x0100), /* bcdDevice */
		1,		/* iManufacturer */
		2,		/* iProduct */
		0,		/* iSerialNumber */
		1		/* bNumConfigurations: 1 */
	},
	{
		0x9,
		2,		/* bDescriptorType: UDESC_CONFIG */
		cpu_to_le16(0x19),
		1,		/* bNumInterface */
		1,		/* bConfigurationValue */
		0,		/* iConfiguration */
		0x40,		/* bmAttributes: UC_SELF_POWER */
		0		/* bMaxPower */
	},
	{
		0x9,		/* bLength */
		4,		/* bDescriptorType: UDESC_INTERFACE */
		0,		/* bInterfaceNumber */
		0,		/* bAlternateSetting */
		1,		/* bNumEndpoints */
		9,		/* bInterfaceClass: UICLASS_HUB */
		0,		/* bInterfaceSubClass: UISUBCLASS_HUB */
		0,		/* bInterfaceProtocol: UIPROTO_HSHUBSTT */
		0		/* iInterface */
	},
	{
		0x7,		/* bLength */
		5,		/* bDescriptorType: UDESC_ENDPOINT */
		0x81,		/* bEndpointAddress:
				 * UE_DIR_IN | EHCI_INTR_ENDPT
				 */
		3,		/* bmAttributes: UE_INTERRUPT */
		8,		/* wMaxPacketSize */
		255		/* bInterval */
	},
};

#if defined(CONFIG_EHCI_IS_TDI)
#define ehci_is_TDI()	(1)
#else
#define ehci_is_TDI()	(0)
#endif

void __ehci_powerup_fixup(uint32_t *status_reg, uint32_t *reg)
{
	mdelay(50);
}

void ehci_powerup_fixup(uint32_t *status_reg, uint32_t *reg)
	__attribute__((weak, alias("__ehci_powerup_fixup")));

static int handshake(uint32_t *ptr, uint32_t mask, uint32_t done, int usec)
{
	uint32_t result;
	do {
		result = ehci_readl(ptr);
		udelay(5);
		if (result == ~(uint32_t)0)
			return -1;
		result &= mask;
		if (result == done)
			return 0;
		usec--;
	} while (usec > 0);
	return -1;
}

static void ehci_free(void *p, size_t sz)
{

}

static int ehci_reset(volatile struct ehci_hcor *hcor)
{
	uint32_t cmd;
	uint32_t tmp;
	uint32_t *reg_ptr;
	int ret = 0;

	cmd = ehci_readl(&hcor->or_usbcmd);
	cmd = CMD_RESET; //(cmd & ~CMD_RUN) | CMD_RESET;
	ehci_writel(&hcor->or_usbcmd, cmd);
	ret = handshake((uint32_t *)&hcor->or_usbcmd, CMD_RESET, 0, 250 * 1000);
	if (ret < 0) {
		printf("EHCI fail to reset\n");
		goto out;
	}

	if (ehci_is_TDI()) {
		reg_ptr = (uint32_t *)((u8 *)hcor + USBMODE);
		tmp = ehci_readl(reg_ptr);
		tmp |= USBMODE_CM_HC;
#if defined(CONFIG_EHCI_MMIO_BIG_ENDIAN)
		tmp |= USBMODE_BE;
#endif
		ehci_writel(reg_ptr, tmp);
	}

#ifdef CONFIG_USB_EHCI_TXFIFO_THRESH
	cmd = ehci_readl(&hcor->or_txfilltuning);
	cmd &= ~TXFIFO_THRESH(0x3f);
	cmd |= TXFIFO_THRESH(CONFIG_USB_EHCI_TXFIFO_THRESH);
	ehci_writel(&hcor->or_txfilltuning, cmd);
#endif
out:
	return ret;
}

static void *ehci_alloc(struct ehci_ctrl *ctrl, size_t sz, size_t align)
{
	void *p;

	switch (sz) {
	case sizeof(struct QH):
		p = &ctrl->qh_pool;
		ctrl->ntds = 0;
		break;
	case sizeof(struct qTD):
		if (ctrl->ntds == 3) {
			debug("out of TDs\n");
			return NULL;
		}
		p = &ctrl->td_pool[ctrl->ntds];
		ctrl->ntds++;
		break;
	default:
		debug("unknown allocation size\n");
		return NULL;
	}

	memset(p, 0, sz);
	return p;
}

static int ehci_td_buffer(struct qTD *td, void *buf, size_t sz)
{
	uint32_t delta, next;
	uint32_t addr = (uint32_t)buf;
	size_t rsz = roundup(sz, ARCH_DMA_MINALIGN);
	int idx;

	if (sz != rsz)
		debug("EHCI-HCD: Misaligned buffer size (%08x)\n", sz);

	if (addr & (ARCH_DMA_MINALIGN - 1))
		debug("EHCI-HCD: Misaligned buffer address (%p)\n", buf);

	idx = 0;
	while (idx < 5) {
		flush_dcache_range(addr, addr + rsz);
		td->qt_buffer[idx] = cpu_to_hc32(addr);
		td->qt_buffer_hi[idx] = 0;
		next = (addr + 4096) & ~4095;
		delta = next - addr;
		if (delta >= sz)
			break;
		sz -= delta;
		addr = next;
		idx++;
	}

	if (idx == 5) {
		debug("out of buffer pointers (%u bytes left)\n", sz);
		return -1;
	}

	return 0;
}

static int
ehci_submit_async(struct usb_device *dev, unsigned long pipe, void *buffer,
		   int length, struct devrequest *req)
{
	struct QH *qh;
	struct qTD *td;
	unsigned long ts;
	uint32_t *tdp;
	uint32_t endpt, token, usbsts;
	uint32_t c, toggle;
	uint32_t cmd;
	int timeout;
	int ret = 0;
	struct ehci_ctrl *ctrl = dev->controller;
	volatile struct ehci_hcor *hcor = ctrl->hcor;

	debug("dev=%p, pipe=%lx, buffer=%p, length=%d, req=%p\n", dev, pipe,
	      buffer, length, req);
	if (req != NULL)
		debug("req=%u (%#x), type=%u (%#x), value=%u (%#x), index=%u\n",
		      req->request, req->request,
		      req->requesttype, req->requesttype,
		      le16_to_cpu(req->value), le16_to_cpu(req->value),
		      le16_to_cpu(req->index));

	qh = ehci_alloc(ctrl, sizeof(struct QH), ARCH_DMA_MINALIGN);
	if (qh == NULL) {
		debug("unable to allocate QH\n");
		return -1;
	}

	toggle = usb_gettoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));

	qh->qh_link = cpu_to_hc32((uint32_t)&ctrl->qh_list | QH_LINK_TYPE_QH);
	c = (usb_pipespeed(pipe) != USB_SPEED_HIGH &&
	     usb_pipeendpoint(pipe) == 0) ? 1 : 0;
	endpt = (8 << 28) |
	    (c << 27) |
	    (usb_maxpacket(dev, pipe) << 16) |
	    (0 << 15) |
	    (1 << 14) |
	    (usb_pipespeed(pipe) << 12) |
	    (usb_pipeendpoint(pipe) << 8) |
	    (0 << 7) | (usb_pipedevice(pipe) << 0);
	qh->qh_endpt1 = cpu_to_hc32(endpt);
	endpt = (1 << 30) |
	    (dev->portnr << 23) |
	    (dev->parent->devnum << 16) | (0 << 8) | (0 << 0);
	qh->qh_endpt2 = cpu_to_hc32(endpt);
	qh->qh_overlay.qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);

	td = NULL;
	tdp = &qh->qh_overlay.qt_next;

	if (req != NULL) {
		td = ehci_alloc(ctrl, sizeof(struct qTD), ARCH_DMA_MINALIGN);
		if (td == NULL) {
			debug("unable to allocate SETUP td\n");
			goto fail;
		}
		td->qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);
		td->qt_altnext = cpu_to_hc32(QT_NEXT_TERMINATE);
		token = (0 << 31) |
		    (sizeof(*req) << 16) |
		    (0 << 15) | (0 << 12) | (3 << 10) | (2 << 8) | (0x80 << 0);
		td->qt_token = cpu_to_hc32(token);
		if (ehci_td_buffer(td, req, sizeof(*req)) != 0) {
			debug("unable construct SETUP td\n");
			ehci_free(td, sizeof(*td));
			goto fail;
		}
		writel(cpu_to_hc32((uint32_t)td), tdp);
		/* Flush the pointer which points at this new descriptor. */
		flush_dcache_range((uint32_t)tdp, (uint32_t)tdp + sizeof(*tdp));
		tdp = &td->qt_next;
		toggle = 1;
		/*
		 * Make sure all the accesses to the td are finished, and then
		 * flush it out to memory.
		 */
		dmb();
		flush_dcache_range((uint32_t)td,
			(uint32_t)td + sizeof(struct qTD));
	}

	if (length > 0 || req == NULL) {
		td = ehci_alloc(ctrl, sizeof(struct qTD), ARCH_DMA_MINALIGN);
		if (td == NULL) {
			debug("unable to allocate DATA td\n");
			goto fail;
		}
		td->qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);
		td->qt_altnext = cpu_to_hc32(QT_NEXT_TERMINATE);
		token = (toggle << 31) |
		    (length << 16) |
		    ((req == NULL ? 1 : 0) << 15) |
		    (0 << 12) |
		    (3 << 10) |
		    ((usb_pipein(pipe) ? 1 : 0) << 8) | (0x80 << 0);
		td->qt_token = cpu_to_hc32(token);
		if (ehci_td_buffer(td, buffer, length) != 0) {
			debug("unable construct DATA td\n");
			ehci_free(td, sizeof(*td));
			goto fail;
		}
		writel(cpu_to_hc32((uint32_t)td), tdp);
		flush_dcache_range((uint32_t)tdp, (uint32_t)tdp + sizeof(*tdp));
		tdp = &td->qt_next;
		dmb();
		flush_dcache_range((uint32_t)td,
			(uint32_t)td + sizeof(struct qTD));
	}

	if (req != NULL) {
		td = ehci_alloc(ctrl, sizeof(struct qTD), ARCH_DMA_MINALIGN);
		if (td == NULL) {
			debug("unable to allocate ACK td\n");
			goto fail;
		}
		td->qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);
		td->qt_altnext = cpu_to_hc32(QT_NEXT_TERMINATE);
		token = (toggle << 31) |
		    (0 << 16) |
		    (1 << 15) |
		    (0 << 12) |
		    (3 << 10) |
		    ((usb_pipein(pipe) ? 0 : 1) << 8) | (0x80 << 0);
		td->qt_token = cpu_to_hc32(token);
		writel(cpu_to_hc32((uint32_t)td), tdp);
		flush_dcache_range((uint32_t)tdp, (uint32_t)tdp + sizeof(*tdp));
		tdp = &td->qt_next;
		dmb();
		flush_dcache_range((uint32_t)td,
			(uint32_t)td + sizeof(struct qTD));
	}

	ctrl->qh_list.qh_link = cpu_to_hc32((uint32_t) qh | QH_LINK_TYPE_QH);

	/* Ensure our writes have happened, and then flush the dcache. */
	dmb();
	flush_dcache_range((uint32_t)(&ctrl->qh_list),
		(uint32_t)(&ctrl->qh_list) + sizeof(struct QH));
	flush_dcache_range((uint32_t)qh, (uint32_t)qh + sizeof(struct QH));

	usbsts = ehci_readl(&hcor->or_usbsts);
	ehci_writel(&hcor->or_usbsts, (usbsts & 0x3f));

	/* Enable async. schedule. */
	cmd = ehci_readl(&hcor->or_usbcmd);
	cmd |= CMD_ASE;
	ehci_writel(&hcor->or_usbcmd, cmd);

	ret = handshake((uint32_t *)&hcor->or_usbsts, STD_ASS, STD_ASS,
			100 * 1000);
	if (ret < 0) {
		printf("EHCI fail timeout STD_ASS set\n");
		goto fail;
	}

	/* Wait for TDs to be processed. */
	ts = get_timer(0);
	timeout = USB_TIMEOUT_MS(pipe);
	do {
		/* Invalidate dcache */
		dmb();
		invalidate_dcache_range((uint32_t)td,
			(uint32_t)td + sizeof(struct qTD));

		token = hc32_to_cpu(readl(&td->qt_token));
		if (!(token & 0x80))
			break;
		WATCHDOG_RESET();
	} while (get_timer(ts) < timeout);

	dmb();
	invalidate_dcache_range((uint32_t)(&ctrl->qh_list),
		(uint32_t)(&ctrl->qh_list) + sizeof(struct QH));
	invalidate_dcache_range((uint32_t)qh,
		(uint32_t)qh + sizeof(struct QH));

	/* Invalidate the memory area occupied by buffer */
	invalidate_dcache_range(((uint32_t)buffer & ~(ARCH_DMA_MINALIGN - 1)),
		((uint32_t)buffer & ~(ARCH_DMA_MINALIGN - 1)) + roundup(length, ARCH_DMA_MINALIGN));

	/* Disable async schedule. */
	cmd = ehci_readl(&hcor->or_usbcmd);
	cmd &= ~CMD_ASE;
	ehci_writel(&hcor->or_usbcmd, cmd);

	/* Check that the TD processing happened */
	if (token & 0x80) {
		printf("EHCI timed out on TD - token=%#x\n", token);
	}

	ret = handshake((uint32_t *)&hcor->or_usbsts, STD_ASS, 0,
			100 * 1000);
	if (ret < 0) {
		printf("EHCI fail timeout STD_ASS reset\n");
		goto fail;
	}

	ctrl->qh_list.qh_link = cpu_to_hc32((uint32_t)&ctrl->qh_list
						| QH_LINK_TYPE_QH);

	token = hc32_to_cpu(qh->qh_overlay.qt_token);
	if (!(token & 0x80)) {
		debug("TOKEN=%#x\n", token);
		switch (token & 0xfc) {
		case 0:
			toggle = token >> 31;
			usb_settoggle(dev, usb_pipeendpoint(pipe),
				       usb_pipeout(pipe), toggle);
			dev->status = 0;
			break;
		case 0x40:
			dev->status = USB_ST_STALLED;
			break;
		case 0xa0:
		case 0x20:
			dev->status = USB_ST_BUF_ERR;
			break;
		case 0x50:
		case 0x10:
			dev->status = USB_ST_BABBLE_DET;
			break;
		default:
			dev->status = USB_ST_CRC_ERR;
			if ((token & 0x40) == 0x40)
				dev->status |= USB_ST_STALLED;
			break;
		}
		dev->act_len = length - ((token >> 16) & 0x7fff);
	} else {
		dev->act_len = 0;
		debug("dev=%u, usbsts=%#x, p[1]=%#x, p[2]=%#x\n",
		      dev->devnum, ehci_readl(&hcor->or_usbsts),
		      ehci_readl(&hcor->or_portsc[0]),
		      ehci_readl(&hcor->or_portsc[1]));
	}

	return (dev->status != USB_ST_NOT_PROC) ? 0 : -1;

fail:
	td = (void *)hc32_to_cpu(qh->qh_overlay.qt_next);
	while (td != (void *)QT_NEXT_TERMINATE) {
		qh->qh_overlay.qt_next = td->qt_next;
		ehci_free(td, sizeof(*td));
		td = (void *)hc32_to_cpu(qh->qh_overlay.qt_next);
	}
	ehci_free(qh, sizeof(*qh));
	return -1;
}

static inline int min3(int a, int b, int c)
{

	if (b < a)
		a = b;
	if (c < a)
		a = c;
	return a;
}

int
ehci_submit_root(struct usb_device *dev, unsigned long pipe, void *buffer,
		 int length, struct devrequest *req)
{
	uint8_t tmpbuf[4];
	u16 typeReq;
	void *srcptr = NULL;
	int len, srclen;
	uint32_t reg;
	uint32_t *status_reg;
	struct ehci_ctrl *ctrl = dev->controller;
	struct ehci_hccr *hccr = ctrl->hccr;
	volatile struct ehci_hcor *hcor = ctrl->hcor;

	if (le16_to_cpu(req->index) > CONFIG_SYS_USB_EHCI_MAX_ROOT_PORTS) {
		printf("The request port(%d) is not configured\n",
			le16_to_cpu(req->index) - 1);
		return -1;
	}
	status_reg = (uint32_t *)&hcor->or_portsc[
						le16_to_cpu(req->index) - 1];
	srclen = 0;

	debug("req=%u (%#x), type=%u (%#x), value=%u, index=%u\n",
	      req->request, req->request,
	      req->requesttype, req->requesttype,
	      le16_to_cpu(req->value), le16_to_cpu(req->index));

	typeReq = req->request | req->requesttype << 8;

	switch (typeReq) {
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		switch (le16_to_cpu(req->value) >> 8) {
		case USB_DT_DEVICE:
			debug("USB_DT_DEVICE request\n");
			srcptr = &descriptor.device;
			srclen = 0x12;
			break;
		case USB_DT_CONFIG:
			debug("USB_DT_CONFIG config\n");
			srcptr = &descriptor.config;
			srclen = 0x19;
			break;
		case USB_DT_STRING:
			debug("USB_DT_STRING config\n");
			switch (le16_to_cpu(req->value) & 0xff) {
			case 0:	/* Language */
				srcptr = "\4\3\1\0";
				srclen = 4;
				break;
			case 1:	/* Vendor */
				srcptr = "\16\3u\0-\0b\0o\0o\0t\0";
				srclen = 14;
				break;
			case 2:	/* Product */
				srcptr = "\52\3E\0H\0C\0I\0 "
					 "\0H\0o\0s\0t\0 "
					 "\0C\0o\0n\0t\0r\0o\0l\0l\0e\0r\0";
				srclen = 42;
				break;
			default:
				debug("unknown value DT_STRING %x\n",
					le16_to_cpu(req->value));
				goto unknown;
			}
			break;
		default:
			debug("unknown value %x\n", le16_to_cpu(req->value));
			goto unknown;
		}
		break;
	case USB_REQ_GET_DESCRIPTOR | ((USB_DIR_IN | USB_RT_HUB) << 8):
		switch (le16_to_cpu(req->value) >> 8) {
		case USB_DT_HUB:
			debug("USB_DT_HUB config\n");
			srcptr = &descriptor.hub;
			srclen = 0x8;
			break;
		default:
			debug("unknown value %x\n", le16_to_cpu(req->value));
			goto unknown;
		}
		break;
	case USB_REQ_SET_ADDRESS | (USB_RECIP_DEVICE << 8):
		debug("USB_REQ_SET_ADDRESS\n");
		ctrl->rootdev = le16_to_cpu(req->value);
		break;
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		debug("USB_REQ_SET_CONFIGURATION\n");
		/* Nothing to do */
		break;
	case USB_REQ_GET_STATUS | ((USB_DIR_IN | USB_RT_HUB) << 8):
		tmpbuf[0] = 1;	/* USB_STATUS_SELFPOWERED */
		tmpbuf[1] = 0;
		srcptr = tmpbuf;
		srclen = 2;
		break;
	case USB_REQ_GET_STATUS | ((USB_RT_PORT | USB_DIR_IN) << 8):
		memset(tmpbuf, 0, 4);
		reg = ehci_readl(status_reg);
		if (reg & EHCI_PS_CS)
			tmpbuf[0] |= USB_PORT_STAT_CONNECTION;
		if (reg & EHCI_PS_PE)
			tmpbuf[0] |= USB_PORT_STAT_ENABLE;
		if (reg & EHCI_PS_SUSP)
			tmpbuf[0] |= USB_PORT_STAT_SUSPEND;
		if (reg & EHCI_PS_OCA)
			tmpbuf[0] |= USB_PORT_STAT_OVERCURRENT;
		if (reg & EHCI_PS_PR)
			tmpbuf[0] |= USB_PORT_STAT_RESET;
		if (reg & EHCI_PS_PP)
			tmpbuf[1] |= USB_PORT_STAT_POWER >> 8;

		if (ehci_is_TDI()) {
			switch ((reg >> 26) & 3) {
			case 0:
				break;
			case 1:
				tmpbuf[1] |= USB_PORT_STAT_LOW_SPEED >> 8;
				break;
			case 2:
			default:
				tmpbuf[1] |= USB_PORT_STAT_HIGH_SPEED >> 8;
				break;
			}
		} else {
			tmpbuf[1] |= USB_PORT_STAT_HIGH_SPEED >> 8;
		}

		if (reg & EHCI_PS_CSC)
			tmpbuf[2] |= USB_PORT_STAT_C_CONNECTION;
		if (reg & EHCI_PS_PEC)
			tmpbuf[2] |= USB_PORT_STAT_C_ENABLE;
		if (reg & EHCI_PS_OCC)
			tmpbuf[2] |= USB_PORT_STAT_C_OVERCURRENT;
		if (ctrl->portreset & (1 << le16_to_cpu(req->index)))
			tmpbuf[2] |= USB_PORT_STAT_C_RESET;

		srcptr = tmpbuf;
		srclen = 4;
		break;
	case USB_REQ_SET_FEATURE | ((USB_DIR_OUT | USB_RT_PORT) << 8):
		reg = ehci_readl(status_reg);
		reg &= ~EHCI_PS_CLEAR;
		switch (le16_to_cpu(req->value)) {
		case USB_PORT_FEAT_ENABLE:
			reg |= EHCI_PS_PE;
			ehci_writel(status_reg, reg);
			break;
		case USB_PORT_FEAT_POWER:
			if (HCS_PPC(ehci_readl(&hccr->cr_hcsparams))) {
				reg |= EHCI_PS_PP;
				ehci_writel(status_reg, reg);
			}
			break;
		case USB_PORT_FEAT_RESET:
			if ((reg & (EHCI_PS_PE | EHCI_PS_CS)) == EHCI_PS_CS &&
			    !ehci_is_TDI() &&
			    EHCI_PS_IS_LOWSPEED(reg)) {
				/* Low speed device, give up ownership. */
				debug("port %d low speed --> companion\n",
				      req->index - 1);
				reg |= EHCI_PS_PO;
				ehci_writel(status_reg, reg);
				break;
			} else {
				int ret;

				reg |= EHCI_PS_PR;
				reg &= ~EHCI_PS_PE;
				ehci_writel(status_reg, reg);
				/*
				 * caller must wait, then call GetPortStatus
				 * usb 2.0 specification say 50 ms resets on
				 * root
				 */
				ehci_powerup_fixup(status_reg, &reg);

				ehci_writel(status_reg, reg & ~EHCI_PS_PR);
				/*
				 * A host controller must terminate the reset
				 * and stabilize the state of the port within
				 * 2 milliseconds
				 */
				ret = handshake(status_reg, EHCI_PS_PR, 0,
						2 * 1000);
				if (!ret)
					ctrl->portreset |=
						1 << le16_to_cpu(req->index);
				else
					printf("port(%d) reset error\n",
					le16_to_cpu(req->index) - 1);
			}
			break;
		default:
			debug("unknown feature %x\n", le16_to_cpu(req->value));
			goto unknown;
		}
		/* unblock posted writes */
		(void) ehci_readl(&hcor->or_usbcmd);
		break;
	case USB_REQ_CLEAR_FEATURE | ((USB_DIR_OUT | USB_RT_PORT) << 8):
		reg = ehci_readl(status_reg);
		switch (le16_to_cpu(req->value)) {
		case USB_PORT_FEAT_ENABLE:
			reg &= ~EHCI_PS_PE;
			break;
		case USB_PORT_FEAT_C_ENABLE:
			reg = (reg & ~EHCI_PS_CLEAR) | EHCI_PS_PE;
			break;
		case USB_PORT_FEAT_POWER:
			if (HCS_PPC(ehci_readl(&hccr->cr_hcsparams)))
				reg = reg & ~(EHCI_PS_CLEAR | EHCI_PS_PP);
		case USB_PORT_FEAT_C_CONNECTION:
			reg = (reg & ~EHCI_PS_CLEAR) | EHCI_PS_CSC;
			break;
		case USB_PORT_FEAT_OVER_CURRENT:
			reg = (reg & ~EHCI_PS_CLEAR) | EHCI_PS_OCC;
			break;
		case USB_PORT_FEAT_C_RESET:
			ctrl->portreset &= ~(1 << le16_to_cpu(req->index));
			break;
		default:
			debug("unknown feature %x\n", le16_to_cpu(req->value));
			goto unknown;
		}
		ehci_writel(status_reg, reg);
		/* unblock posted write */
		(void) ehci_readl(&hcor->or_usbcmd);
		break;
	default:
		debug("Unknown request\n");
		goto unknown;
	}

	mdelay(1);
	len = min3(srclen, le16_to_cpu(req->length), length);
	if (srcptr != NULL && len > 0)
		memcpy(buffer, srcptr, len);
	else
		debug("Len is 0\n");

	dev->act_len = len;
	dev->status = 0;
	return 0;

unknown:
	debug("requesttype=%x, request=%x, value=%x, index=%x, length=%x\n",
	      req->requesttype, req->request, le16_to_cpu(req->value),
	      le16_to_cpu(req->index), le16_to_cpu(req->length));

	dev->act_len = 0;
	dev->status = USB_ST_STALLED;
	return -1;
}

void usb_lowlevel_stop(int index)
{
	ehci_hcd_stop(index);
}

void *usb_lowlevel_init(int index)
{
	uint32_t reg;
	uint32_t cmd;
	struct ehci_hccr *hccr;
	volatile struct ehci_hcor *hcor;
	struct QH *qh_list;
	struct QH *periodic;

	if (ehci_hcd_init(index, &hccr, (struct ehci_hcor **)&hcor) != 0)
		return NULL;

	/* EHCI spec section 4.1 */
	if (ehci_reset(hcor) != 0)
		return NULL;

#if defined(CONFIG_EHCI_HCD_INIT_AFTER_RESET)
	if (ehci_hcd_init(index, &hccr, (struct ehci_hcor **)&hcor) != 0)
		return NULL;
#endif
	ehcic[index].hccr = hccr;
	ehcic[index].hcor = hcor;
	qh_list = &ehcic[index].qh_list;

	/* Set the high address word (aka segment) for 64-bit controller */
	if (ehci_readl(&hccr->cr_hccparams) & 1) /* 64-bit Addressing */
		ehci_writel(&hcor->or_ctrldssegment, 0);

	/* Set head of reclaim list */
	memset(qh_list, 0, sizeof(*qh_list));
	qh_list->qh_link = cpu_to_hc32((uint32_t)qh_list | QH_LINK_TYPE_QH);
	qh_list->qh_endpt1 = cpu_to_hc32((1 << 15) | (USB_SPEED_HIGH << 12));
	qh_list->qh_curtd = cpu_to_hc32(QT_NEXT_TERMINATE);
	qh_list->qh_overlay.qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);
	qh_list->qh_overlay.qt_altnext = cpu_to_hc32(QT_NEXT_TERMINATE);
	qh_list->qh_overlay.qt_token = cpu_to_hc32(0x40);

	/* Set async. queue head pointer. */
	ehci_writel(&hcor->or_asynclistaddr, (uint32_t)qh_list);

	/* Set up periodic list */
	/* Step 1: Parent QH for all periodic transfers. */
	periodic = &ehcic[index].periodic_queue;
	memset(periodic, 0, sizeof(*periodic));
	periodic->qh_link = cpu_to_hc32(QH_LINK_TERMINATE);
	periodic->qh_overlay.qt_next = cpu_to_hc32(QT_NEXT_TERMINATE);
	periodic->qh_overlay.qt_altnext = cpu_to_hc32(QT_NEXT_TERMINATE);

	/* Step 2: Setup frame-list: Every microframe, USB tries the same list.
	 *         In particular, device specifications on polling frequency
	 *         are disregarded. Keyboards seem to send NAK/NYet reliably
	 *         when polled with an empty buffer.
	 *
	 *         Split Transactions will be spread across microframes using
	 *         S-mask and C-mask.
	 */
	ehcic[index].periodic_list = memalign(4096, 1024*4);
	int i;
	for (i = 0; i < 1024; i++)
		ehcic[index].periodic_list[i] = (uint32_t)periodic
						| QH_LINK_TYPE_QH;

	/* Set periodic list base address */
	ehci_writel(&hcor->or_periodiclistbase,
		(uint32_t)ehcic[index].periodic_list);

	reg = ehci_readl(&hccr->cr_hcsparams);
	descriptor.hub.bNbrPorts = HCS_N_PORTS(reg);
	printf("Register %x NbrPorts %d\n", reg, descriptor.hub.bNbrPorts);

	/* Reset hub */
if (1) {
	int i;
	for (i = 0; i < 2; i++) {
	int ret;
	int index = i;
	uint32_t *status_reg;
	status_reg = (uint32_t *)&hcor->or_portsc[
						le16_to_cpu(index) - 1];

	printf("[USB] %s:%d %d\n", __func__, __LINE__, i);
	reg = ehci_readl(status_reg);
	reg |= EHCI_PS_PR;
	reg &= ~EHCI_PS_PE;
	ehci_writel(status_reg, reg);
	/*
	 * caller must wait, then call GetPortStatus
	 * usb 2.0 specification say 50 ms resets on
	 * root
	 */
	ehci_powerup_fixup(status_reg, &reg);

	ehci_writel(status_reg, reg & ~EHCI_PS_PR);
	/*
	 * A host controller must terminate the reset
	 * and stabilize the state of the port within
	 * 2 milliseconds
	 */
	ret = handshake(status_reg, EHCI_PS_PR, 0,
			2 * 1000);
	if (!ret)
		ehcic[index].portreset |=
			1 << le16_to_cpu(index);
	else
		printf("port(%d) reset error\n",
		le16_to_cpu(index) - 1);
	}
}

	/* Port Indicators */
	if (HCS_INDICATOR(reg))
		descriptor.hub.wHubCharacteristics |= 0x80;
	/* Port Power Control */
	if (HCS_PPC(reg))
		descriptor.hub.wHubCharacteristics |= 0x01;

	/* Start the host controller. */
	cmd = ehci_readl(&hcor->or_usbcmd);
	/*
	 * Philips, Intel, and maybe others need CMD_RUN before the
	 * root hub will detect new devices (why?); NEC doesn't
	 */
	cmd &= ~(CMD_LRESET|CMD_IAAD|CMD_PSE|CMD_ASE|CMD_RESET);
	cmd |= CMD_RUN;
	ehci_writel(&hcor->or_usbcmd, cmd);

	/* take control over the ports */
	cmd = ehci_readl(&hcor->or_configflag);
	cmd |= FLAG_CF;
	ehci_writel(&hcor->or_configflag, cmd);
	/* unblock posted write */
	cmd = ehci_readl(&hcor->or_usbcmd);
	mdelay(5);
	reg = HC_VERSION(ehci_readl(&hccr->cr_capbase));
	printf("USB EHCI %x.%02x\n", reg >> 8, reg & 0xff);

	ehcic[index].rootdev = 0;

	return ehcic + index;
}

int
submit_bulk_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		int length)
{

	if (usb_pipetype(pipe) != PIPE_BULK) {
		debug("non-bulk pipe (type=%lu)", usb_pipetype(pipe));
		return -1;
	}
	return ehci_submit_async(dev, pipe, buffer, length, NULL);
}

int
submit_control_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		   int length, struct devrequest *setup)
{
	struct ehci_ctrl *ctrl = dev->controller;

	if (usb_pipetype(pipe) != PIPE_CONTROL) {
		debug("non-control pipe (type=%lu)", usb_pipetype(pipe));
		return -1;
	}

	if (usb_pipedevice(pipe) == ctrl->rootdev) {
		if (ctrl->rootdev == 0)
			dev->speed = USB_SPEED_HIGH;
		return ehci_submit_root(dev, pipe, buffer, length, setup);
	}
	return ehci_submit_async(dev, pipe, buffer, length, setup);
}

struct int_queue {
	struct QH *first;
	struct QH *current;
	struct QH *last;
	struct qTD *tds;
};

#define NEXT_QH(qh) (struct QH *)((qh)->qh_link & ~0x1f)

static int
enable_periodic(struct ehci_ctrl *ctrl)
{
	uint32_t cmd;
	volatile struct ehci_hcor *hcor = ctrl->hcor;
	cmd = ehci_readl(&hcor->or_usbcmd);
	cmd |= CMD_PSE;
	ehci_writel(&hcor->or_usbcmd, cmd);

	int ret = handshake((uint32_t *)&hcor->or_usbsts,
			STD_PSS, STD_PSS, 100 * 1000);
	if (ret < 0) {
		printf("EHCI failed: timeout when enabling periodic list\n");
		return -1;
	}
	udelay(1000);
	return 0;
}

static int
disable_periodic(struct ehci_ctrl *ctrl)
{
	uint32_t cmd;
	volatile struct ehci_hcor *hcor = ctrl->hcor;
	cmd = ehci_readl(&hcor->or_usbcmd);
	cmd &= ~CMD_PSE;
	ehci_writel(&hcor->or_usbcmd, cmd);

	int ret = handshake((uint32_t *)&hcor->or_usbsts,
			STD_PSS, 0, 100 * 1000);
	if (ret < 0) {
		printf("EHCI failed: timeout when enabling periodic list\n");
		return -1;
	}
	return 0;
}

int periodic_schedules;

struct int_queue *
create_int_queue(struct usb_device *dev, unsigned long pipe, int queuesize,
		 int elementsize, void *buffer)
{
	int i;
	struct ehci_ctrl *ctrl = dev->controller;
	struct int_queue *result = NULL;
	debug("Enter create_int_queue\n");
	if (usb_pipetype(pipe) != PIPE_INTERRUPT) {
		debug("non-interrupt pipe (type=%lu)", usb_pipetype(pipe));
		return NULL;
	}

	/* limit to 4 full pages worth of data -
	 * we can safely fit them in a single TD,
	 * no matter the alignment
	 */
	if (elementsize >= 16384) {
		debug("too large elements for interrupt transfers\n");
		return NULL;
	}

	result = malloc(sizeof(*result));
	if (!result) {
		debug("ehci intr queue: out of memory\n");
		goto fail1;
	}
	result->first = memalign(ARCH_DMA_MINALIGN, sizeof(struct QH) * queuesize);
	if (!result->first) {
		debug("ehci intr queue: out of memory\n");
		goto fail2;
	}
	result->current = result->first;
	result->last = result->first + elementsize - 1;
	result->tds = memalign(ARCH_DMA_MINALIGN, sizeof(struct qTD) * queuesize);
	if (!result->tds) {
		debug("ehci intr queue: out of memory\n");
		goto fail3;
	}
	memset(result->first, 0, sizeof(struct QH) * queuesize);
	memset(result->tds, 0, sizeof(struct qTD) * queuesize);

	for (i = 0; i < queuesize; i++) {
		struct QH *qh = result->first + i;
		struct qTD *td = result->tds + i;
		void **buf = (void **)qh->fill;

		qh->qh_link = (uint32_t)(qh+1) | QH_LINK_TYPE_QH;
		if (i == queuesize - 1)
			qh->qh_link = QH_LINK_TERMINATE;

		qh->qh_overlay.qt_next = (uint32_t)td;
		qh->qh_endpt1 = (0 << 28) | /* No NAK reload (ehci 4.9) */
			(usb_maxpacket(dev, pipe) << 16) | /* MPS */
			(1 << 14) | /* TODO: Data Toggle Control */
			(usb_pipespeed(pipe) << 12) | /* EPS */
			(usb_pipeendpoint(pipe) << 8) | /* Endpoint Number */
			(usb_pipedevice(pipe) << 0);
		qh->qh_endpt2 = (1 << 30); /* 1 Tx per mframe */
		if (usb_pipespeed(pipe) < 2) { /* full or low speed */
			debug("TT: port: %d, hub address: %d\n",
				dev->portnr, dev->parent->devnum);
			qh->qh_endpt2 |= (dev->portnr << 23) |
				(dev->parent->devnum << 16) |
				(0x1c << 8) | /* C-mask: microframes 2-4 */
				(1 << 0); /* S-mask: microframe 0 */
		}

		td->qt_next = QT_NEXT_TERMINATE;
		td->qt_altnext = QT_NEXT_TERMINATE;
		debug("communication direction is '%s'\n",
		      usb_pipein(pipe) ? "in" : "out");
		td->qt_token = (elementsize << 16) |
			((usb_pipein(pipe) ? 1 : 0) << 8) | /* IN/OUT token */
			0x80; /* active */
		td->qt_buffer[0] = (uint32_t)buffer + i * elementsize;
		td->qt_buffer[1] = (td->qt_buffer[0] + 0x1000) & ~0xfff;
		td->qt_buffer[2] = (td->qt_buffer[0] + 0x2000) & ~0xfff;
		td->qt_buffer[3] = (td->qt_buffer[0] + 0x3000) & ~0xfff;
		td->qt_buffer[4] = (td->qt_buffer[0] + 0x4000) & ~0xfff;

		*buf = buffer + i * elementsize;
	}

	if (disable_periodic(ctrl) < 0) {
		debug("FATAL: periodic should never fail, but did");
		goto fail3;
	}

	/* hook up to periodic list */
	struct QH *list = &ctrl->periodic_queue;
	result->last->qh_link = list->qh_link;
	list->qh_link = (uint32_t)result->first | QH_LINK_TYPE_QH;

	if (enable_periodic(ctrl) < 0) {
		debug("FATAL: periodic should never fail, but did");
		goto fail3;
	}
	periodic_schedules++;

	debug("Exit create_int_queue\n");
	return result;
fail3:
	if (result->tds)
		free(result->tds);
fail2:
	if (result->first)
		free(result->first);
	if (result)
		free(result);
fail1:
	return NULL;
}

/* TODO: requeue depleted buffers */
void *
poll_int_queue(struct usb_device *dev, struct int_queue *queue)
{
	struct QH *cur = queue->current;
	/* depleted queue */
	if (cur == NULL) {
		debug("Exit poll_int_queue with completed queue\n");
		return NULL;
	}
	/* still active */
	if (cur->qh_overlay.qt_token & 0x80) {
		debug("Exit poll_int_queue with no completed intr transfer. "
		      "token is %x\n", cur->qh_overlay.qt_token);
		return NULL;
	}
	/* TODO: Handle failures */
	if (!(cur->qh_link & QH_LINK_TERMINATE))
		queue->current++;
	else
		queue->current = NULL;
	debug("Exit poll_int_queue with completed intr transfer. "
	      "token is %x at %p (first at %p)\n", cur->qh_overlay.qt_token,
	      &cur->qh_overlay.qt_token, queue->first);
	return (void *)cur->fill[0];
}

/* Do not free buffers associated with QHs, they're owned by someone else */
int
destroy_int_queue(struct usb_device *dev, struct int_queue *queue)
{
	struct ehci_ctrl *ctrl = dev->controller;
	int result = -1;
	unsigned long timeout;

	if (disable_periodic(ctrl) < 0) {
		debug("FATAL: periodic should never fail, but did");
		goto out;
	}
	periodic_schedules--;

	struct QH *cur = &ctrl->periodic_queue;
	timeout = get_timer(0) + 500; /* abort after 500ms */
	while (!(cur->qh_link & QH_LINK_TERMINATE)) {
		debug("considering %p, with qh_link %x\n", cur, cur->qh_link);
		if (NEXT_QH(cur) == queue->first) {
			debug("found candidate. removing from chain\n");
			cur->qh_link = queue->last->qh_link;
			result = 0;
			goto out;
		}
		cur = NEXT_QH(cur);
		if (get_timer(0) > timeout) {
			printf("Timeout destroying interrupt endpoint queue\n");
			result = -1;
			break;
		}
	}

	if (periodic_schedules > 0)
		if (enable_periodic(ctrl) < 0) {
			debug("FATAL: periodic should never fail, but did");
			result = -1;
		}

out:
	free(queue->tds);
	free(queue->first);
	free(queue);
	return result;
}

int
submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
	       int length, int interval)
{
	void *backbuffer;
	struct int_queue *queue;
	unsigned long timeout;
	int result = 0;

	debug("dev=%p, pipe=%lu, buffer=%p, length=%d, interval=%d",
	      dev, pipe, buffer, length, interval);

	queue = create_int_queue(dev, pipe, 1, length, buffer);

	timeout = get_timer(0) + USB_TIMEOUT_MS(pipe);
	while ((backbuffer = poll_int_queue(dev, queue)) == NULL)
		if (get_timer(0) > timeout) {
			printf("Timeout poll on interrupt endpoint\n");
			result = -1;
			break;
		}

	if (backbuffer != buffer) {
		debug("got wrong buffer back (%x instead of %x)\n",
		      (uint32_t)backbuffer, (uint32_t)buffer);
		return -1;
	}

	if (destroy_int_queue(dev, queue) == -1)
		return -1;

	/* everything worked out fine */
	return result;
}

#ifdef CONFIG_SYS_USB_EVENT_POLL
/*
 * This function polls for USB keyboard data.
 */
void usb_event_poll()
{
	struct stdio_dev *dev;
	struct usb_device *usb_kbd_dev;
	struct usb_interface *iface;
	struct usb_endpoint_descriptor *ep;
	int pipe;
	int maxp;

	/* Get the pointer to USB Keyboard device pointer */
	dev = stdio_get_by_name("usbkbd");
	usb_kbd_dev = (struct usb_device *)dev->priv;
	iface = &usb_kbd_dev->config.if_desc[0];
	ep = &iface->ep_desc[0];
	pipe = usb_rcvintpipe(usb_kbd_dev, ep->bEndpointAddress);

	/* Submit a interrupt transfer request */
	maxp = usb_maxpacket(usb_kbd_dev, pipe);
	usb_submit_int_msg(usb_kbd_dev, pipe, &new[0],
			maxp > 8 ? 8 : maxp, ep->bInterval);
}
#endif /* CONFIG_SYS_USB_EVENT_POLL */
