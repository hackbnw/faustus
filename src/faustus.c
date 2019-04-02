/*
 * faustus.c - Platform driver for ASUS TUF-series laptops
 * 
 * Copyright(C) 2019 Yurii Pavlovskyi <yurii.pavlovskyi@gmail.com>
 * 
 * Portions based on asus-wmi.c:
 * 	Copyright(C) 2010 Intel Corporation.
 * 	Copyright(C) 2010-2011 Corentin Chary <corentin.chary@gmail.com>
 *
 * 	Portions based on wistron_btns.c:
 * 	Copyright (C) 2005 Miloslav Trmac <mitr@volny.cz>
 * 	Copyright (C) 2005 Bernhard Rosenkraenzer <bero@arklinux.org>
 * 	Copyright (C) 2005 Dmitry Torokhov <dtor@mail.ru>
 *
 * Portions based on asus-nb-wmi.c:
 * 	Copyright(C) 2010 Corentin Chary <corentin.chary@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/wmi.h>
#include <linux/dmi.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>

MODULE_AUTHOR("Yurii Pavlovskyi");
MODULE_DESCRIPTION("Platform driver for ASUS TUF-series laptops");
MODULE_LICENSE("GPL");

#define INPUT_NAME "Faustus Hotkey Input"

#define PR KBUILD_MODNAME ": " // log prefix

#define WMI_EVENT_GUID 		"0B3CBB35-E3C2-45ED-91C2-4C5A6D195D1C"
#define WMI_EVENT_VALUE 	0xFF	// is always the same
#define WMI_EVENT_QUEUE_SIZE	0x10
#define WMI_EVENT_QUEUE_END	0x1

#define ATW_WMI_METHOD_GUID		"97845ED0-4E6D-11DE-8A39-0800200C9A66"

/* WMI Methods */
#define WMI_METHOD_DSTS		0x53545344
#define WMI_METHOD_DEVS		0x53564544 /* DEVice Set */
#define WMI_METHOD_INIT		0x54494E49 /* INITialize */

#define DEVID_KBBL		0x00050021
#define DEVID_FAN_MODE		0x00110018
#define DEVID_KB_RGB		0x00100056
#define DEVID_KB_RGB2		0x00100057

#define DSTS_PRESENCE_BIT	0x00010000

#define WED_KEY_KBBL_INC 	0xC4
#define WED_KEY_KBBL_DEC 	0xC5
#define WED_KEY_FAN_MODE 	0x99

#define ATW_KBBL_MAX		3
#define ATW_FAN_MODE_COUNT	3

// Parameters *****************************************************************

static bool debug;
module_param(debug, bool, 0);
MODULE_PARM_DESC(debug, "Enable debug logging");

static bool handle_fan = 1;
module_param(handle_fan, bool, 0);
MODULE_PARM_DESC(handle_fan, "Handle fan mode change key");

static bool handle_kbbl;
module_param(handle_kbbl, bool, 0);
MODULE_PARM_DESC(handle_kbbl, "Handle backlight change keys");

static bool let_it_burn;
module_param(let_it_burn, bool, 0);
MODULE_PARM_DESC(let_it_burn, "Disable DMI check, force load");

// Driver state ***************************************************************

struct atw_state {
	u8 kbbl_value;
	u8 fan_mode;

	u8 kbbl_red;
	u8 kbbl_green;
	u8 kbbl_blue;
	u8 kbbl_mode;
	u8 kbbl_speed;

	u8 kbbl_set_red;
	u8 kbbl_set_green;
	u8 kbbl_set_blue;
	u8 kbbl_set_mode;
	u8 kbbl_set_speed;
	u8 kbbl_set_flags;

	struct input_dev *input;
	struct led_classdev led_kbbl;
};

static struct atw_state drv_state = {
	.kbbl_value = ATW_KBBL_MAX,
	.fan_mode = 0,

	.kbbl_red = 0xFF,
	.kbbl_green = 0xFF,
	.kbbl_blue = 0xFF,
	.kbbl_mode = 0x00,
	.kbbl_speed = 0x00,

	.kbbl_set_red = 0xFF,
	.kbbl_set_green = 0xFF,
	.kbbl_set_blue = 0xFF,
	.kbbl_set_mode = 0x00,
	.kbbl_set_speed = 0x00,
	.kbbl_set_flags = 0xff
};

static struct platform_device *atw_platform_dev;

// WMI communication **********************************************************

static int atw_poll_wmi_event(void)
{
	struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *obj;
	acpi_status status;
	int code = -1;

	status = wmi_get_event_data(WMI_EVENT_VALUE, &output);
	if (ACPI_FAILURE(status)) {
		pr_warn(PR "Failed to poll WMI event code: %s\n",
				acpi_format_exception(status));
		return -EIO;
	}

	obj = (union acpi_object *)output.pointer;

	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		code = ((int)obj->integer.value) & 0xFF;
	}

	kfree(obj);
	return code;
}

// NOTE: DEVS RGB method takes 3 qword arguments
struct atw_wmi_input {
	u32 arg0;
	u32 arg1;
	u32 arg2;
} __packed;

static int atw_call_wmi_method(u32 method_id, u32 arg0, u32 arg1, u32 arg2,
		u32 *retval)
{
	struct atw_wmi_input args = {
			.arg0 = arg0,
			.arg1 = arg1,
			.arg2 = arg2,
	};

	struct acpi_buffer input = {(acpi_size)sizeof(args), &args};
	struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
	acpi_status status;
	union acpi_object *obj;
	int result = 0;

	status = wmi_evaluate_method(ATW_WMI_METHOD_GUID, 0, method_id,
			&input, &output);

	if (ACPI_FAILURE(status)) {
		pr_info(PR "Failed to call WMI method: %s\n",
				acpi_format_exception(status));
		return -EIO;
	}

	obj = (union acpi_object *)output.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		result = (u32)obj->integer.value;

	if (retval)
		*retval = result;

	kfree(obj);

	return 0;
}

// IO methods *****************************************************************

static int read_kb_backlight(void)
{
	int err;
	u32 retval;

	err = atw_call_wmi_method(WMI_METHOD_DSTS, DEVID_KBBL,
			0, 0, &retval);
	if (err) {
		pr_warn(PR "Failed to get KB backlight status: %d\n", err);
		return err;
	}

	if (!(retval & DSTS_PRESENCE_BIT)) {
		pr_warn(PR "No KB backlight present\n");
		return -ENODEV;
	}

	if (debug) {
		pr_info(PR "Keyboard backlight value: 0x%x\n", retval);
	}

	if (retval & 0x100) {
		drv_state.kbbl_value = retval & 0x03;
	} else {
		drv_state.kbbl_value = 0;
	}

	return 0;
}

static int write_kb_backlight(void)
{
	int err;
	u8 value;
	u32 retval;

	value = drv_state.kbbl_value & 0x3;
	if (drv_state.kbbl_value > 0) {
		value |= 0x80;
	}

	if (debug) {
		pr_info(PR "Set KB backlight level: %u\n",
				drv_state.kbbl_value);
	}

	err = atw_call_wmi_method(WMI_METHOD_DEVS, DEVID_KBBL,
			value, 0x00000000, &retval);
	if (err) {
		pr_warn(PR "Failed to set KB backlight: %d\n", err);
		return err;
	}

	if (err || retval != 1) {
		pr_warn(PR "Failed to set KB backlight (retval): %x\n",
				retval);
		return -EIO;
	}

	return 0;
}

static int check_fan_mode_devid(void)
{
	int err;
	u32 retval;

	err = atw_call_wmi_method(WMI_METHOD_DSTS, DEVID_FAN_MODE,
			0, 0, &retval);
	if (err) {
		pr_warn(PR "Failed to get fan mode status: %d\n", err);
		return err;
	}

	if (retval != 0x10003) {
		pr_warn(PR "Unexpected fan device status: %x\n", retval);
		return -ENODEV;
	}

	return 0;
}

static int write_fan_mode(void)
{
	int err;
	u8 value;
	u32 retval;

	value = drv_state.fan_mode & 0x3;

	pr_info(PR "Set fan mode: %u\n", value);

	err = atw_call_wmi_method(WMI_METHOD_DEVS, DEVID_FAN_MODE,
			value, 0x00000000, &retval);
	if (err || retval != 1) {
		pr_warn(PR "Failed to set fan mode: %d\n", err);
		return err;
	}

	if (retval != 1) {
		pr_warn(PR "Failed to set fan mode (retval): %x\n", retval);
		return -EIO;
	}

	return 0;
}

static int check_rgbkb_devids(void)
{
	int err;
	u32 retval;

	err = atw_call_wmi_method(WMI_METHOD_DSTS, DEVID_KB_RGB,
			0, 0, &retval);
	if (err) {
		pr_warn(PR "RGB keyboard device 1, read error: %d\n", err);
		return err;
	}

	if (!(retval & DSTS_PRESENCE_BIT)) {
		pr_warn(PR "RGB keyboard device 1, not present: %x\n", retval);
		return -ENODEV;
	}

	err = atw_call_wmi_method(WMI_METHOD_DSTS, DEVID_KB_RGB2,
				0, 0, &retval);
	if (err) {
		pr_warn(PR "RGB keyboard device 2, read error: %d\n", err);
		return err;
	}

	if (!(retval & DSTS_PRESENCE_BIT)) {
		pr_warn(PR "RGB keyboard device 2, not present: %x\n", retval);
		return -ENODEV;
	}

	return 0;
}

static int write_kb_rgb(int persistent)
{
	int err;
	u32 retval;
	u8 speed_byte;
	u8 mode_byte;
	u8 speed;
	u8 mode;

	speed = drv_state.kbbl_set_speed;
	switch(speed) {
	case 0:
	default:
		speed_byte = 0xe1; // slow
		speed = 0;
		break;
	case 1:
		speed_byte = 0xeb; // medium
		break;
	case 2:
		speed_byte = 0xf5; // fast
		break;
	}

	mode = drv_state.kbbl_set_mode;
	switch(mode) {
	case 0:
	default:
		mode_byte = 0x00; // static color
		mode = 0;
		break;
	case 1:
		mode_byte = 0x01; // blink
		break;
	case 2:
		mode_byte = 0x02; // rainbow
		break;
	case 3:
		mode_byte = 0x0a; // strobe
		break;
	}

	err = atw_call_wmi_method(WMI_METHOD_DEVS, DEVID_KB_RGB,
		(persistent ? 0xb4 : 0xb3) |
		(mode_byte << 8) |
		(drv_state.kbbl_set_red << 16) |
		(drv_state.kbbl_set_green << 24),
		(drv_state.kbbl_set_blue) |
		(speed_byte << 8), &retval);
	if (err) {
		pr_warn(PR "RGB keyboard device 1, write error: %d\n", err);
		return err;
	}

	if (retval != 1) {
		pr_warn(PR "RGB keyboard device 1, write error (retval): %x\n",
				retval);
		return -EIO;
	}

	err = atw_call_wmi_method(WMI_METHOD_DEVS, DEVID_KB_RGB2,
		(0xbd) |
		(drv_state.kbbl_set_flags << 16) |
		(persistent ? 0x0100 : 0x0000), 0, &retval);
	if (err) {
		pr_warn(PR "RGB keyboard device 2, write error: %d\n", err);
		return err;
	}

	if (retval != 1) {
		pr_warn(PR "RGB keyboard device 2, write error (retval): %x\n",
				retval);
		return -EIO;
	}

	drv_state.kbbl_red = drv_state.kbbl_set_red;
	drv_state.kbbl_green = drv_state.kbbl_set_green;
	drv_state.kbbl_blue = drv_state.kbbl_set_blue;
	drv_state.kbbl_mode = mode;
	drv_state.kbbl_speed = speed;

	return 0;
}

// Sysfs **********************************************************************

static ssize_t show_u8(u8 value, char* buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", value);
}

static ssize_t store_u8(u8 *value, const char* buf, int count)
{
	int err;
	u8 result;

	err = kstrtou8(buf, 16, &result);
	if (err < 0) {
		pr_warn(PR "Trying to store invalid value\n");
		return err;
	}

	*value = result;

	return count;
}

static ssize_t show_kbbl_red(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return show_u8(drv_state.kbbl_red, buf);
}

static ssize_t store_kbbl_red(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_red, buf, count);
}

static ssize_t show_kbbl_green(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_u8(drv_state.kbbl_green, buf);
}

static ssize_t store_kbbl_green(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_green, buf, count);
}

static ssize_t show_kbbl_blue(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_u8(drv_state.kbbl_blue, buf);
}

static ssize_t store_kbbl_blue(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_blue, buf, count);
}

static ssize_t show_kbbl_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_u8(drv_state.kbbl_mode, buf);
}

static ssize_t store_kbbl_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_mode, buf, count);
}

static ssize_t show_kbbl_speed(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_u8(drv_state.kbbl_speed, buf);
}

static ssize_t store_kbbl_speed(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_speed, buf, count);
}

static ssize_t show_kbbl_flags(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_u8(drv_state.kbbl_set_flags, buf);
}

static ssize_t store_kbbl_flags(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return store_u8(&drv_state.kbbl_set_flags, buf, count);
}

static ssize_t show_kbbl_set(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
			"Write to configure RGB keyboard backlight\n");
}

static ssize_t store_kbbl_set(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u8 value;

	int result = store_u8(&value, buf, count);
	if (result < 0) {
		return result;
	}

	if (value == 1) {
		write_kb_rgb(1);
	} else if (value == 2) {
		write_kb_rgb(0);
	}

	return count;
}

// RGB values: 00 .. ff
static DEVICE_ATTR(kbbl_red, S_IRUGO | S_IWUSR,
		show_kbbl_red, store_kbbl_red);
static DEVICE_ATTR(kbbl_green, S_IRUGO | S_IWUSR,
		show_kbbl_green, store_kbbl_green);
static DEVICE_ATTR(kbbl_blue, S_IRUGO | S_IWUSR,
		show_kbbl_blue, store_kbbl_blue);

// Color modes: 0 - static color, 1 - blink, 2 - rainbow, 3 - strobe
static DEVICE_ATTR(kbbl_mode, S_IRUGO | S_IWUSR,
		show_kbbl_mode, store_kbbl_mode);

// Speed for modes 1 and 2: 0 - slow, 1 - medium, 2 - fast
static DEVICE_ATTR(kbbl_speed, S_IRUGO | S_IWUSR,
		show_kbbl_speed, store_kbbl_speed);

// Enable: 02 - on boot (until module load) | 08 - awake | 20 - sleep 
// (2a or ff to enable everything)
static DEVICE_ATTR(kbbl_flags, S_IRUGO | S_IWUSR,
		show_kbbl_flags, store_kbbl_flags);
// TODO: Logically 80 would be shutdown, but no visible effects of this option 
// were observed so far

// Config mode: 1 - permanently, 2 - temporarily (reset after reboot)
static DEVICE_ATTR(kbbl_set, S_IRUGO | S_IWUSR,
		show_kbbl_set, store_kbbl_set);


static ssize_t show_fan_mode(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return show_u8(drv_state.fan_mode, buf);
}

static ssize_t store_fan_mode(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int result;
	u8 new_mode;

	result = store_u8(&new_mode, buf, count);
	if (result < 0) {
		return result;
	}

	drv_state.fan_mode = new_mode % ATW_FAN_MODE_COUNT;
	write_fan_mode();

	return result;
}

// Fan mode: 0 - normal, 1 - turbo, 2 - quiet?
static DEVICE_ATTR(fan_mode, S_IRUGO | S_IWUSR, show_fan_mode, store_fan_mode);

static struct attribute *atw_sysfs_attrs[] = {
	&dev_attr_kbbl_red.attr,
	&dev_attr_kbbl_green.attr,
	&dev_attr_kbbl_blue.attr,
	&dev_attr_kbbl_mode.attr,
	&dev_attr_kbbl_speed.attr,
	&dev_attr_kbbl_flags.attr,
	&dev_attr_kbbl_set.attr,
	&dev_attr_fan_mode.attr,
	NULL,
};

static const struct attribute_group atw_sysfs_group = {
	.attrs = atw_sysfs_attrs
};

// LEDs ***********************************************************************

static int set_kbbl_brightness(struct led_classdev *led_cdev,
		       enum led_brightness brightness)
{
	if (brightness >= 0 && brightness <= ATW_KBBL_MAX) {
		if (led_cdev->flags & LED_UNREGISTERING) {
			return 0; // prevent disabling on module unregister
		}

		drv_state.kbbl_value = brightness;
		write_kb_backlight();
		return 0;
	}

	return -EINVAL;
}

static enum led_brightness get_kbbl_brightness(struct led_classdev *led_cdev)
{
	return drv_state.kbbl_value;
}

static int atw_led_register(void) {
	int err;
	struct led_classdev *led_dev;

	led_dev = &drv_state.led_kbbl;

	led_dev->max_brightness = ATW_KBBL_MAX;
	led_dev->flags = LED_BRIGHT_HW_CHANGED;

	// NOTE: The name must end with the kbd_backlight in order for the LED
	// to be detected by UPower daemon
	led_dev->name = KBUILD_MODNAME "::kbd_backlight";

	led_dev->brightness_set_blocking = set_kbbl_brightness;
	led_dev->brightness_get = get_kbbl_brightness;

	err = led_classdev_register(&atw_platform_dev->dev, led_dev);
	if (err < 0) {
		goto fail_register;
	}

	return 0;

fail_register:
	return err;
}

static void atw_led_unregister(void)
{
	led_classdev_unregister(&drv_state.led_kbbl);
}

// Hotkey mapping *************************************************************

static const struct key_entry atw_keymap[] = {
	{KE_KEY, 0x33, {KEY_DISPLAYTOGGLE}},
	{KE_IGNORE, 0x57, }, /* Battery mode */
	{KE_IGNORE, 0x58, }, /* AC mode */
	{KE_KEY, 0x6B, {KEY_TOUCHPAD_TOGGLE}},
	{KE_KEY, 0x7c, {KEY_MICMUTE}},
	{KE_KEY, 0xB5, {KEY_CALC}},
	{KE_KEY, WED_KEY_FAN_MODE, {KEY_FN_F5}},
	{KE_KEY, WED_KEY_KBBL_INC, {KEY_KBDILLUMUP}},
	{KE_KEY, WED_KEY_KBBL_DEC, {KEY_KBDILLUMDOWN}},

	// TODO: Check if those ever come
	// { KE_IGNORE, 0x6E, },  /* Low Battery notification */
	// { KE_KEY, 0x88, { KEY_RFKILL  } }, /* Radio Toggle Key */
	{KE_END, 0},
};

static int atw_input_register(void)
{
	int err;

	drv_state.input = input_allocate_device();
	if (!drv_state.input)
		return -ENOMEM;

	drv_state.input->name = INPUT_NAME;
	drv_state.input->phys = KBUILD_MODNAME "/input0";
	drv_state.input->id.bustype = BUS_HOST;
	drv_state.input->dev.parent = &atw_platform_dev->dev;

	err = sparse_keymap_setup(drv_state.input, atw_keymap, NULL);
	if (err)
		goto fail_sparse_keymap;

	err = input_register_device(drv_state.input);
	if (err)
		goto fail_register_dev;

	return 0;

fail_register_dev:
fail_sparse_keymap:
	input_free_device(drv_state.input);
	return err;
}

static void atw_input_unregister(void)
{
	if (drv_state.input)
		input_unregister_device(drv_state.input);

	drv_state.input = NULL;
}

static int atw_flush_event_queue(void)
{
	int i;
	int code;

	for (i=0; i<WMI_EVENT_QUEUE_SIZE+1; i++) {
		code = atw_poll_wmi_event();
		if (code < 0) {
			return -EIO;
		} else if (code == WMI_EVENT_QUEUE_END) {
			return 0;
		}
	}

	return -EIO;
}

static void atw_handle_key(int code) {
	if (code == WED_KEY_KBBL_INC && handle_kbbl) {
		if (drv_state.kbbl_value < ATW_KBBL_MAX) {
			drv_state.kbbl_value ++;
			led_classdev_notify_brightness_hw_changed(
					&drv_state.led_kbbl,
					drv_state.kbbl_value);
		}
		write_kb_backlight();
	} else if (code == WED_KEY_KBBL_DEC && handle_kbbl) {
		if (drv_state.kbbl_value > 0) {
			drv_state.kbbl_value --;
			led_classdev_notify_brightness_hw_changed(
					&drv_state.led_kbbl,
					drv_state.kbbl_value);
		}
		write_kb_backlight();
	} else if (code == WED_KEY_FAN_MODE && handle_fan) {
		drv_state.fan_mode =
				(drv_state.fan_mode + 1) % ATW_FAN_MODE_COUNT;
		write_fan_mode();
	} else {
		if (!sparse_keymap_report_event(drv_state.input, code, 1, 1))
			pr_info(PR "Unrecognized event code: 0x%x\n", code);
	}
}

static void atw_wmi_notify(u32 value, void *context)
{
	int i;
	int code;

	if (value != WMI_EVENT_VALUE) {
		pr_info(PR "Unexpected event value: 0x%x\n", value);
		return;
	}

	for (i=0; i<WMI_EVENT_QUEUE_SIZE; i++) {
		code = atw_poll_wmi_event();
		if (code < 0) {
			pr_warn(PR "Failed to get event code: %d\n", code);
			break;
		} else if (code == WMI_EVENT_QUEUE_END) {
			break;
		} else {
			atw_handle_key(code);
		}
	}
}

// Platform driver ************************************************************

static int atw_driver_probe(struct platform_device *device)
{
	acpi_status acpi_err;
	int err;
	u32 retval;

	err = atw_input_register();
	if (err) {
		pr_err(PR "Failed to create input device: %d\n", err);
		goto fail_input;
	}

	err = atw_led_register();
	if (err) {
		pr_err(PR "Failed to register backlight LED device: %d\n", err);
		goto fail_led;
	}

	err = sysfs_create_group(&atw_platform_dev->dev.kobj, &atw_sysfs_group);
	if (err) {
		pr_err(PR "Error configuring sysfs: %d\n", err);
		goto fail_sysfs;
	}

	// NOTE: Init can not be reverted
	err = atw_call_wmi_method(WMI_METHOD_INIT, 0, 0, 0, &retval);
	if (err) {
		pr_err(PR "Failed to init: %d:%x\n", err, retval);
		goto fail_init;
	}

	acpi_err = wmi_install_notify_handler(WMI_EVENT_GUID,
					atw_wmi_notify, NULL);
	if (ACPI_FAILURE(acpi_err)) {
		pr_err(PR "Failed to install WMI notify handler: %s\n",
				acpi_format_exception(acpi_err));
		err = -EIO;
		goto fail_notify;
	}

	err = atw_flush_event_queue();
	if (err) {
		pr_err(PR "Failed to flush the WMI event queue: %d\n", err);
		goto fail_flush;
	}

	err = read_kb_backlight();
	if (err) {
		pr_err(PR "Failed check keyboard backlight: %d\n", err);
		goto fail_kbbl;
	}

	err = check_fan_mode_devid();
	if (err) {
		pr_err(PR "Failed check fan mode: %d\n", err);
		goto fail_fbm;
	}

	err = check_rgbkb_devids();
	if (err) {
		pr_err(PR "Failed check RGB keyboard: %d\n", err);
		goto fail_kbrgb;
	}

	pr_info(PR "Driver ready\n");

	return 0;

fail_kbrgb:
fail_fbm:
fail_kbbl:
fail_flush:
	wmi_remove_notify_handler(WMI_EVENT_GUID);
fail_notify:
fail_init:
	sysfs_remove_group(&atw_platform_dev->dev.kobj, &atw_sysfs_group);
fail_sysfs:
	atw_led_unregister();
fail_led:
	atw_input_unregister();
fail_input:
	return err;
}

static int atw_driver_remove(struct platform_device *device)
{
	atw_input_unregister();
	atw_led_unregister();
	sysfs_remove_group(&atw_platform_dev->dev.kobj, &atw_sysfs_group);
	wmi_remove_notify_handler(WMI_EVENT_GUID);

	pr_info(PR "Driver unload\n");
	return 0;
}

static struct platform_driver atw_platform_driver = {
	.probe = atw_driver_probe,
	.remove = atw_driver_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE
	}
};

// Probing ********************************************************************

static int __init dmi_check_callback(const struct dmi_system_id *id)
{
	pr_info(PR "DMI checK: %s\n", id->ident);
	return 1;
}

static const struct dmi_system_id atw_dmi_list[] __initconst = {
	{
		.callback = dmi_check_callback,
		.ident = "FX505GM",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "FX505GM"),
		},
	},
	{ }
};

static int __init atw_init(void)
{
	int status;

	if (!let_it_burn) {
		if (!dmi_check_system(atw_dmi_list)) {
			return -ENODEV;
		}
	} else {
		pr_info(PR "Omitting DMI verification\n");
	}

	if (!wmi_has_guid(ATW_WMI_METHOD_GUID)) {
		if (debug) {
			pr_info(PR "Method WMI GUID not found\n");
		}
		return -ENODEV;
	}

	if (!wmi_has_guid(WMI_EVENT_GUID)) {
		if (debug) {
			pr_info(PR "Event WMI GUID not found\n");
		}
		return -ENODEV;
	}


	atw_platform_dev = platform_device_register_simple(
			KBUILD_MODNAME, -1, NULL, 0);
	if (IS_ERR(atw_platform_dev)) {
		return PTR_ERR(atw_platform_dev);
	}

	status = platform_driver_probe(&atw_platform_driver, atw_driver_probe);
	if (status) {
		pr_err(PR "Can't probe platform driver: %d\n", status);
		platform_device_unregister(atw_platform_dev);
		return status;
	}

	return 0;
}

static void __exit atw_cleanup(void)
{
	platform_driver_unregister(&atw_platform_driver);
	platform_device_unregister(atw_platform_dev);
}
 
module_init(atw_init);
module_exit(atw_cleanup);
