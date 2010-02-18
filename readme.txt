
'''cIOS usb2''' beta3
by kwiirk

==Description==

This program is a modification of custom ios module by neimod, implementing a usb2 (ehci) driver on
the side of legacy [[:/dev/usb/oh0|OH0]] and [[:/dev/usb/oh1|OH1]] modules.

Basically, it enables USB 2.0 devices to run at full USB 2.0 speeds. Before, it would only allow USB 1.1 speeds

The sources included also contain various tools to help the ioshacker. Use with care..
A modified copy of libfat shows developers a simple way to use full USB speed for their applications, while keeping usb1 working
if cios_usb2 is not installed

=== Disclaimer ===
This software is in beta stage, it is not heavily tested, but proved to work on the author system.
Physical damages to your hardrive is possible due to very immature state of the code.
Damages to hard-drive data are very possible.
See the NO WARANTY part of the GPL license.

Please dont use this software for illegal purpose.

=== beta3 changelog ===
*improved the reliability of the usbstorage driver. It now resets the drive if it starts to error.
*implemented libwbfs, a filesystem to store wii backups on an harddrive (see readme in sources for details)
*implemented yal, the most simple usb-loader.
*Two versions are available. 
**cios_usb2_install is IOS 202, and does not contain the DIP plugin nor yal.
**cios_usb2_dip_install is IOS 222, and is not distributed on wiibrew.org. Please do not link to it on wiibrew.

=== beta2 changelog ===
*added a ehc custom module which implements standard ios usb API over /dev/usb/ehc, plus custom
usbstorage direct api.
*restored the legacy oh0 module from nintendo. External USB1.1 devices will be taken care of by
this module.
*changed the order of module loading in order to be sure that EHCI initialization code runs before
OH1, now bluetooth should work more reliabily.
*added libfat w/ read_ahead and USBspeedTest, courtesy of rodries from mplayer-ce team.
=== beta1 changelog ===
*added libcios, to avoid (source) code duplication between modules and iostester
*corrected some bugs in the original code (stack address pointed to the begginning of actual memory allocation, but needed to be at the end.)
*added tinyehci, a minimal generic ehci driver based on linux ehci-hcd driver.
*added oh0module, which implements the oh0 ios messages with tinyehci, and new usbstorage messages for even better speed.
*added patchmii installer

==Explaination==
[http://en.wikipedia.org/wiki/Host_controller_interface|background usb-hcd knowledge]
The wii's main CPU uses a high level interface to access usb devices. This API doesn't know about
USB2.0 or USB1.1, it is just a matter of sending control messages, and bulk messages.

The original wii IOS implements this interface with a OHCI driver in the OH0 module. That is why the unmodified Wii is limited to USB1.1.

The weird thing about IOS is they actually have 2 OHCI drivers, to deal with the each of the 2 OHCI HW modules.
OH0 deals with the one that is connected to external ports, and OH1 deals with the internal ports's one (which is then connected to the bluetooth controller).

These two OH modules seems to duplicate most of their code. The difference might be in their optimization level. 
OH1 also does not support hotplug nor hubs.

The idea is that we can add another module that stays in some freespace of IOS, and implements our ehci
driver. This module has to be initialized before OH0, and OH1, so that it can release the USB ports
that are not USB2, or that we want to be managed by legacy code (bluetooth dongle).

Original beta1 hack, implemented ehci module in the place of oh0 module, that meant external usb1
was disabled, but old libogc code will work out of the box, the homebrew code though it talk with
the usb1 bign driver but actually talked with usb2 homebrew code. 

Now, with beta2 and following, I figured out how to add modules in the tmd, and how I can find some free ram space to put them (see sources).
Original oh0 has been put back, so that homebrew code has to know
there is a new /dev/usb/ehc interface it can talk to with the usual API.

==Driver limitation==

*The lack of a real usb stack means the driver lacks usb2 hub support. The USB2 hubs "TT" mechanism is a bit complicated, and would need a lot more work.
*The driver doesn't implement the scheduled pipes, ie interrupt and isochronous pipes, which would be
needed for highend webcams (I cannot think of other usb2 devices type that uses such pipes.)
*The driver does not implement hotplug detection. Plug your usb2 devices before launching an
application using ios 202, and it will be recognised as usb2, else, it will be recognised by usb1 driver.

==Usage==

*install the cios_usb2 with cios_usb2_install.dol
*Plug your usb key into wii external port (for HW modders, this should actually also work with the unused internal port if the pins are present on the PCB..)
**test it with raw_speed_test.elf or fat_speed_test.dol
**or launch a usb2 aware application
*If your usb2 device is connected before application launch, then it will be recognised as
usb2. Else, it will be recognised as usb1.
*if you connect a usb1 device at anytime, it will be recognised as usb1 by OH0 module.

==Programmers usage==
the cios_usb2 is compatible with libogc, you just have to do:
 IOS_ReloadIOS(202);
at the very begginning of a program to use this ios.

 usbstorage_starlet.c
is present in a modified libfat, which allows libfat to directly use cios_usb2 mass storage API for better
performance (3 times less ios ioctls). This modified libfat adds in disc.c a usb2:/ devices which allows to access usb2 discs, and keeps usb:/ for legacy usb access,
in case cios_usb2 is not installed.


==License==
This program is free software. You are allowed and encouraged to use/change it for your homebrew hacks, even if you only release it when done, as soon as you repect the GPL. ;-)

==Linux==
to use it on linux, you must recompile the kernel after modifying the file
 starlet_es.c:
 #define STARLET_ES_IOS_MAX 202
The linux driver is a nice hack that uses the IOS OH0 API to implement a hcd driver. This breaks the
software layers rules but this works. As cios_usb2 implements the exact same API (except for
possible implementation bugs) linux driver should work just by telling him there is a new usb device
tree available:
<pre>
diff --git a/rvl-sthcd.c b/rvl-sthcd.c
index 4c117e8..cc4e821 100644
--- a/rvl-sthcd.c
+++ b/rvl-sthcd.c
@@ -160,7 +160,7 @@ struct sthcd_oh {
 struct sthcd_hcd {
        spinlock_t lock;
 
-       struct sthcd_oh oh[2];
+       struct sthcd_oh oh[3];
 
        struct sthcd_port *ports;       /* array of ports */
        unsigned int nr_ports;
@@ -1257,8 +1257,11 @@ static int sthcd_udev_open(struct sthcd_udev *udev)
                sthcd_udev_close(udev);
        }
 
-       snprintf(pathname, sizeof(pathname), "/dev/usb/oh%u/%04x/%04x",
-                oh->index, udev->idVendor, udev->idProduct);
+       if(oh->index==2)
+               snprintf(pathname, sizeof(pathname), "/dev/usb/ehc/%04x/%04x", , udev->idVendor, udev->idProduct);
+       else
+               snprintf(pathname, sizeof(pathname), "/dev/usb/oh%u/%04x/%04x",
+                       oh->index, udev->idVendor, udev->idProduct);
        error = starlet_open(pathname, 0);
        if (error < 0) {
                drv_printk(KERN_ERR, "open %s failed\n", pathname);
@@ -1980,10 +1983,12 @@ static int sthcd_oh_init(struct sthcd_oh *oh, unsigned int index,
        char pathname[16];
        int error;
 
-       if (index != 0 && index != 1)
+       if (index != 0 && index != 1 && index != 2)
                return -EINVAL;
-
-       snprintf(pathname, sizeof(pathname), "/dev/usb/oh%u", index);
+       if(index==2)
+               snprintf(pathname, sizeof(pathname), "/dev/usb/ehc");
+       else
+               snprintf(pathname, sizeof(pathname), "/dev/usb/oh%u", index);
        error = starlet_open(pathname, 0);
        if (error < 0)
                return error;
@@ -2025,8 +2030,9 @@ static int sthcd_rescan_thread(void *arg)
         * We may need to rescan oh1 if bluetooth dongle disconnects.
         */
 
-       /* oh1 has non-removable devices only, so just scan it once */
+       /* oh1 and ehc has non-removable devices only, so just scan it once */
        sthcd_oh_rescan(&sthcd->oh[1]);
+       sthcd_oh_rescan(&sthcd->oh[2]);
 
        oh = &sthcd->oh[0];
 
@@ -2064,6 +2070,12 @@ static int sthcd_start(struct usb_hcd *hcd)
 
        hcd->uses_new_polling = 1;
 
+       /* ehc is the external usb2 bus */
+       error = sthcd_oh_init(&sthcd->oh[2], 2, sthcd, STHCD_MAX_DEVIDS);
+       if (error < 0) {
+               DBG("%s: error=%d (%x)\n", __func__, error, error);
+               return error;
+       }
        /* oh0 is the external bus */
        error = sthcd_oh_init(&sthcd->oh[0], 0, sthcd, STHCD_MAX_DEVIDS);
        if (error < 0) {
@@ -2101,6 +2113,7 @@ static void sthcd_stop(struct usb_hcd *hcd)
 
        sthcd_oh_exit(&sthcd->oh[0]);
        sthcd_oh_exit(&sthcd->oh[1]);
+       sthcd_oh_exit(&sthcd->oh[2]);
 
        hcd->state &= ~HC_STATE_RUNNING;
 }
</pre>
This is not tested for now, please report bugs..


==todo==
*Performances are still limited to 4MB/s on a HD which can do up can do up to 30MB/s with dd on linux/x86. 
*compatibility: I got one lowend USB key, which doesn't work (stalled at the first bulk message)
*more testing is needed, but for now it seems more stable than nintendo's oh0 driver (which crashed when doing a performance test..)

==Thanks==
Thanks to the helpful wii homebrew and open-source community, especially those who write code
cios_usb2 is based on. (see source code)
