#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (the 100-slot hook table)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorEffectTrigger.h
//
// The director's camera-PFX-hook trigger vocabulary: the HookNameStringWrapper value type,
// the EffectInterface enumeration of live hooks, and the BrnDirector::Camera namespace free
// functions the arbitrator states call to request / stop a camera post-FX hook on a camera.
//
// Recovered from the DecFIGS DWARF (BrnDirectorEffectTrigger.h) + the Feb-2007 partial source
// (style/idiom) + the ARTIST asm for the free functions:
//   BrnDirector::Camera::EnsureEffectIsPlaying @0x821F2720
//   BrnDirector::Camera::StopCurrentEffect     @0x82205BB8
//   BrnDirector::HookNameStringWrapper::Set    @0x821F15B8
// DECLARATION-ONLY: the per-TU `cl /c` gate does not link, so the bodies (which live with the
// EffectTrigger / Camera TUs) are not needed here. ArbStateRoaming #includes this to request
// its per-event hooks (Smash / Billboard / Checkpoint / Wrecked / Race_Day / Damage_Crit).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // BrnGui::KI_MAX_PFX_ID_LENGTH (asm asserts strlen <= 0x20), so a hook name is at most 33
    // bytes incl. the NUL (DWARF: typedef char[33] HookNameString).
    struct HookNameStringWrapper
    {
        char mHookNameString[33];   // BrnDirectorEffectTrigger.h:40

        // @0x821F15B8: strcpy(mHookNameString, lpcName) after asserting non-NULL + length.
        void Set(const char* lpcName);
        bool operator==(const HookNameStringWrapper& lrRhs) const;
        bool operator==(const char* lpcName) const;
        bool operator!=(const HookNameStringWrapper& lrRhs) const;
        bool operator!=(const char* lpcName) const;
    };

    // The set of camera-PFX hooks currently live this frame (the SharedInfo's
    // mpEffectInterface). Formerly a minimal accessor slice; upgraded to the real DWARF
    // class shape (BrnDirectorEffectTrigger.h:79, members h:195-206) when its own TU
    // landed the hook-registration Update. X360 layout (documented; access BY NAME):
    // maHookNames data +0x000..+0xCE3 with the Array count word at +0xCE4 (3300 ==
    // 100*33 -- the trailing-count CgsArray shape), muRequestedPostFxId +0xCE8, the two
    // blends +0xCEC/+0xCF0, the two current names +0xCF4/+0xD15, then the four flag
    // bytes mbGotHooks +0xD36 / mbHasCurrentEffectName +0xD37 (the ArbStateRankUp gate
    // byte) / mbHasCurrentBackgroundEffectName +0xD38 / mbHasCurrentEffectId +0xD39.
    struct EffectInterface
    {
        // DWARF h:85 (cpp:31) -- its own ledger function (declaration-only here).
        void Construct();

        // @0x8221E0F0 (the EffectTrigger TU, cpp:46) -- register this frame's hook-name
        // enumeration: clear the table, wrap+append each name (asserting non-NULL), and
        // latch mbGotHooks; the out-flag asks the caller to (re)enumerate when the
        // hooks are still missing.
        void Update(s32 liNumHooks, const char* const* lapHookNames,
                    bool* lpbRequestEnumerationOut);

        // DWARF h:95 (cpp:72) -- its own ledger function (declaration-only here).
        void Update(bool* lpbRequestEnumerationOut);

        // DWARF h:98/h:102/h:106 -- their own ledger functions (declaration-only).
        s32 GetNumHooks() const;
        bool HookExists(const char* lpcName) const;
        const char* GetHookName(s32 liIndex) const;

        // True when an effect hook is currently requested (X360 byte @+0xD37).
        bool HasCurrentEffectName() const { return mbHasCurrentEffectName; }

        // The name of the currently-requested effect hook. NOTE: the DWARF shape
        // returns `const HookNameStringWrapper&`; the established consumers
        // (ArbStateRankUp) strcmp the raw string, so this home keeps the char*
        // form -- same bytes (the wrapper IS the char[33]).
        const char* GetCurrentEffectName() const { return mCurrentEffectName.mHookNameString; }

        // DWARF h:115-185 -- their own ledger functions (declaration-only here).
        f32 GetCurrentEffectBlendAmount() const;
        bool HasCurrentBackgroundEffectName() const;
        const HookNameStringWrapper& GetCurrentBackgroundEffectName() const;
        f32 GetCurrentBackgroundEffectBlendAmount() const;
        bool HasCurrentEffectId() const;
        u32 GetCurrentEffectId() const;
        void RegisterStartingEffectWithName(const HookNameStringWrapper& lrName, f32 lfBlend);
        void RegisterStoppingEffectWithName(const HookNameStringWrapper& lrName);
        void RegisterStartingBackgroundEffectWithName(const HookNameStringWrapper& lrName, f32 lfBlend);
        void RegisterStoppingBackgroundEffectWithName(const HookNameStringWrapper& lrName);
        void RegisterStartingEffectWithId(u32 luEffectId);
        u32 GetNullEffectId() const;

    private:
        Array<HookNameStringWrapper, 100> maHookNames;             // +0x000 (DWARF h:195; count @+0xCE4)
        u32                   muRequestedPostFxId;                  // +0xCE8 (DWARF h:197)
        f32                   mfCurrentEffectBlendAmount;           // +0xCEC (DWARF h:198)
        f32                   mfCurrentBackgroundEffectBlendAmount; // +0xCF0 (DWARF h:199)
        HookNameStringWrapper mCurrentEffectName;                   // +0xCF4 (DWARF h:200)
        HookNameStringWrapper mCurrentBackgroundHookName;           // +0xD15 (DWARF h:201)
        bool                  mbGotHooks;                           // +0xD36 (DWARF h:203)
        bool                  mbHasCurrentEffectName;               // +0xD37 (DWARF h:204)
        bool                  mbHasCurrentBackgroundEffectName;     // +0xD38 (DWARF h:205)
        bool                  mbHasCurrentEffectId;                 // +0xD39 (DWARF h:206)
    };

    // ------------------------------------------------------------------------
    // BackgroundEffectRequest (ADDITIVE GROW: its class TU) -- a pending
    // background camera-PFX request (the hook name + blend a director state
    // stages, applied against the live EffectInterface each frame). Both DWARF
    // assert cites (h:249/h:361) put its home in this header. X360 layout:
    // the leading HookNameStringWrapper (the request IS passable as the name),
    // mfBlendAmount @+0x24, mbStartRequested @+0x28, mbStopRequest @+0x29
    // (FLAG: the two flag names are inferred from HasBackgroundStartRequest()).
    // ------------------------------------------------------------------------
    struct BackgroundEffectRequest
    {
        // h:249's asserted condition -- a start request that is not a stop.
        bool HasBackgroundStartRequest() const { return mbStartRequested && !mbStopRequest; }

        // @0x823A79C8 (class TU; body in BrnDirectorEffectTrigger.cpp) -- the
        // staged blend, guarded by the h:249 tripwire.
        f32 GetBackgroundStartRequestBlendAmount() const;

        // @0x82232E88 (class TU; body in BrnDirectorEffectTrigger.cpp) -- apply
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

        // @0x821F2720. Request that the named camera-PFX hook be playing on lrCamera at the
        // given blend, but only re-trigger it when it is not already the live hook at that
        // blend (the X360 compares against the EffectInterface's current hook/blend). Used
        // for the Race_Day / Damage_Crit / Smash_Effect / Wrecked event hooks.
        void EnsureEffectIsPlaying(Camera& lrCamera, const EffectInterface& lrSource,
                                   const char* lpcHook, f32 lfBlend);

        // @0x82205BB8. Stop whatever camera-PFX effect is currently requested on lrCamera (the
        // default each frame unless an event re-requests one). Consults the EffectInterface to
        // decide whether a stop-hook or a background-effect clear is needed.
        void StopCurrentEffect(Camera& lrCamera, const EffectInterface& lrSource);

        // The X360 inlines this 3-write request into ProcessPossibleFX (set the start-hook
        // name, set its blend, latch "has start hook"): an unconditional start-hook PFX
        // request on lrCamera (used for Smash_Effect / Billboard_Effect / Checkpoint). De-
        // inlined here to a named call; the body sets mCamera.mEffects' start-hook fields.
        void RequestStartEffectHook(Camera& lrCamera, const char* lpcHook, f32 lfBlend);

        // Ensure NO camera-PFX hook by the given name is left playing on lrCamera: when the
        // named hook is the live one, stop it (the complement of EnsureEffectIsPlaying). The
        // online-race-intro arbitrator state (BrnArbStateOnlineRaceIntro::Update epilogue) calls
        // it with "BlackFadeIn_Quick" while the game says the intro may use the result bars.
        // DECLARATION-ONLY (the body lands with the EffectTrigger TU; the per-TU cl /c gate does
        // not link). FLAG: modelled on the EnsureEffectIsPlaying/StopCurrentEffect family; the
        // X360 reads the current hook from the EffectInterface and clears it if it matches.
        void EnsureEffectIsStopped(Camera& lrCamera, const EffectInterface& lrSource,
                                   const char* lpcHook);

        // The X360 inlines this RESET-then-request into ArbStateOnlineCarSelect::Update's
        // SELECTING_LIVERY case: it first clears the prior start-hook latch / secondary-latch /
        // a stale name word (mEffects start-hook latch == 0, +1 latch == 0, name-head word == 0),
        // then sets the new start-hook name + blend and re-latches "has start hook". This is the
        // RequestStartEffectHook variant that re-arms the start hook from a clean state (used for
        // the "BlackFadeIn_Quick" fade). De-inlined here to a named call; the body resets and
        // re-requests mCamera.mEffects' start-hook fields. FLAG: the precise pre-clear field set
        // is mEffects' own concern (this header does not pin those offsets).
        void RequestStartEffectHookReset(Camera& lrCamera, const char* lpcHook, f32 lfBlend);
    }
}

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H
