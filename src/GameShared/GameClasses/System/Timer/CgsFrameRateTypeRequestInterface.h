#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h" // CgsSystem::EFrameRateManagerType

// CgsSystem::FrameRateTypeRequestInterface -- the per-frame request a consumer
// posts to change the game's frame-rate manager type (single / capped-multiple /
// uncapped-multiple). At most one change request may be live per frame; Append
// folds a second request in and asserts when both sides are already flagged. The
// layout and method surface are pinned by the DecFIGS DWARF
// (System/Timer/CgsFrameRateTypeRequestInterface.h) and ARTIST's Append asm @0x823A7D10:
//   +0x00  meRequestedFrameRateType        (EFrameRateManagerType; lwz/stw 0(rN))
//   +0x04  mbIsFrameRateTypeChangeRequested (bool; lbz/stb 4(rN))
// The object is 8 bytes with natural 4-byte alignment. BrnGameStateModuleIO's former
// 12-byte opaque span included four bytes outside this object.
//
// Clear and the direct member accessors preserve the ARTIST stores/loads. Request
// records the enum and then marks it valid. Append @0x823A7D10 folds another pending
// request in and asserts on a genuine conflict (both sides already flagged).
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
