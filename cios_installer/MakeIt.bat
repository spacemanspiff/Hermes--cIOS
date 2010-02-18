copy ..\ehcmodule\bin\ehcmodule.elf data\ehc.elf
copy ..\haxxmodule\bin\haxxmodule.elf data\haxx.elf

set ADD_DIP_PLUGIN=-DADD_DIP_PLUGIN
 
make

pause
