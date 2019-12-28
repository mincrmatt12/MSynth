# bootloader

The MSynth bootloader is designed to allow booting of:
	- one PRIMARY application (the synthesizer itself)
	- one TEST application (the "factory test" )
	- any number of SECONDARY applications

These applications are contained in SECTORS, which reside in contiguous flash sectors. Each sector contains a single application. The bootloader enumerates all sectors to find valid
applications.  When booting normally, the first (and only legal) PRIMARY application found is launched. If F1 and F2 are held while booting, the first (and only legal) TEST application is launched. If the
PRIMARY application cannot be found, but the TEST application can be, the TEST application is launched, otherwise the boot-menu appears. If all three mode buttons are held while booting, the boot-menu appears.

## Application Header

Each application has the following header, placed at the sector start:

| Offset | Format | Description |
| ---- | -------| --------- |
| 0x00 | `"MAPP"` | 4-byte magic |
| 0x04 | char[32] | Null terminated name |
| 0x24 | u32 | Vector table offset |
| 0x28 | u32 | Entry point |
| 0x2C | char[4] | APPID |
| 0x30 | u16 | Flags |
| 0x34 | u32 | Debug data pointer (0 if not present) |
| 0x38 | u32 | Initial stack pointer |
| 0x3C | u32 | Length of application data |

The flags are:

```
                            LSB
  
XXX 0 10 1 01100      1 10 10                
|// | || | |////      | |/ ||
|   | || | |          | |  |-is PRIMARY
|   | || | |          | |  --is TEST
|   | || | -FSuse slot| --EEPROM slot used
|   | || -uses FS     --uses EEPROM       
|   | |--uses LOWscreen RAM layout
|   | --is installed                         
|   -- supports APPCMDs                     
-- unused
```

The slot entries are added by the bootloader if isinstalled is not set, or during installation, if their enable bits are set. If the `LOWscreen` flag is set, the bootloader will assume the screen is setup as a
single-layer 8bpp located at the beginning of SRAM1 and SRAM2. This allows it to write messages over top of the application screen, which is useful in some cases. It assumes color 0 is black and color 1 is white.

## Memory Map

The first sector (and possibly second, depending on bootloader size) is reserved by the bootloader. No headers are present, there is effectively just a small application loaded into flash (i.e. an ISR table at flash
address 0)

The data in SRAM and CCMRAM are undefined, however in general it is split into 3 logical zones:
- SRAM1 & SRAM2 are VRAM (upper 512 bytes unused)
- SRAM3 is RAM
- CCMRAM is CCMRAM

The data in the upper 128 bytes of BKPRAM is the bootloader serialization area. When the bootloader starts, it reads from here and when it ends it writes to here.

The bootloader reserves the first 8 bytes of EEPROM.

## Bootloader Serialization Data

| Offset | Format | Description |
| ------ | ------------- | ----------- |
| 0x00 | `"MBLD"` | 4-byte magic |
| 0x04 | u8 | Init flags |
| 0x05 | u8 | Boot command |
| 0x08 | u32[8] | Boot command parameters |
| 0x28 | u8 | Boot mode |
| 0x2C | u32 | PTR to APPINFO |
| 0x30 | u8 | App Special Command |
| 0x34 | u32[8] | App command parameters |
| 0x54 | u32 | App command return value |

The init flags are set based on which parts of the data are valid. These flags are:

```
		LSB
XXXX 0000
|/// ||||
|    |||-- Boot command set
|    ||-- Boot mode/APPINFO set
|    |-- App special command set
|    -- App return pointer set
-- unused
```

For example, when an application boots, bit 1 should be set, and if the bootloader is calling a special routine in the ap, bit 2 will also be set.
If an application wants to trigger a command, it should write data to the correct offsets and set bit 0, then reset.

### Boot Mode

The boot mode indicates what events caused the app to start executing, an enumeration of:

| Value | Name |
| ----- | ---- |
| 0x00  | UNKNOWN_BOOT |
| 0x10  | NORMAL_BOOT_PRIMARY |
| 0x11  | NORMAL_BOOT_TESTBUTTON |
| 0x12  | NORMAL_BOOT_KEYCODE |
| 0x20  | OVERRIDE_BOOT_PRIMARY_FAIL |
| 0x21  | OVERRIDE_BOOT_MENU |
| 0x30  | UPDATE_OK_PRIMARY |
| 0x31  | UPDATE_FAILED_PRIMARY |
| 0x32  | UPDATE_OK_BOOTLOADER |
| 0x33  | UPDATE_FAILED_BOOTLOADER |
| 0x40  | SUB_LAUNCH_BOOT |
| 0x41  | SUB_LAUNCH_DONE | 

- Normal boot refers to the system booting using key combinations or normally.
- Override boot occurs when the first choice of the system was bypassed.
- Update boot occurs after an app self-updated
- Sub launch is when an app boots another app / returns from said app.

### Special Commands

The bootloader can call into your application as an API if the flag is set in the APPINFO.
The various commands are:

| Value | Name | Arguments |
| ----- | ---- | --------- |
| 0x00  | NORMAL_BOOT | none |
| 0x10  | UPDATE_FS_USE_FLAGS | none |
| 0x11  | RELOCATE_EEPROM_DATA | old sector, new sector; returns success |
| 0x20  | SUB_LAUNCH | params copied from boot command |

Normally, the value is 0, meaning a normal boot. When the filesystem is cleaned (during an update / removal) the UPDATE_FS_USE_FLAGS is called. This allows the application to mark all of the resources it uses.

## Boot State

When an app is launched, the MSP is first set to the location of 

