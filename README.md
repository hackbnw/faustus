# Faustus Project

Experimental unofficial Linux platform driver module for ASUS TUF Gaming series laptops which I wrote for mine. The basic support in current kernel versions is very good out of the box. This driver adds some of the fancier features.

If your machine does expose keyboard backlight as USB device (you see any devices with the ASUS vendor in output of `lsusb`) then this driver is not for you, check out [openauranb](https://github.com/MidhunSureshR/openauranb).

## Fair warning
**The driver is highly experimental and the ACPI / WMI controls the dangerous low-level hardware features (for instance thermal management). So the possibility exists (especially when ignoring the DMI verification) that using it could potentially erase data, lock up the system, disable thermal management and set your laptop on fire or worse. So use at your own risk if you know what you are doing and compare source code with the DSDT beforehand if the model is not in the list.**

## Systems

|Model      |BIOS        |OS                  |Kernel version     |Reports        |
|-          |-           |-                   |-                  |-              |
|FX505GM    |FX505GM.301 |Ubuntu 18.04.2 LTS  |4.18.0-16-generic  |1              |

To check your exact model run 
```
$ sudo dmidecode | less
```
and scroll down to check BIOS Information / Version (2nd column) and Base Board Information / Product name (1st column).

Not all machines of this series have RGB keyboard backlight. The ones without are not yet supported at all (see Contributing section to change that).

## Features
* Additional Fn-X Hotkeys
* Keyboard backlight intensity
* Color and mode control for RGB keyboard backlight
* Fan boost mode switching

## Installation

### Using DKMS
```
$ sudo apt-get install dkms
$ make dkms
```

The source code will probably be installed in `/usr/src/faustus-<version>/` and the module itself will be compiled and installed in the current kernel module directory `/lib/modules/...`. It should also be automatically rebuilt when the kernel is upgraded.

Next, try to load the module
```
$ sudo modprobe faustus
```
and check `dmesg | tail` to verify the driver is loaded.
```
[   89.001787] faustus: DMI checK: Asus FX505GM
[   89.001861] input: Faustus Hotkey Input as /devices/platform/faustus/input/input27
[   89.005816] faustus: Driver ready
```
Check that everything works, the system is stable. Also try unloading the driver with 
```
$ sudo rmmod faustus
```
and inserting it back.

To uninstall the DKMS module execute
```
$ sudo make dkmsclean
```
or
```
$ sudo dkms remove faustus/<version> --all
```

The DKMS install does work with secure boot on Ubuntu 18.04.

### Using make
Alternatively you can load the driver temporarily.
```
$ make
$ sudo insmod src/faustus.ko
```

to unload:
```
$ sudo rmmod faustus
```

### Load on boot
Execute 
```
$ sudo make onboot
```
or add it to your config files in the other way. Revert with
```
$ sudo make noboot
```

This is OS dependent, check the manual how to do it right.

## Usage

### Hotkeys
Some of the hotkeys are HID events and work out of the box, but some are WMI events. The driver exposes additional input device. I did not manage to actually use these additional hotkeys in GNOME at the time, but still they are there. To check that it's working install `evtest` and run `sudo evtest`. Find the input device of the driver, enter the number and try pressing:

* Mic off
* Display toggle
* Touchpad toggle
* Calc

### Keyboard backlight intensity
Is exposed via ledclass device `/sys/class/leds/faustus::kbd_backlight` takes values 0 to 3. By default the hotkeys are forwarded via input device. The UPower daemon will recognize the ledclass device as keyboard backlight by the suffix automatically. The GNOME should then properly change the backlight and show OSD.

If parameter `handle_kbbl=1` is provided the driver will change brightness himself instead of forwarding hotkeys (useful for terminal-only mode).

### RGB backlight

TLDR: Run the `./set_rgb.sh` script as root.

Driver exposes sysfs attributes in `/sys/devices/platform/faustus/`. You have to write all the parameters and then write 1 to `kbbl_set` to write them permanently or 2 to write them temporary (the settings will reset on restart or hibernation). 

The list of settings is:
* `kbbl_red` - red component in hex [00 - ff]
* `kbbl_green` - green component in hex [00 - ff]
* `kbbl_blue` - blue component in hex [00 - ff]
* `kbbl_mode` - mode: 
  - 0 - static color
  - 1 - breathing
  - 2 - rainbow (the color component parameters have no effect)
  - 3 - strobe (epileptic mode, speed parameter has no effect)
* `kbbl_speed` - speed for modes 1 and 2:
  - 0 - slow
  - 1 - medium
  - 2 - fast
* `kbbl_flags` - enable flags (must be ORed to get the value), use 2a or ff to set all
  - 02 - on boot (before module load) 
  - 08 - awake 
  - 20 - sleep 
  - 80? - should be logically shutdown, but I have genuinely no idea what it does neither I've observed any effect (I did wipe my laptop 2 hours after unpacking, so if you can tell me what it does, it would be much appreciated)
  - Other ones? The parameter is passed directly to WMI.

### Fan mode
Is controlled by default by the driver itself when `Fn-F5` is pressed switching three modes (no vendor documentation or even hint found):
* 0 - normal
* 1 - turbo or something
* 2 - quiet? or whatever

If you pass the parameter `handle_fan=0` then instead of switching the modes by itself, the driver will pass key event `KEY_FN_F5` to the userland. Then the mode switching must be handled by some user program (none found at the time).

It can also be read and written with `/sys/devices/platform/faustus/fan_mode`. The mode will not be preserved on reboot or hibernation.

## Contributing

If you own a machine of this series from the table above it would be much appreciated if you test the driver and write your feedback (successful and otherwise) in an issue on GitHub.

If you machine of this series is not in the list but it also has all the listed features, then likely it is supported. You can check the DSDT yourself and try to load the driver without DMI verification by passing `let_it_burn=1` (POTENTIALLY DANGEROUS, see Fair warning) or drop me your DSDT and DMI information.

If your system does not have RGB keyboard backlight then likely this driver will not even load so please drop me the DSDT if you want to have the support.

### Information to include in feedback
Always
```
$ sudo dmidecode | grep "BIOS Inf\|Board Inf" -A 3 
BIOS Information
	Vendor: American Megatrends Inc.
	Version: FX505GM.301
	Release Date: 09/21/2018
--
Base Board Information
	Manufacturer: ASUSTeK COMPUTER INC.
	Product Name: FX505GM
	Version: 1.0
```

and additionally for a new model
```
$ sudo cat /sys/firmware/acpi/tables/DSDT > dsdt.aml
```

## Roadmap
The ultimate goal would be to evaluate whether and how to, and then integrate the driver into mainline Linux kernel. This repository will provide the DKMS version of the module and will be maintained at least until it reaches stable Ubuntu version. The name of the driver in mainline will be changed.

## Disclaimers
### Trademarks
ASUS Trademark is either a US registered trademark or trademark of ASUSTeK Computer Inc. in the United States and/or other countries. Reference to any ASUS products, services, processes, or other information and/or use of ASUS Trademarks does not constitute or imply endorsement, sponsorship, or recommendation thereof by ASUS.

All other trademarks are the property of their respective owners.

### Affiliation
Moreover, ASUS does not participate, authorize, approve, sponsor, support or is affiliated with this project in any way, neither this project with ASUS.

### Epilepsy warning
The driver can turn on blinking lights on the laptop that might cause seizures.