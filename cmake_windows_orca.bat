mkdir build
cd build
cmake -G"Visual Studio 11 2012 Win64" .. -DHSAIL-Tools-PATH=H:/ws/HSAIL-Tools -DHSA-Runtime-Inc-PATH=H:/ws/hsa/drivers/hsa/runtime/inc -DHSA-Runtime-Ext-PATH=H:/ws/hsa/drivers/hsa/compiler/finalizer/Interface -DOCL-Path=H:/ws/hsa/drivers/opencl/dist/windows/debug -DENABLE_HEXL_ORCA=1
