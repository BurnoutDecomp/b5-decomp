#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_EFFECT_TRIGGER_H

#include "types.hpp"

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
    // mpEffectInterface). The roaming state only passes it BY REFERENCE to the two free
    // functions below (they query it for the current hook name / blend amount), so its
    // internal layout is owned by the EffectTrigger TU and opaque here. DWARF home
    // BrnDirectorEffectTrigger.h:79.
    //
    // The rank-up arbitrator state (BrnArbStateRankUp::Update) additionally queries it for
    // whether an effect is currently requested and, if so, that effect's name (to decide
    // whether the live take is the checkpoint take). Those two reads are modelled here as
    // named accessors so the consumer never pokes the interface by offset; they are
    // DECLARATION-ONLY (the bodies live with the EffectTrigger TU; the per-TU cl /c gate
    // does not link). FLAG: minimal slice -- the real EffectInterface layout/method set
    // lands with its own TU; the accessor NAMES are stable.
    //   X360 (ArbStateRankUp::Update @0x82236380): the gate byte read at EffectInterface
    //   +0xD37 -> HasCurrentEffectName(); the name fetch (sub @0x821F17C0) ->
    //   GetCurrentEffectName().
    struct EffectInterface
    {
        // True when an effect hook is currently requested (X360 byte read at +0xD37).
        bool        HasCurrentEffectName() const;
        // The name of the currently-requested effect hook (a NUL-terminated string).
        const char* GetCurrentEffectName() const;
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
