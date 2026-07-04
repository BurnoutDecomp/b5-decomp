#include "GameSource/Game/X360/BrnSystemHWX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ---------------------------------------------------------------------------
// BrnMassive::operator++(System360HWMassive::EPrepareStage&, int) @ 0x823A8958
//
// Postfix increment for the Massive prepare-stage state machine. Advances the
// stage by one and asserts the result has not run past the final stage, then
// returns the pre-increment value (postfix semantics). Mirrors the sibling
// BrnHW::operator++(System360HW::EPrepareStage&, int) @ 0x823A8A18 store-for-
// store; only the enum type and the DONE bound (E_PREPARESTAGE_DONE == 4) differ.
//
// asm: lwz r31,0(r3); addi r11,r31,1; cmpwi cr6,r11,4; stw r11,0(r3);
//      ble cr6,skip  =>  assert fires when result > 4.
// The X360 assert path (BrnSystemHWX360Massive.h:98) is dropped per convention;
// the rodata message is reproduced verbatim.
// ---------------------------------------------------------------------------
namespace BrnMassive
{
    System360HWMassive::EPrepareStage operator++(System360HWMassive::EPrepareStage& reStage, int)
    {
        System360HWMassive::EPrepareStage lePrevious = reStage;
        reStage = static_cast<System360HWMassive::EPrepareStage>(reStage + 1);
        CGS_ASSERT(reStage <= System360HWMassive::E_PREPARESTAGE_DONE,
                   "leEnumIndex <= System360HWMassive::E_PREPARESTAGE_DONE");
        return lePrevious;
    }
}
