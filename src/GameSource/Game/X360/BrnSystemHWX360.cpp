#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnHW::operator++(System360HW::EPrepareStage&, int)   @ 0x823A8A18
//
// Postfix increment for the X360 hardware-prepare state machine enum. It advances
// the stage by one and asserts the result has not run past the final stage, then
// returns the pre-increment value (postfix semantics). The X360 header this lives
// in (BrnSystemHWX360.h) is platform-specific and absent from the DecFIGS DWARF;
// the enum bound (E_PREPARESTAGE_DONE) is recovered from the assert text + the
// pseudocode's `v2 > 1` overflow test.

namespace BrnHW
{
    struct System360HW
    {
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 1
        };
    };

    System360HW::EPrepareStage operator++(System360HW::EPrepareStage& reStage, int)
    {
        System360HW::EPrepareStage lePrevious = reStage;
        reStage = static_cast<System360HW::EPrepareStage>(reStage + 1);
        CGS_ASSERT(reStage <= System360HW::E_PREPARESTAGE_DONE,
                   "leEnumIndex <= System360HW::E_PREPARESTAGE_DONE");
        return lePrevious;
    }
}
