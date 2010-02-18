Custom IOS module & tester for Wii   by neimod
----------------------------------------------

This package contains the necessary basic tools and project files needed to create a custom IOS module and interact with it. This is perfect for running test code on the Wii's IOS system.

Sourcecode is delivered for all tools and projects.

For compiling the ARM IOS code, devkitARM (arm-elf or arm-eabi) is needed.
For compiling the PPC Wii code, devkitPPC and libOGC are needed.

Package contents:

- stripios: This is a tool that will take a tradional ARM ELF file and converts it into a format the IOS system understands.


- iosmodule: This is the project which will create a custom IOS module.

In order to create a working IOS module, you must supply the fixed absolute memory addresses for the module to live in, in the linker script.
Currently, it defaults to the memory addresses set by the DIP module from IOS31.

To install a custom IOS module, you need to add it to the TMD of an existing IOS, and allocate it with an unused content id, or replace an existing one.

Currently, the produced IOS module is tested by replacing the DIP module from IOS 31. This is done by letting patchmii install IOS 31 as IOS 254, and replacing the contents of the DIP module with the custom IOS module.

The example main source file sets up a device called "/dev/haxx" and allows you to read/write to the IOS memory space (currently limited to the module's memory space because it is running from usermode), and allows you to upload and execute custom code.

- iostester: This is the project which will interact with the custom IOS module.

In the first step, a completely position independent ARM program is compiled, and the file arm.bin is produced.

The ARM program is made position indepdenent by means of an global offset table (GOT), which the ARM program will automatically correct upon boot up.

The arm.bin file is automatically added into the PPC program, so that it can upload it to the custom IOS module, and let it execute it from the IOS side.



Happy hacking!
