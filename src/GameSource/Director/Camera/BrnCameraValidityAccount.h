#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<32> (the u64-backed flag set)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

// BrnDirector::Camera::ValidityAccount - the per-behaviour camera-validity flag
// account the director's shared info carries @ +0x138 (the block the Behaviour /
// CollisionPolicy Fail paths record their failure reasons into). DWARF home
// gamesource/director/camera/BrnCameraValidityAccount.h (the SetFlag assert cite).
// MINIMAL slice: only SetFlag @0x82204028 is homed; the named per-flag enumerators
// (h:219 asserts a failed-flag range of [0,14)) are not yet recovered -- FLAG: the
// range bounds are modelled, the flag names land with the account's own TU.
namespace BrnDirector
{
namespace Camera
{
    class ValidityAccount
    {
    public:
        enum
        {
            E_FIRST_FAILED_FLAG = 0,    // h:219 (lower bound; the asm performs both checks)
            E_END_FAILED_FLAG   = 14,   // h:219 (exclusive upper bound, asm `cmplwi 0xE`)
        };

        // @0x82204028 (class TU; body in BrnCameraValidityAccount.cpp) -- record a
        // failure reason: range-check it, then raise its bit.
        void SetFlag(s32 leFlag);

    private:
        // The u64-backed 32-slot bit set (the X360 SetFlag inlines the BitArray
        // 64-bit-field SetBit with the CgsBitArray.h:222 index guard).
        CgsContainers::BitArray<32u> mFailedFlags;   // FLAG: member name inferred
    };
}
}
