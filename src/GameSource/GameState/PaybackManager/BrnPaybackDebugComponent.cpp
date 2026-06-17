#include "GameSource/GameState/PaybackManager/BrnPaybackDebugComponent.h"
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"      // full PaybackManager (named meActiveDirtyTrickType/meAwardedDirtyTrick)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The payback debug menu registers two read-only
// enum rows mirroring the PaybackManager's currently-active / last-awarded dirty trick, shown
// via the dirty-trick options string list (the X360 fed the single shared &unk_8202AC40 to both
// SetOptions calls). The base register/range/options/readonly API is the real CgsDev::
// DebugComponent surface, called by name. The enum fields are BrnNetwork::EPaybackType (4-byte);
// the API is s32*-typed, so the named field address is reinterpret_cast<s32*>.

namespace BrnGameState
{
    namespace
    {
        // The shared dirty-trick options table (X360 &unk_8202AC40 in .rdata): each EPaybackType
        // value paired with its display name. CgsDev::DebugUI::StringList = { s32 miValue; const
        // char* mpcName; }. The array decays to the const StringList* SetOptions takes.
        const CgsDev::DebugUI::StringList KA_DIRTY_TRICK_OPTIONS[] =
        {
            { BrnNetwork::E_PAYBACK_TYPE_REVERSE_STEERING,                "Reverse Steering"   },
            { BrnNetwork::E_PAYBACK_TYPE_BOOST_LOCK,                      "Boost Lock"         },
            { BrnNetwork::E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM, "Aggressor Controls" },
            { BrnNetwork::E_PAYBACK_TYPE_SIX_AXIS_STEERING,               "Six Axis Steering"  },
        };
    }

    // @ 0x82357B88.
    const char* PaybackDebugComponent::GetName() const
    {
        return "Payback";
    }

    // @ 0x82357B98.
    void PaybackDebugComponent::OnActivate()
    {
        // "Active Dirty Trick": read-only mirror of the manager's currently-active dirty trick,
        // shown via the dirty-trick name list. (X360: RegisterVariable / SetReadOnly / SetOptions
        // on &mpPaybackManager->meActiveDirtyTrickType == *(this+12)+596.)
        s32* lpiActiveDirtyTrick = reinterpret_cast<s32*>(&mpPaybackManager->meActiveDirtyTrickType);
        RegisterVariable(lpiActiveDirtyTrick, "Active Dirty Trick");
        SetReadOnly(lpiActiveDirtyTrick, true);
        SetOptions(lpiActiveDirtyTrick, KA_DIRTY_TRICK_OPTIONS);

        // "Awarded Dirty Trick": read-only mirror of the awarded dirty trick, clamped to the valid
        // EPaybackType range [0,3] (== E_PAYBACK_TYPE_COUNT-1). (X360: ...+600, SetRange(...,0,3).)
        s32* lpiAwardedDirtyTrick = reinterpret_cast<s32*>(&mpPaybackManager->meAwardedDirtyTrick);
        RegisterVariable(lpiAwardedDirtyTrick, "Awarded Dirty Trick");
        SetRange(lpiAwardedDirtyTrick, 0, 3);
        SetReadOnly(lpiAwardedDirtyTrick, true);
        SetOptions(lpiAwardedDirtyTrick, KA_DIRTY_TRICK_OPTIONS);
    }
}
