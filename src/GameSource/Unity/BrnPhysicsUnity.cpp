// BrnPhysicsUnity: one of ARTIST's unity translation units (DWARF compile unit BrnPhysicsUnity.cpp),
// ARTIST 0x8259C1C0-0x825B257C.
// Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x8208CBE0: ...gamesource\unity\../Physics/BrnPhysicsModule.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 20/25 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of BrnPhysics::PhysicsModule::Construct).

#include "../Physics/BrnPhysicsModule.cpp"
#include "../Physics/BrnPhysicsModuleIO_InputBuffer_Accessors.cpp"  // holds functions of BrnPhysicsModule.cpp (x360 tree split)
#include "../Physics/BrnPhysicsModuleBridgeFunctions.cpp"
#include "../Physics/BrnPhysicsModuleIO_InputBuffer.cpp"  // holds functions of BrnPhysicsModuleIO.cpp (x360 tree split)
#include "../Physics/BrnPhysicsModuleIO_OutputBuffer.cpp"  // holds functions of BrnPhysicsModuleIO.cpp (x360 tree split)
#include "../Physics/BrnPhysicsModuleUpdateFunctions.cpp"
#include "../Physics/BrnPhysicsModuleGameActions.cpp"  // holds functions of BrnPhysicsModuleUpdateFunctions.cpp (x360 tree split)
#include "../Physics/PhysicsUtilities/ExternalPhysicsBody.cpp"
#include "../Physics/PhysicsUtilities/ExternalPhysicsBody_ReadPropertiesFromRenderware.cpp"  // holds functions of ExternalPhysicsBody.cpp (x360 tree split)
#include "../Physics/PhysicsUtilities/InterpedParam3.cpp"
#include "../Physics/PhysicsUtilities/Spring1D.cpp"
#include "../Physics/PhysicsUtilities/SuspensionSpring.cpp"  // holds functions of Spring1D.cpp (x360 tree split)
#include "../Physics/PhysicsUtilities/ExternallySimulatedBody.cpp"
#include "../Physics/ContactSpies/BrnContactSpyData.cpp"
