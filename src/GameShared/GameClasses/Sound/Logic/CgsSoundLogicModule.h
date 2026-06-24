#ifndef CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
#define CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (range-guarded operator++)

// =============================================================================
// CgsSound::Logic::Module -- the sound-logic module.
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. MINIMAL-SLICE NOTE: the Module class
// is a large keystone (Prepare/Release state machines, the VariableEventQueue, the
// Environment, etc.). The ONLY surface modelled here is the prepare-stage enum and
// its post-increment operator (X360 @0x82681D30), which the prepare state machine
// walks. The rest of the class is left to its full owning keystone TU.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// The Module's prepare-stage enum. The X360 prepare state machine walks the stages
// with the post-increment operator below, range-guarding the stepped-to stage against
// the final eModulePrepareDone stage (== 6, the bound the X360 asserts at
// CgsSoundLogicModule.h:277: "leEnumIndex <= Module::eModulePrepareDone").
namespace Module
{
    enum EPrepareState
    {
        eModulePrepareStage0 = 0,
        eModulePrepareStage1 = 1,
        eModulePrepareStage2 = 2,
        eModulePrepareStage3 = 3,
        eModulePrepareStage4 = 4,
        eModulePrepareStage5 = 5,
        eModulePrepareDone   = 6
    };
}

// Post-increment over the Module prepare stages. X360 @0x82681D30 (DWARF cites the
// assert at CgsSoundLogicModule.h:277): saves the old stage, increments in place, then
// range-guards the stepped-to stage with the project CGS_ASSERT. The X360 fires the
// assert when the incremented stage exceeds Module::eModulePrepareDone (i.e. > 6) and
// returns the SAVED OLD stage in every path -- the guard is a non-gating tripwire.
//   asm: lwz r31,0(r3); addi r11,r31,1; cmpwi r11,6; stw r11,0(r3); ble skip;
//        <Begin/Fire("leEnumIndex <= Module::eModulePrepareDone",...,277)/End>; mr r3,r31
// POST-increment (the trailing dummy `int` Hex-Rays drops as "unused"): it returns the
// SAVED OLD value, the post-increment contract. Caller: Module::Prepare @0x826C42A8.
Module::EPrepareState operator++(Module::EPrepareState& leEnumIndex, int);

}
}

#endif // CGS_SOUND_LOGIC_CGSSOUNDLOGICMODULE_H
