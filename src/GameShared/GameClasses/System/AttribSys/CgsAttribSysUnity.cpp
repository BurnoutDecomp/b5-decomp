// CgsAttribSysUnity: one of ARTIST's unity translation units (DWARF compile unit CgsAttribSysUnity.cpp),
// ARTIST 0x82802520-0x8280FF4C. Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820D8BB8: ...gameshared\gameclasses\system\attribsys\CgsAttribSysModule.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 44/55 transitions forward);

#include "CgsAttribSysModule.cpp"
#include "CgsAttribSysModuleIO.cpp"
#include "CgsAttribSysMemoryManager.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribcollection.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.cpp"  // holds functions of attribdatabase.cpp (x360 tree split)
#include "CgsAttribSysDebugComponent.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attriblivelink.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribgarbagecollector.cpp"  // holds functions of attribloadandgo.cpp (x360 tree split)
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribsupport.cpp"
#include "CgsAttribSysVaultSlot.cpp"
#include "CgsAttribSysVaultLoad.cpp"  // holds functions of CgsAttribSysVaultSlot.cpp (x360 tree split)
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.cpp"
#include "../../../../SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhashmap.cpp"  // holds functions of attribute.cpp (x360 tree split)
#include "CgsAttribSysCollectionKey.cpp"
#include "CgsAttribSysVaultArray.cpp"
#include "CgsAttribSysVaultAllocator.cpp"
