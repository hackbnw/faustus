# Faustus Project

Experimental unofficial Linux platform driver module for ASUS TUF Gaming series laptops. 

It is a backport of the asus-wmi / asus-nb-wmi drivers from the fornext branch + RGB backlight crudely cut-down to be useful for these laptops and packed as a DKMS module for 4.x / 5.x kernels.

If your machine does expose keyboard backlight as USB device (you see any devices with the ASUS vendor in output of `lsusb`) then this driver is not for you, check out [openauranb](https://github.com/MidhunSureshR/openauranb).

## Fair warning
**This is highly experimental and controls the ACPI / WMI responsible for dangerous low-level hardware features (for instance thermal management). So the possibility exists that you could erase data, lock up the system, disable thermal management and set your laptop on fire or worse. So use at your own risk if you know what you are doing.**

## Systems

|Model                   |BIOS        |OS                  |Kernel version     |
|-                       |-           |-                   |-                  |
|FX505GM                 |FX505GM.301 |Ubuntu 18.04.2 LTS  |4.18.0-25-generic  |
|FX505DD (not tested)    |?           |?                   |                   |
|FX505DY (not tested)    |?           |?                   |                   |

See "Contributing" section for other versions.

To check your exact model run 
```
$ sudo dmidecode | less
```
and scroll down to check BIOS Information / Version (2nd column) and Base Board Information / Product name (1st column).

## Features
* Additional Fn-X Hotkeys
* Keyboard backlight intensity
* Color and mode control for RGB keyboard backlight
* Fan boost mode switching

## Installation

How to: first disable old drivers, then proceed using make to test that it works at all, then install via DKMS permanently and enable on boot.

### Disable original modules

Create file /etc/modprobe.d/faustus.conf with following contents
```
blacklist asus_wmi
blacklist asus_nb_wmi
```

Also unload them before proceeding in case they miraculously managed to load.
```
sudo rmmod asus_nb_wmi
sudo rmmod asus_wmi
```

Some reports may suggest that you need to reboot after blacklisting, as they seem fail cleaning up on errors (do if you see AE_ALREADY_ACQUIRED in dmesg).

### Install build dependencies and DKMS
```
$ sudo apt-get install dkms
```

### Using make
Compile and load the driver temporarily
```
$ make
$ sudo modprobe sparse-keymap wmi video
$ sudo insmod src/faustus.ko
```
and check `dmesg | tail` to verify the driver is loaded and no errors are present.
```
[ 8295.475755] faustus: DMI checK: FX505GM
[ 8295.476475] faustus: Initialization: 0x1
[ 8295.477057] faustus: BIOS WMI version: 8.1
[ 8295.477680] faustus: SFUN value: 0x4a0061
[ 8295.477687] faustus faustus: Use DSTS
[ 8295.477691] faustus faustus: Enable event queue
[ 8295.490603] input: Asus WMI hotkeys as /devices/platform/faustus/input/input34
[ 8295.492695] faustus: Number of fans: 1
```

Check that everything works, the system is stable. Also try unloading the driver with 
```
$ sudo rmmod faustus
```
and inserting it back.

### Using DKMS
```
$ make dkms
```

The source code will probably be installed in `/usr/src/faustus-<version>/` and the module itself will be compiled and installed in the current kernel module directory `/lib/modules/...`. It should also be automatically rebuilt when the kernel is upgraded.

Next, try to load the module
```
$ sudo modprobe faustus
```

To uninstall the DKMS module execute
```
$ sudo make dkmsclean
```
or
```
$ sudo dkms remove faustus/<version> --all
```

NOTE: The DKMS install does work with secure boot on Ubuntu 18.04.

### Load on boot
On Ubuntu execute 
```
$ sudo make onboot
```
or add it to your config files in the other way. Revert with
```
$ sudo make noboot
```

This is OS dependent, check the internet on how to do it right.

## Usage

### Keyboard backlight intensity
Is exposed via ledclass device `/sys/class/leds/faustus::kbd_backlight` takes values 0 to 3. The driver changes brightness by itself when hotkeys are pressed.

### RGB backlight

TLDR: Run the `./set_rgb.sh` script as root.

NOTE: The interface will most definitely switch to LED subsystem when submitted to mainline. This here is sort of hack.

Driver exposes sysfs attributes in `/sys/devices/platform/faustus/kbbl/`. You have to write all the parameters and then write 1 to `kbbl_set` to write them permanently or 2 to write them temporarily (the settings will reset on restart or hibernation). 

The list of settings is:
* `kbbl_red` - red component in hex [00 - ff]
* `kbbl_green` - green component in hex [00 - ff]
* `kbbl_blue` - blue component in hex [00 - ff]
* `kbbl_mode` - mode: 
  - 0 - static color
  - 1 - breathing
  - 2 - color cycle (the color component parameters have no effect)
  - 3 - strobe (epileptic mode, speed parameter has no effect)
* `kbbl_speed` - speed for modes 1 and 2:
  - 0 - slow
  - 1 - medium
  - 2 - fast
* `kbbl_flags` - enable flags (must be ORed to get the value), use 2a or ff to set all
  - 02 - on boot (before module load) 
  - 08 - awake 
  - 20 - sleep 
  - 80? - should be logically shutdown, but I have genuinely no idea what it does

### Fan mode
Is controlled by default by the driver itself when `Fn-F5` is pressed switching three modes:
* 0 - normal
* 1 - overboost
* 2 - silent

It can also be read and written with `/sys/devices/platform/faustus/fan_mode`. The mode will not be preserved on reboot or hibernation.

## Contributing

If you own a machine of this series from the table above it would be much appreciated if you test the driver and write your feedback (successful and otherwise) in an issue on GitHub.

If you machine of this series is not in the list, likely it is supported too. You can check the DSDT yourself and try loading the driver without DMI verification by passing `let_it_burn=1`:
```
sudo insmod ./src/faustus.ko let_it_burn=1
```
and send feedback if it works.

### Information to include in feedback
Always OS / kernel version and
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
The patches are in fornext branch except for RGB backlight. This repository will provide usable DKMS version and will be maintained at least until it reaches stable Ubuntu version.

## Disclaimers
### Trademarks
ASUS Trademark is either a US registered trademark or trademark of ASUSTeK Computer Inc. in the United States and/or other countries. Reference to any ASUS products, services, processes, or other information and/or use of ASUS Trademarks does not constitute or imply endorsement, sponsorship, or recommendation thereof by ASUS.

All other trademarks are the property of their respective owners.

### Affiliation
Moreover, ASUS does not participate, authorize, approve, sponsor, support or is affiliated with this project in any way, neither this project with ASUS.

### Epilepsy warning
The driver can turn on blinking lights on the laptop that might cause seizures.