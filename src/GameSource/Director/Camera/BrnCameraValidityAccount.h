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
    // ADDITIVE (attested by CameraState::Clear @0x82220950, which asserts
    // "sbFailFlagMaskSet" at BrnCameraValidityAccount.h:193 and ANDs the state's
    // head set with the mask; DWARF names both at BrnCameraValidityAccount.h:169/
    // :172 -- `extern bool sbFailFlagMaskSet; extern BitArray<32u> sFailFlagMask;`).
    // Defined in BrnCameraValidityAccount.cpp; produced by ValidityAccount::
    // SetupFailFlagMask @0x82221118 (reconstructed there -- DRIVE wave 2026-07-26).
    extern bool                        sbFailFlagMaskSet;   // byte_82FAA5EC
    extern CgsContainers::BitArray<32u> sFailFlagMask;      // qword_82FAA5D0

    class ValidityAccount
    {
    public:
        enum
        {
            E_FIRST_FAILED_FLAG = 0,    // h:219 (lower bound; the asm performs both checks)
            E_END_FAILED_FLAG   = 14,   // h:219 (exclusive upper bound, asm `cmplwi 0xE`)

            // The "the director may not cut AWAY from this behaviour" band. Bounds are
            // asm-attested from sub_82204148 == SetNoCutFromFlag (`cmpwi 0x1B` /
            // `cmpwi 0x1F`, assert text "leFlag >= E_FIRST_NOCUTFROM_FLAG && leFlag <
            // E_END_NOCUTFROM_FLAG", BrnCameraValidityAccount.h:245).
            E_FIRST_NOCUTFROM_FLAG = 27,
            E_END_NOCUTFROM_FLAG   = 31,
        };

        // @0x82204028 (class TU; body in BrnCameraValidityAccount.cpp) -- record a
        // failure reason: range-check it, then raise its bit.
        void SetFlag(s32 leFlag);

        // @0x82204148 (`sub_82204148` in the ARTIST export; identified by its assert text
        // and by its ONLY caller, Behaviour::SetCantSwitchFromMeNow @0x82206388, which
        // passes `camera + 0x138` == this account). Range-check the no-cut-from reason,
        // then raise its bit in the same u64 set SetFlag uses.
        void SetNoCutFromFlag(s32 leFlag);

        // Per-frame reset (X360 BehaviourHelper::Update @0x82220688, inlined):
        //     assert(sbFailFlagMaskSet);                       // h:193
        //     *(camera + 0x138) &= sFailFlagMask;              // ld / and / std
        // i.e. keep only the FAILURE bits and drop every no-cut book-keeping bit, so each
        // frame's "can the director cut to/from this behaviour" verdict is rebuilt from
        // scratch while a failure stays latched. De-opted to named BitArray ops.
        void MaskToFailFlags();

        // The "the director may not cut TO this behaviour" counterpart, called by
        // Behaviour::SetCantSwitchToMeNow (Behaviour.h:447). DECLARATION-ONLY: no X360
        // export for it is present in the available dumps, so its flag BAND is NOT
        // attested and is deliberately not fabricated here. DELETE-WHEN: the sibling's
        // address is identified (it is the ICF-adjacent twin of @0x82204148).
        void SetNoCutToFlag(s32 leFlag);

        // @0x82221118 (body in BrnCameraValidityAccount.cpp) -- one-time setup of the
        // module fail-flag mask pair: raise bits [0..E_END_FAILED_FLAG) in sFailFlagMask
        // and latch sbFailFlagMaskSet. Called by CameraState::Construct @0x82252348.
        static void SetupFailFlagMask();

    private:
        // The u64-backed 32-slot bit set (the X360 SetFlag inlines the BitArray
        // 64-bit-field SetBit with the CgsBitArray.h:222 index guard).
        CgsContainers::BitArray<32u> mFailedFlags;   // FLAG: member name inferred
    };
}
}
