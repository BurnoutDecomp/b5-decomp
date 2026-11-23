// BrnResourceUnity: one of ARTIST's unity translation units (DWARF compile unit BrnResourceUnity.cpp),
// ARTIST 0x82661758-0x82674CCC.
// Each member also compiles on its own; this file holds only their includes.
// Include form: as ARTIST's other unity TUs' __FILE__ strings show (relative to this directory).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 15/19 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of BrnResource::GameDataModule::Construct).

#include "../Resource/BrnGameDataModule.cpp"
#include "../Resource/BrnGameDataTextureRender.cpp"  // holds functions of BrnGameDataModule.cpp (x360 tree split)
#include "../Resource/BrnGameDataModuleIO.cpp"
#include "../Resource/BrnResourceAllocator.cpp"
#include "../Resource/BrnResourceDebugComponent.cpp"
#include "../Resource/BrnGameDataResourceTypes.cpp"  // holds functions of BrnGameDataModuleResourceReg.cpp (x360 tree split)
#include "../Resource/BrnDLCManager.cpp"
#include "../Resource/BrnDLCManagerMonitor.cpp"  // holds functions of BrnDLCManager.cpp (x360 tree split)
#include "../Resource/SharedIO/BrnGameDataAllocatorList.cpp"
