#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// BrnGameState::PaybackDebugComponent - the in-game debug menu for the payback / dirty-trick
// manager. Derives from the real CgsDev::DebugComponent. Recovered from the DecFIGS DWARF; the
// component owns a back-pointer to its BrnGameState::PaybackManager (first derived member -> +0x0C,
// matching the X360 `*(this + 12)` deref) and, on activation, registers two read-only enum rows
// ("Active Dirty Trick" / "Awarded Dirty Trick") that mirror the manager's meActiveDirtyTrickType /
// meAwardedDirtyTrick fields with the dirty-trick options string list. Incremental: only the
// GetName / OnActivate slice (the two boot-trace functions) is reconstructed here; render / record
// methods, if any, are owned by their own passes.

namespace BrnGameState
{
    struct PaybackManager;   // pointer member only; full definition in BrnPaybackManager.h (the .cpp includes it)

    class PaybackDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        const char* GetName() const override;   // @ 0x82357B88
        void        OnActivate() override;       // @ 0x82357B98

    private:
        // First derived member -> offset +0x0C (vtable ptr + CgsDev::DebugComponent base subobject =
        // 12 bytes), matching the X360 `*(this + 12)` access in OnActivate.
        PaybackManager* mpPaybackManager;
    };
}
