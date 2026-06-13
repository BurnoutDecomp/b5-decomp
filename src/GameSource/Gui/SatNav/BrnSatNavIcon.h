#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::CrashNavMapIcon::SetAlpha    @ 0x827DD5D0
//   BrnGui::CrashNavMapIcon::SetPosition @ 0x827DD568
//   BrnGui::CrashNavMapIcon::SetRotation @ 0x827DD5B0
//   BrnGui::CrashNavMapIcon::SetState    @ 0x827DD5F0
//   BrnGui::SatNavMapIcon::GetPosition   @ 0x8280FFC0
//   BrnGui::SatNavMapIcon::GetState      @ 0x827DD7C0
//   BrnGui::SatNavMapIcon::SetState      @ 0x827DD790
//
// Two map-icon classes. Common field layout (byte offsets, recovered):
//   +16  position vector (4 floats)   +32 rotation (f32)   +36 alpha (f32)
//   +40  state (s32)                  +348 dirty flag      +349 state-dirty flag
// The Set* mutators only touch the dirty flag when the value actually changes.
//
// SetPosition/GetPosition are VMX (lvx/stvx/vcmpeqfp) in the X360 build; they are
// reconstructed here as the equivalent 4-float vector compare/copy. The lane-merge
// (vrlimi128) before the compare is reproduced as a straight component-wise compare;
// flagged for review since the exact masked-lane equality is inferred.
//
// SatNavMapIcon::SetState drives the underlying flash/apt component to the animation
// label for the new state via BrnGui::FlaptIconComponent::GotoAndStopLabel; the
// component subobject sits 32 bytes before the icon, and the per-state label table
// (X360 off_82F25A00) lives in another TU.

namespace BrnGui
{
    // Forward declarations; defined in other TUs.
    struct FlaptIconComponent
    {
        void* GotoAndStopLabel(const void* pLabel);
    };
    extern void* const gaSatNavStateLabels[];   // off_82F25A00

    class CrashNavMapIcon
    {
    public:
        void* SetAlpha(f32 lfAlpha);
        void* SetRotation(f32 lfRotation);
        void* SetState(s32 liState);
        void* SetPosition(const f32* lpPosition);
    };

    inline void* CrashNavMapIcon::SetAlpha(f32 lfAlpha)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        f32& lrAlpha = *reinterpret_cast<f32*>(lBase + 36);
        if (lfAlpha != lrAlpha)
        {
            lrAlpha = lfAlpha;
            *reinterpret_cast<u8*>(lBase + 348) = 1;
        }
        return this;
    }

    inline void* CrashNavMapIcon::SetRotation(f32 lfRotation)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        f32& lrRotation = *reinterpret_cast<f32*>(lBase + 32);
        if (lfRotation != lrRotation)
        {
            lrRotation = lfRotation;
            *reinterpret_cast<u8*>(lBase + 348) = 1;
        }
        return this;
    }

    inline void* CrashNavMapIcon::SetState(s32 liState)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        s32& lrState = *reinterpret_cast<s32*>(lBase + 40);
        if (lrState != liState)
        {
            lrState = liState;
            *reinterpret_cast<u8*>(lBase + 348) = 1;
            *reinterpret_cast<u8*>(lBase + 349) = 1;
        }
        return this;
    }

    inline void* CrashNavMapIcon::SetPosition(const f32* lpPosition)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        f32* lpStored = reinterpret_cast<f32*>(lBase + 16);

        bool lbEqual = lpStored[0] == lpPosition[0]
                    && lpStored[1] == lpPosition[1]
                    && lpStored[2] == lpPosition[2]
                    && lpStored[3] == lpPosition[3];

        if (!lbEqual)
        {
            lpStored[0] = lpPosition[0];
            lpStored[1] = lpPosition[1];
            lpStored[2] = lpPosition[2];
            lpStored[3] = lpPosition[3];
            *reinterpret_cast<u8*>(lBase + 348) = 1;
        }
        return this;
    }

    class SatNavMapIcon
    {
    public:
        void  GetPosition(f32* lpOut) const;
        s32   GetState() const { return *reinterpret_cast<const s32*>(reinterpret_cast<uintptr_t>(this) + 40); }
        void* SetState(s32 liState);
    };

    inline void SatNavMapIcon::GetPosition(f32* lpOut) const
    {
        const f32* lpStored = reinterpret_cast<const f32*>(reinterpret_cast<uintptr_t>(this) + 16);
        lpOut[0] = lpStored[0];
        lpOut[1] = lpStored[1];
        lpOut[2] = lpStored[2];
        lpOut[3] = lpStored[3];
    }

    inline void* SatNavMapIcon::SetState(s32 liState)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        s32& lrState = *reinterpret_cast<s32*>(lBase + 40);
        if (lrState != liState)
        {
            lrState = liState;
            FlaptIconComponent* lpComponent =
                reinterpret_cast<FlaptIconComponent*>(lBase - 32);
            return lpComponent->GotoAndStopLabel(gaSatNavStateLabels[liState]);
        }
        return this;
    }
}
