#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<32> (the u64-backed flag set)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

// BrnDirector::Camera::ValidityAccount - the per-behaviour camera-validity flag
// account the director's shared info carries @ +0x138 (the block the Behaviour /
// CollisionPolicy Fail paths record their failure reasons into). DWARF home
// gamesource/director/camera/BrnCameraValidityAccount.h (the SetFlag assert cite).
// DecFIGS declares three distinct reason enums sharing this account's 32 bits.
namespace BrnDirector
{
    // The two dev sinks the Print overloads write to. Reference-only here, so the forward
    // declarations suffice (their real home is DirectorModule/BrnDirectorModuleDebugPrinter.h).
    // ⚠️ CLASS-KEY: `struct DebugPrinter` / `class DebugLog`, matching that home exactly -- MSVC
    // mangles the class-key into the symbol, so getting either wrong mints a symbol no TU can
    // ever define.
    struct DebugPrinter;
    class  DebugLog;

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
        enum EFailedFlag
        {
            E_FIRST_FAILED_FLAG = 0,
            E_FAILED_COLLISION = 0,
            E_FAILED_VISIBILITY = 1,
            E_FAILED_STARTED_OFF_SCREEN = 2,
            E_FAILED_COULDNT_FIND_GROUND = 3,
            E_FAILED_STARTED_IN_GEOMETRY = 4,
            E_FAILED_COLLISION_POLICY = 5,
            E_FAILED_COULDNT_FIND_ROADSIDE = 6,
            E_FAILED_POSITION_TOO_FAR_FROM_SUBJECT = 7,
            E_FAILED_ICE_MOVIE_COMPLETE = 8,
            E_FAILED_STARTED_TOO_NEAR_GEOMETRY = 9,
            E_FAILED_STARTED_TOO_NEAR_VEHICLE = 10,
            E_FAILED_INVALID_VEHICLE_REF = 11,
            E_FAILED_SUBJECT_LEFT_FRAME = 12,
            E_FAILED_SUBJECT_OCCLUDED = 13,
            E_END_FAILED_FLAG = 14,
        };

        enum ENoCutToFlag
        {
            E_FIRST_NOCUTTO_FLAG = 14,
            E_NOCUTTO_FINDING_POSITION = 14,
            E_NOCUTTO_SUBJECT_ABOUT_TO_LEAVE_FRAME = 15,
            E_NOCUTTO_SUBJECT_LEFT_FRAME = 16,
            E_NOCUTTO_SUBJECT_OCCLUDED = 17,
            E_NOCUTTO_MOMENT_CONDITIONS_NOT_MET = 18,
            E_NOCUTTO_WAITING_FOR_BEHAVIOUR_TO_PREPARE = 19,
            E_NOCUTTO_WAITING_TO_CUT_TO_BEHAVIOUR = 20,
            E_NOCUTTO_OPPORTUNITY_WINDOW_PASSED = 21,
            E_NOCUTTO_TIME_LIMIT_NEARLY_ELAPSED = 22,
            E_NOCUTTO_MOMENT_INHIBITED = 23,
            E_NOCUTTO_BEHAVIOUR_NOCUTTO = 24,
            E_NOCUTTO_ABOUT_TO_HIT_GEOMETRY = 25,
            E_NOCUTTO_ABOUT_TO_HIT_VEHICLE = 26,
            E_END_NOCUTTO_FLAG = 27,
        };

        enum ENoCutFromFlag
        {
            E_FIRST_NOCUTFROM_FLAG = 27,
            E_NOCUTFROM_INTERPOLATING = 27,
            E_NOCUTFROM_MONEY_SHOT = 28,
            E_NOCUTFROM_ICE_MOVIE_NOT_FINISHED = 29,
            E_NOCUTFROM_BEHAVIOUR_NOCUTFROM = 30,
            E_END_NOCUTFROM_FLAG = 31,
        };

        // ---- the two dev read-outs (DWARF BrnCameraValidityAccount.h:85 / :88) -------------
        // Both are `void Print(...) const` overloads: one dumps the raised reason bits to the
        // on-screen debug printer, the other appends them to the scrolling debug log. BOTH ARE
        // BODIED in BrnCameraValidityAccount.cpp as of 2026-09-11, over the reason-name table
        // recovered there -- the GROUP G trap stubs that stood in for them are gone.
        //
        // ⭐ ADDED 2026-08-29 (crash-camera wave). Both are X360-attested by
        // ArbStateCrashing: SelectNormalCrashCamera @0x82254FB0 calls the DebugPrinter form on
        // the selected moment's camera account and on its own, and ProcessPossibleStateChanges
        // @0x8224F5B0 calls the DebugLog form (the export's unnamed `sub_8223E6E0`, identified
        // as this overload by the DWARF pair and by its argument being mpDebugLog).
        // ⚠️ EVERY call site in that state is inside `if (IsDebugDisplayActive())`, and
        // ArbStateCrashing::Construct seeds that flag FALSE, so retail never reaches either.
        void Print(DebugPrinter& lrDebugPrinter) const;
        void Print(DebugLog& lrDebugLog) const;

        // @0x82204028 (class TU; body in BrnCameraValidityAccount.cpp) -- record a
        // failure reason: range-check it, then raise its bit.
        void SetFlag(EFailedFlag leFlag);

        // @0x82204148 (`sub_82204148` in the ARTIST export; identified by its assert text
        // and by its ONLY caller, Behaviour::SetCantSwitchFromMeNow @0x82206388, which
        // passes `camera + 0x138` == this account). Range-check the no-cut-from reason,
        // then raise its bit in the same u64 set SetFlag uses.
        void SetFlag(ENoCutFromFlag leFlag);

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
        void SetFlag(ENoCutToFlag leFlag);

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
