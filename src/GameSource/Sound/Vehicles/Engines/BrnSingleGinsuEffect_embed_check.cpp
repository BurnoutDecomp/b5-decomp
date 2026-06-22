#include "GameSource/Sound/Vehicles/Engines/BrnSingleGinsuEffect.h"
#include <cstring>

// Embed check for BrnSound::Vehicles::Engines::SingleGinsuEffect. Exercises the
// three bodied ledger functions and confirms the BrnEffectObject base is reused.

namespace
{
    using BrnSound::Vehicles::Engines::SingleGinsuEffect;
    using BrnSound::Vehicles::Engines::HybridExhaustControl;

    void ExerciseSingleGinsuEffect()
    {
        SingleGinsuEffect lEffect;

        // GetTypeName @ 0x82685098 — static RTTI type-name string.
        if (std::strcmp(lEffect.GetTypeName(), "SingleGinsuEffect") != 0)
        {
            volatile int liFail = 1;
            (void)liFail;
        }

        // GetController @ 0x826850A8 — slot 0 -> 6, any other slot -> -1.
        if (lEffect.GetController(0) != 6 || lEffect.GetController(1) != -1)
        {
            volatile int liFail = 1;
            (void)liFail;
        }

        // AttachController @ 0x826850C0 — a controller whose object-id masks to the
        // HybridExhaust tag (0x50) is latched; others are ignored.
        HybridExhaustControl lController;
        lController.miObjectId = 0x50; // (0x50 & 0x7F0) == 0x50 -> matches
        lEffect.AttachController(&lController);

        CgsSound::Logic::EffectBase lOther;
        lOther.miObjectId = 0x10; // (0x10 & 0x7F0) != 0x50 -> ignored
        lEffect.AttachController(&lOther);

        // Reused-by-name: SingleGinsuEffect IS-A BrnEffectObject.
        BrnSound::Logic::BrnEffectObject* lpBase = &lEffect;
        (void)lpBase;
    }
}

int main()
{
    ExerciseSingleGinsuEffect();
    return 0;
}
