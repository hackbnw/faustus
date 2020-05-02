/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Asus PC WMI hotkey driver
 *
 * Copyright(C) 2010 Intel Corporation.
 * Copyright(C) 2010-2011 Corentin Chary <corentin.chary@gmail.com>
 *
 * Portions based on wistron_btns.c:
 * Copyright (C) 2005 Miloslav Trmac <mitr@volny.cz>
 * Copyright (C) 2005 Bernhard Rosenkraenzer <bero@arklinux.org>
 * Copyright (C) 2005 Dmitry Torokhov <dtor@mail.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __FAUSTUS_H
#define __FAUSTUS_H

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/platform_device.h>
#include <linux/i8042.h>

// platform_data/x86/asus-wmi.h

/* WMI Methods */
#define ASUS_WMI_METHODID_SPEC	        0x43455053 /* BIOS SPECification */
#define ASUS_WMI_METHODID_SFBD		0x44424653 /* Set First Boot Device */
#define ASUS_WMI_METHODID_GLCD		0x44434C47 /* Get LCD status */
#define ASUS_WMI_METHODID_GPID		0x44495047 /* Get Panel ID?? (Resol) */
#define ASUS_WMI_METHODID_QMOD		0x444F4D51 /* Quiet MODe */
#define ASUS_WMI_METHODID_SPLV		0x4C425053 /* Set Panel Light Value */
#define ASUS_WMI_METHODID_AGFN		0x4E464741 /* FaN? */
#define ASUS_WMI_METHODID_SFUN		0x4E554653 /* FUNCtionalities */
#define ASUS_WMI_METHODID_SDSP		0x50534453 /* Set DiSPlay output */
#define ASUS_WMI_METHODID_GDSP		0x50534447 /* Get DiSPlay output */
#define ASUS_WMI_METHODID_DEVP		0x50564544 /* DEVice Policy */
#define ASUS_WMI_METHODID_OSVR		0x5256534F /* OS VeRsion */
#define ASUS_WMI_METHODID_DCTS		0x53544344 /* Device status (DCTS) */
#define ASUS_WMI_METHODID_DSTS		0x53545344 /* Device status (DSTS) */
#define ASUS_WMI_METHODID_BSTS		0x53545342 /* Bios STatuS ? */
#define ASUS_WMI_METHODID_DEVS		0x53564544 /* DEVice Set */
#define ASUS_WMI_METHODID_CFVS		0x53564643 /* CPU Frequency Volt Set */
#define ASUS_WMI_METHODID_KBFT		0x5446424B /* KeyBoard FilTer */
#define ASUS_WMI_METHODID_INIT		0x54494E49 /* INITialize */
#define ASUS_WMI_METHODID_HKEY		0x59454B48 /* Hot KEY ?? */

#define ASUS_WMI_UNSUPPORTED_METHOD	0xFFFFFFFE

/* Wireless */
#define ASUS_WMI_DEVID_HW_SWITCH	0x00010001
#define ASUS_WMI_DEVID_WIRELESS_LED	0x00010002
#define ASUS_WMI_DEVID_CWAP		0x00010003
#define ASUS_WMI_DEVID_WLAN		0x00010011
#define ASUS_WMI_DEVID_WLAN_LED		0x00010012
#define ASUS_WMI_DEVID_BLUETOOTH	0x00010013
#define ASUS_WMI_DEVID_GPS		0x00010015
#define ASUS_WMI_DEVID_WIMAX		0x00010017
#define ASUS_WMI_DEVID_WWAN3G		0x00010019
#define ASUS_WMI_DEVID_UWB		0x00010021

/* Leds */
/* 0x000200XX and 0x000400XX */
#define ASUS_WMI_DEVID_LED1		0x00020011
#define ASUS_WMI_DEVID_LED2		0x00020012
#define ASUS_WMI_DEVID_LED3		0x00020013
#define ASUS_WMI_DEVID_LED4		0x00020014
#define ASUS_WMI_DEVID_LED5		0x00020015
#define ASUS_WMI_DEVID_LED6		0x00020016

/* Backlight and Brightness */
#define ASUS_WMI_DEVID_ALS_ENABLE	0x00050001 /* Ambient Light Sensor */
#define ASUS_WMI_DEVID_BACKLIGHT	0x00050011
#define ASUS_WMI_DEVID_BRIGHTNESS	0x00050012
#define ASUS_WMI_DEVID_KBD_BACKLIGHT	0x00050021
#define ASUS_WMI_DEVID_LIGHT_SENSOR	0x00050022 /* ?? */
#define ASUS_WMI_DEVID_LIGHTBAR		0x00050025
#define ASUS_WMI_DEVID_FAN_BOOST_MODE	0x00110018
#define ASUS_WMI_DEVID_THROTTLE_THERMAL_POLICY 0x00120075
#define ASUS_WMI_DEVID_KBD_RGB		0x00100056
#define ASUS_WMI_DEVID_KBD_RGB2		0x00100057

/* Misc */
#define ASUS_WMI_DEVID_CAMERA		0x00060013

/* Storage */
#define ASUS_WMI_DEVID_CARDREADER	0x00080013

/* Input */
#define ASUS_WMI_DEVID_TOUCHPAD		0x00100011
#define ASUS_WMI_DEVID_TOUCHPAD_LED	0x00100012
#define ASUS_WMI_DEVID_FNLOCK		0x00100023

/* Fan, Thermal */
#define ASUS_WMI_DEVID_THERMAL_CTRL	0x00110011
#define ASUS_WMI_DEVID_FAN_CTRL		0x00110012 /* deprecated */
#define ASUS_WMI_DEVID_CPU_FAN_CTRL	0x00110013

/* Power */
#define ASUS_WMI_DEVID_PROCESSOR_STATE	0x00120012

/* Deep S3 / Resume on LID open */
#define ASUS_WMI_DEVID_LID_RESUME	0x00120031

/* Maximum charging percentage */
#define ASUS_WMI_DEVID_RSOC		0x00120057

/* DSTS masks */
#define ASUS_WMI_DSTS_STATUS_BIT	0x00000001
#define ASUS_WMI_DSTS_UNKNOWN_BIT	0x00000002
#define ASUS_WMI_DSTS_PRESENCE_BIT	0x00010000
#define ASUS_WMI_DSTS_USER_BIT		0x00020000
#define ASUS_WMI_DSTS_BIOS_BIT		0x00040000
#define ASUS_WMI_DSTS_BRIGHTNESS_MASK	0x000000FF
#define ASUS_WMI_DSTS_MAX_BRIGTH_MASK	0x0000FF00
#define ASUS_WMI_DSTS_LIGHTBAR_MASK	0x0000000F

// drivers/platform/x86/asus-wmi.h

#define ASUS_WMI_KEY_IGNORE (-1)
#define ASUS_WMI_BRN_DOWN	0x20
#define ASUS_WMI_BRN_UP		0x2f

struct module;
struct key_entry;
struct asus_wmi;

struct quirk_entry {
	bool hotplug_wireless;
	bool scalar_panel_brightness;
	bool store_backlight_power;
	bool wmi_backlight_power;
	bool wmi_backlight_native;
	bool wmi_backlight_set_devstate;
	bool wmi_force_als_set;
	int wapf;
	/*
	 * For machines with AMD graphic chips, it will send out WMI event
	 * and ACPI interrupt at the same time while hitting the hotkey.
	 * To simplify the problem, we just have to ignore the WMI event,
	 * and let the ACPI interrupt to send out the key event.
	 */
	int no_display_toggle;
	u32 xusb2pr;

	bool (*i8042_filter)(unsigned char data, unsigned char str,
			     struct serio *serio);
};

struct asus_wmi_driver {
	int			brightness;
	int			panel_power;
	int			wlan_ctrl_by_user;

	const char		*name;
	struct module		*owner;

	const char		*event_guid;

	const struct key_entry	*keymap;
	const char		*input_name;
	const char		*input_phys;
	struct quirk_entry	*quirks;
	/* Returns new code, value, and autorelease values in arguments.
	 * Return ASUS_WMI_KEY_IGNORE in code if event should be ignored. */
	void (*key_filter) (struct asus_wmi_driver *driver, int *code,
			    unsigned int *value, bool *autorelease);

	int (*probe) (struct platform_device *device);
	void (*detect_quirks) (struct asus_wmi_driver *driver);

	struct platform_driver	platform_driver;
	struct platform_device *platform_device;
};

#endif	/* __FAUSTUS_H */
