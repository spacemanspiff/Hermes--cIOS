path=%path%;C:\devkitPro\msys\bin

set ADD_DIP_PLUGIN=-DADD_DIP_PLUGIN

cd  haxxmodule
make
cd ..
pause

cd  ehcmodule
make
cd ..
pause

copy ehcmodule\bin\ehcmodule.elf cios_installer\data\ehc.elf
copy haxxmodule\bin\haxxmodule.elf cios_installer\data\haxx.elf

cd cios_installer
make clean
make
cd ..

pause