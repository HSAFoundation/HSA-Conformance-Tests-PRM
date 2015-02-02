mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. -DHSAIL-Tools-PATH=/srv/HSAIL-Tools -DHSA-Runtime-Inc-PATH=/srv/hsa/drivers/hsa/runtime/inc -DHSA-Runtime-Ext-PATH=/srv/hsa/drivers/hsa/compiler/finalizer/Interface "$@" && make 
