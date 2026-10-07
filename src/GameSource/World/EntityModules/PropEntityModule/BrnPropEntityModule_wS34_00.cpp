// BrnPropEntityModule_wS34_00.cpp -- PropEntityModule::Destruct (the teardown twin of the
// Release in BrnPropEntityModule.cpp).

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"

namespace BrnWorld
{
    // ========================================================================
    // PropEntityModule::Destruct (vtable slot 3). One instruction, a tail branch into
    // ModuleSingleBuffered::Destruct; identical-code folding shares that body with
    // OverlapGenerationModule::Destruct and the sound logic module's Destruct.
    // ========================================================================
    void PropEntityModule::Destruct()
    {
        CgsModule::ModuleSingleBuffered::Destruct();
    }
}
