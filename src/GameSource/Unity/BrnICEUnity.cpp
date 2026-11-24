// BrnICEUnity: one of ARTIST's unity translation units (DWARF compile unit BrnICEUnity.cpp),
// ARTIST 0x8252A948-0x825403D0 (the leaves of its first pass, ICEMath/ICEData, sit before
// 0x8252B500).
// Each member also compiles on its own; this file holds only their includes.
// Include form: as ARTIST's other unity TUs' __FILE__ strings show (relative to this directory).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 24/31 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of ICE::ICECamera::Construct).

#include "../../SDKs/Packages/ICE/ICECamera.cpp"
#include "../../SDKs/Packages/ICE/ICECameraMover.cpp"
#include "../../SDKs/Packages/ICE/ICECameraSpaceHandler.cpp"
#include "../../SDKs/Packages/ICE/ICEData.cpp"
#include "../../SDKs/Packages/ICE/ICEDataEnums.cpp"
#include "../../SDKs/Packages/ICE/ICEDataICETake.cpp"  // ICETake editor methods (x360 tree split)
#include "../../SDKs/Packages/ICE/ICEFile.cpp"
#include "../../SDKs/Packages/ICE/ICEManager.cpp"
#include "../../SDKs/Packages/ICE/ICEMath.cpp"
#include "../../SDKs/Packages/ICE/ICEMemory.cpp"
#include "../../SDKs/Packages/ICE/ICERender.cpp"
#include "../../SDKs/Packages/ICE/ICEPoint.cpp"
#include "../../SDKs/Packages/ICE/ICEWrapper.cpp"
#include "../Director/BrnDirectorICEWrapperPrepare.cpp"  // holds functions of ICEWrapper.cpp (x360 tree split)
#include "../Director/BrnDirectorICEWrapper.cpp"  // EditorOn/EditorOff: ARTIST 0x82540360/0x825403D0, the TU's last pass
// Not yet included: the ICE editor, X360 ARTIST only (no PS3 counterpart); ARTIST interleaves its
// functions with the members above across 0x8252B1A8-0x8253F4E8, so it belongs here. Its x360 files
// (ICEAuthor*.cpp, ICEController*.cpp, ICEWidget/*.cpp) do not compile as one TU yet: the splits
// repeat file-local names the original had once (KI_ICE_ASSEMBLY_CHANNEL, KI_ICE_ELEMENT_TAKE_START,
// ResolveEndKey, KF_ICE_PARAM_ENGAGE_THRESHOLD, KPC_ICE_MENU_TOO_LONG, ...).
