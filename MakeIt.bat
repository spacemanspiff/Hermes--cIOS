

set ADD_DIP_PLUGIN= -DIOS38 -DADD_DIP_PLUGIN -DADD_ES_PLUGIN

rem  Now dip_plugin is external and the patches is only a bypass to get the control externally

cd  mload
make clean
make
cd ..
pause

cd cios_installer
make clean
make
cd ..

pause