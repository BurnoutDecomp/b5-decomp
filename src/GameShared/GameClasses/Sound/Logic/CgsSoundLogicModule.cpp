// ============================================================================
// CgsSoundLogicModule.cpp -- CgsSound::Logic::Module runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Logic::operator++(Module::EPrepareState&, int)  @ 0x82681D30
//
// MINIMAL-SLICE NOTE: only the prepare-stage post-increment operator is homed here
// (the prepare state machine steps through the stages with it). The rest of the
// Module keystone (Prepare/Release, the VariableEventQueue, the Environment) is left
// to its full owning TU.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// operator++(Module::EPrepareState&, int)  @ 0x82681D30
//   v1 = *a1; *a1 = v1 + 1; if (v1 + 1 > 6) <assert>; return v1;
// (asm: lwz r31,0(r3) [old]; addi r11,r31,1 [new]; cmpwi cr6,r11,6; stw r11,0(r3)
//  [store new UNCONDITIONALLY]; ble cr6,skip; Begin/Fire/End; mr r3,r31 [return old].)
// The store of the incremented value happens before the guard, so the increment is
// applied even on the assert path; the return is always the saved old value.
// ---------------------------------------------------------------------------
Module::EPrepareState operator++(Module::EPrepareState& leEnumIndex, int)
{
    const Module::EPrepareState leOldEnumIndex = leEnumIndex; // lwz r31, 0(r3)
    leEnumIndex = static_cast<Module::EPrepareState>(static_cast<s32>(leEnumIndex) + 1); // addi/stw

    CGS_ASSERT(leEnumIndex <= Module::eModulePrepareDone,
               "leEnumIndex <= Module::eModulePrepareDone");

    return leOldEnumIndex; // mr r3, r31
}

} // namespace Logic
} // namespace CgsSound
