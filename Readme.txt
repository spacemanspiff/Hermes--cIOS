cIOS 222 beta 3 Modified by Hermes

Note: It uses the dip_pluging from Waninkoko cIOS249 rev 9 and it changes 
two IOS_Ioctl calls to works (see DI_Set_OffsetBase() and DI_Enable_WBFS()
functions from uloader source code (dip.c))

This cIOS prevent the hang in some games because timeout readings
or when you unplug/plug the mass storage device.

It works very fine with my problematic HDD box (resolving a problem getting 
the max lun)

Also it prevent others problematics conditions reading sectors ingame.

It supports WBFS from Windows extended partitions (max 8 subpartitions
 from the extended partition)