#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h" // CgsSystem::EFrameRateManagerType

// CgsSystem::FrameRateTypeRequestInterface -- the per-frame request a consumer
// posts to change the game's frame-rate manager type (single / capped-multiple /
// uncapped-multiple). At most one change request may be live per frame; Append
// folds a second request in and asserts on a genuine conflict (both sides already
// flagged). Layout + method surface pinned by the DecFIGS DWARF
// (System/Timer/CgsFrameRateTypeRequestInterface.h) and the Append asm @0x823A7D10:
//   +0x00  meRequestedFrameRateType        (EFrameRateManagerType; lwz/stw 0(rN))
//   +0x04  mbIsFrameRateTypeChangeRequested (bool; lbz/stb 4(rN))
// Stored as a 12B span per BrnGameStateModuleIO
// (mFrameRateTypeRequestInterfaceStorage[0x4040-0x4034]).
//
// Only Append @0x823A7D10 is bodied by this TU (in the .cpp); Clear /
// IsFrameRateTypeChangeRequested / GetRequestedFrameRateTypeChange /
// RequestFrameRateTypeChange are DWARF-declared but each is its own ledger
// function, so they are declaration-only here.
namespace CgsSystem
{
    struct FrameRateTypeRequestInterface
    {
        void Clear();
        bool IsFrameRateTypeChangeRequested() const;
        EFrameRateManagerType GetRequestedFrameRateTypeChange() const;
        void RequestFrameRateTypeChange(EFrameRateManagerType leType);

        // @0x823A7D10 (this TU) -- combine another pending request into this one.
        void Append(const FrameRateTypeRequestInterface& lrOther);

    private:
        EFrameRateManagerType meRequestedFrameRateType;         // +0x00
        bool                  mbIsFrameRateTypeChangeRequested; // +0x04
    };
}
