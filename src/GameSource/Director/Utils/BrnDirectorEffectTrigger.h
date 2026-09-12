#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H

#include "types.hpp"
#include <cstring>                                        // strcpy / strcmp / strlen (Set + the comparisons)
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (the 100-slot hook table)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (the current-effect tripwires)
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"            // BrnGui::KI_MAX_PFX_ID_LENGTH (Set's tripwire)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorEffectTrigger.h
//
// The director's camera-PFX-hook trigger vocabulary: the HookNameStringWrapper value type,
// the EffectInterface enumeration of live hooks, and the BrnDirector::Camera namespace free
// functions the arbitrator states call to request / stop a camera post-FX hook on a camera.
//
// Reconstructed from the original declarations plus the attested behaviour of the free
// functions BrnDirector::Camera::EnsureEffectIsPlaying, BrnDirector::Camera::StopCurrentEffect
// and BrnDirector::HookNameStringWrapper::Set.
// DECLARATION-ONLY: the per-TU `cl /c` gate does not link, so the bodies (which live with the
// EffectTrigger / Camera TUs) are not needed here. ArbStateRoaming #includes this to request
// its per-event hooks (Smash / Billboard / Checkpoint / Wrecked / Race_Day / Damage_Crit).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // BrnGui::KI_MAX_PFX_ID_LENGTH (the console asserts strlen <= 0x20), so a hook name is at
    // most 33 bytes incl. the NUL (the original types it `typedef char[33] HookNameString`).
    struct HookNameStringWrapper
    {
        char mHookNameString[33];   // +0x00

        // BODIED 2026-08-01 (was declaration-only; it was one of the camera wave's unresolved
        // externals, pulled in by Camera::EnsureEffectIsPlaying).
        // HEADER INLINE by rule: the function's own two tripwires name THIS HEADER as their
        // home, so it is defined here rather than emitted out of line. Attested behaviour,
        // in order:
        //   assert lpcName != NULL
        //   assert strlen(lpcName) <= 0x20
        //   a third assert belongs to the bounded-copy helper it uses, not to this function
        //     ("String <x> is too long. Buffer size = 32, string length = N"); it fires at
        //     strlen >= 0x20, one tighter than the second, and is folded into the same
        //     CGS_ASSERT here rather than fabricating the StrStream message.
        //   a plain strcpy including the NUL.
        void Set(const char* lpcName)
        {
            CGS_ASSERT(lpcName != NULL, "lpcName != NULL");                              // 1st
            CGS_ASSERT(strlen(lpcName) <= BrnGui::KI_MAX_PFX_ID_LENGTH,
                       "strlen(lpcName) <= BrnGui::KI_MAX_PFX_ID_LENGTH");               // 2nd
            strcpy(mHookNameString, lpcName);
        }

        // User-defined copy-assign routed through Set (GROWN by the
        // Array<HookNameStringWrapper,100> instantiation TU: its Append assigns the new
        // slot through the Set symbol -- the string sits at offset 0, so the strcpy-shaped
        // operator= and Set(const char*) fold into one function in the original build).
        HookNameStringWrapper& operator=(const HookNameStringWrapper& lrRhs)
        {
            Set(lrRhs.mHookNameString);
            return *this;
        }

        // All four are declared (and so defined) IN THIS HEADER by the original.
        // BODIED 2026-08-01: they were declaration-only, and the pair taking `const char*` is
        // already used by BrnDirectorEffectTrigger.cpp's RegisterStopping*EffectWithName and
        // (inlined) by Camera::EnsureEffectIsPlaying / EffectInterface::HookExists. Every
        // console site performs the same open-coded byte compare that `strcmp(a,b) == 0`
        // compiles to (e.g. inside EnsureEffectIsPlaying).
        bool operator==(const HookNameStringWrapper& lrRhs) const
        {
            return strcmp(mHookNameString, lrRhs.mHookNameString) == 0;
        }
        bool operator==(const char* lpcName) const
        {
            return strcmp(mHookNameString, lpcName) == 0;
        }
        bool operator!=(const HookNameStringWrapper& lrRhs) const { return !(*this == lrRhs); }
        bool operator!=(const char* lpcName) const               { return !(*this == lpcName); }
    };

    // The set of camera-PFX hooks currently live this frame (the SharedInfo's
    // mpEffectInterface). Formerly a minimal accessor slice; upgraded to the real class
    // shape from the original declarations when its own TU landed the hook-registration
    // Update. Console layout (documented; access BY NAME):
    // maHookNames data +0x000..+0xCE3 with the Array count word at +0xCE4 (3300 ==
    // 100*33 -- the trailing-count CgsArray shape), muRequestedPostFxId +0xCE8, the two
    // blends +0xCEC/+0xCF0, the two current names +0xCF4/+0xD15, then the four flag
    // bytes mbGotHooks +0xD36 / mbHasCurrentEffectName +0xD37 (the ArbStateRankUp gate
    // byte) / mbHasCurrentBackgroundEffectName +0xD38 / mbHasCurrentEffectId +0xD39.
    struct EffectInterface
    {
        // Its own ledger function (declaration-only here).
        void Construct();

        // In the EffectTrigger TU -- register this frame's hook-name
        // enumeration: clear the table, wrap+append each name (asserting non-NULL), and
        // latch mbGotHooks; the out-flag asks the caller to (re)enumerate when the
        // hooks are still missing.
        void Update(s32 liNumHooks, const char* const* lapHookNames,
                    bool* lpbRequestEnumerationOut);

        // Its own ledger function (declaration-only here).
        void Update(bool* lpbRequestEnumerationOut);

        // Their own ledger functions (declaration-only).
        s32 GetNumHooks() const;
        const char* GetHookName(s32 liIndex) const;

        // BODIED 2026-08-01 in BrnDirectorEffectTrigger.cpp (the walk is in that banner).
        bool HookExists(const char* lpcName) const;

        // True when an effect hook is currently requested (the byte at +0xD37).
        bool HasCurrentEffectName() const { return mbHasCurrentEffectName; }

        // The name of the currently-requested effect hook. NOTE: the original shape
        // returns `const HookNameStringWrapper&`; the established consumers
        // (ArbStateRankUp) strcmp the raw string, so this home keeps the char*
        // form -- same bytes (the wrapper IS the char[33]).
        // In the class TU: the tripwire, then the name (the console returns the
        // wrapper's address == its string; non-gating).
        const char* GetCurrentEffectName() const
        {
            CGS_ASSERT(mbHasCurrentEffectName, "HasCurrentEffectName()");   // non-gating
            return mCurrentEffectName.mHookNameString;
        }

        // Their own ledger functions (declaration-only here).
        f32 GetCurrentEffectBlendAmount() const;
        bool HasCurrentBackgroundEffectName() const;
        const HookNameStringWrapper& GetCurrentBackgroundEffectName() const;
        f32 GetCurrentBackgroundEffectBlendAmount() const;
        void RegisterStartingEffectWithName(const HookNameStringWrapper& lrName, f32 lfBlend);
        void RegisterStoppingEffectWithName(const HookNameStringWrapper& lrName);
        void RegisterStoppingBackgroundEffectWithName(const HookNameStringWrapper& lrName);
        void RegisterStartingEffectWithId(u32 luEffectId);

        // BODIED 2026-08-01 in BrnDirectorEffectTrigger.cpp -- the console INLINES it at its
        // only caller, BackgroundEffectRequest::RegisterAndUpdateRequest, and that is where
        // the three stores were read from.
        void RegisterStartingBackgroundEffectWithName(const HookNameStringWrapper& lrName, f32 lfBlend);

        // ---- the effect-ID trio (header inlines) ----------------------------------------
        // BODIED 2026-08-01. All three are inlined by the console; the one function that reads
        // all three in one place is Camera::StopCurrentEffect, which:
        //     reads the flag byte at +0xD39 -> mbHasCurrentEffectId
        //     reads the word at +0xCE8      -> the current effect id (muRequestedPostFxId)
        //     materialises the constant 0x7BEC6 == 507078, the value it BOTH compares against
        //                                     and writes back as "no effect"
        // i.e. the null-effect id is a literal, not a member. (Note the original's member name
        // for +0xCE8 is muRequestedPostFxId -- the interface stores the requested id and the
        // getter's original name is GetCurrentEffectId; same word.)
        bool HasCurrentEffectId() const { return mbHasCurrentEffectId; }
        u32  GetCurrentEffectId() const { return muRequestedPostFxId; }
        u32  GetNullEffectId() const    { return 507078u; }   // 0x7BEC6

    private:
        Array<HookNameStringWrapper, 100> maHookNames;             // +0x000 (count at +0xCE4)
        u32                   muRequestedPostFxId;                  // +0xCE8
        f32                   mfCurrentEffectBlendAmount;           // +0xCEC
        f32                   mfCurrentBackgroundEffectBlendAmount; // +0xCF0
        HookNameStringWrapper mCurrentEffectName;                   // +0xCF4
        HookNameStringWrapper mCurrentBackgroundHookName;           // +0xD15
        bool                  mbGotHooks;                           // +0xD36
        bool                  mbHasCurrentEffectName;               // +0xD37
        bool                  mbHasCurrentBackgroundEffectName;     // +0xD38
        bool                  mbHasCurrentEffectId;                 // +0xD39
    };

    // ------------------------------------------------------------------------
    // BackgroundEffectRequest (ADDITIVE GROW: its class TU) -- a pending
    // background camera-PFX request (the hook name + blend a director state
    // stages, applied against the live EffectInterface each frame). Both of its
    // assert tripwires name this header as its home. Console layout:
    // the leading HookNameStringWrapper (the request IS passable as the name),
    // mfBlendAmount @+0x24, mbStartRequested @+0x28, mbStopRequest @+0x29
    // (FLAG: the two flag names are inferred from HasBackgroundStartRequest()).
    // ------------------------------------------------------------------------
    struct BackgroundEffectRequest
    {
        // The first tripwire's asserted condition -- a start request that is not a stop.
        bool HasBackgroundStartRequest() const { return mbStartRequested && !mbStopRequest; }

        // Body in BrnDirectorEffectTrigger.cpp -- the staged blend, guarded by that
        // same tripwire.
        f32 GetBackgroundStartRequestBlendAmount() const;

        // Body in BrnDirectorEffectTrigger.cpp -- apply
        // the pending request against the live interface.
        void RegisterAndUpdateRequest(EffectInterface* lpEffectInterface);

        HookNameStringWrapper mHookName;         // +0x00 (the request's hook name)
        u8                    maPad21[3];        // +0x21..+0x23
        f32                   mfBlendAmount;     // +0x24
        bool                  mbStartRequested;  // +0x28
        bool                  mbStopRequest;     // +0x29
    };

    namespace Camera
    {
        struct Camera;

        // BODIED 2026-08-01 in BrnDirectorEffectTrigger.cpp (see the walk in the banner
        // there). Drop this frame's start/stop-hook request and the requested
        // post-FX id on lrCamera, then re-request the named camera-PFX hook at the given
        // blend UNLESS it is already the EffectInterface's live hook at exactly that blend.
        // Used for the Car_Reset / Race_Day / Damage_Crit / Smash_Effect / Wrecked hooks and
        // by ArbStateCarSelect for the junkyard fade-outs.
        // SIGNATURE RECOVERED FROM THE ATTESTED CALLS: Camera&, const EffectInterface&,
        // const char*, f32 -- matching the committed order below.
        void EnsureEffectIsPlaying(Camera& lrCamera, const EffectInterface& lrSource,
                                   const char* lpcHook, f32 lfBlend);

        // Stop whatever camera-PFX effect is currently requested on lrCamera (the
        // default each frame unless an event re-requests one). Consults the EffectInterface to
        // decide whether a stop-hook or a background-effect clear is needed.
        void StopCurrentEffect(Camera& lrCamera, const EffectInterface& lrSource);

        // The console inlines this 3-write request into ProcessPossibleFX (set the start-hook
        // name, set its blend, latch "has start hook"): an unconditional start-hook PFX
        // request on lrCamera (used for Smash_Effect / Billboard_Effect / Checkpoint). De-
        // inlined here to a named call; the body sets mCamera.mEffects' start-hook fields.
        void RequestStartEffectHook(Camera& lrCamera, const char* lpcHook, f32 lfBlend);

        // BODIED 2026-09-12 in BrnDirectorEffectTrigger.cpp (see the walk in the
        // banner there). Ensure NO camera-PFX hook by the given name is left playing on
        // lrCamera: drop a pending start-hook request for that name, then -- unless a request is
        // still standing -- ask for the hook to be stopped by name when it is the interface's
        // live effect. The complement of EnsureEffectIsPlaying. The online-race-intro arbitrator
        // state (BrnArbStateOnlineRaceIntro::Update epilogue) calls it with "BlackFadeIn_Quick"
        // while the game says the intro may use the result bars.
        void EnsureEffectIsStopped(Camera& lrCamera, const EffectInterface& lrSource,
                                   const char* lpcHook);

        // The console inlines this RESET-then-request into ArbStateOnlineCarSelect::Update's
        // SELECTING_LIVERY arm and ArbStateOnlineRaceIntro::Update's epilogue: it first clears
        // the prior request state (the start-hook latch, the stop-hook latch and the requested
        // post-FX id), then sets the new start-hook name + blend and re-latches "has start
        // hook". This is the RequestStartEffectHook variant that re-arms the start hook from a
        // clean state (used for the "BlackFadeIn_Quick" fade). De-inlined here to a named call;
        // BODIED 2026-09-12 in BrnDirectorEffectTrigger.cpp.
        void RequestStartEffectHookReset(Camera& lrCamera, const char* lpcHook, f32 lfBlend);
    }
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H
