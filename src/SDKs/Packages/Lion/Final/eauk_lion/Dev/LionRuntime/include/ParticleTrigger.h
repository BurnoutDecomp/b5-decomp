#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h
//
// cParticleTrigger -- a Lion (eauk_lion) particle effect trigger: a small
// stop-watch (current / start / stop time stamps) plus an enable flag, driving
// whether an effect is currently running and how far through its lifetime it is.
//
// cLionFX::TriggerUpdate -- the free dispatch shim BrnParticle::ParticleModule::
// DispatchThreadUpdate calls per trigger: null-guards the trigger pointer then
// forwards (flags, time) to cParticleTrigger::Update.
//
// LAYOUT AUTHORITY: member set / order / types are from the DecFIGS DWARF
// (ParticleTrigger.h:21). The only function bodied in this TU is the
// cLionFX::TriggerUpdate shim @0x82908888; cParticleTrigger's own methods are
// declared here (their bodies live in their own TUs) -- Update is the one the
// shim tail-calls.
//
// The mangled symbol _ZN7cLionFX13TriggerUpdateEP16cParticleTriggerjRK5cTime
// fixes the shim's true arity: (cParticleTrigger*, u32, const cTime&) -- the
// Hex-Rays single-arg view drops the two args the X360 simply forwards in the
// tail-call (b cParticleTrigger__Update keeps r4/r5 live).
//
// cTime IS NO LONGER A PLACEHOLDER HERE (2026-09-03). This header used to carry its own
// 4-byte `struct cTime`, derived correctly from cParticleTrigger::Update @0x82908808 --
// which reads/writes it as a SINGLE 32-bit word (lwz/stw, signed cmpw), with the three
// trigger stamps at +0x00/+0x04/+0x08 and mbEnabled at +0x0C. That reading was right, and
// the sibling ParticleBucket.h copy (8 bytes) was wrong; both are now retired onto the one
// home at ext-include/GameStructs/cTime.h, whose DWARF says `S32 mTicks`.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"

// cTime now comes from its real home, ext-include/GameStructs/cTime.h (included above).
// THIS HEADER'S OWN NOTE WAS RIGHT ALL ALONG and is what the unification is built on: it
// reasoned the width to 4 bytes from cParticleTrigger::Update @0x82908808 ("each stamp is
// loaded/stored as one 32-bit word (lwz/stw) and signed-compared (cmpw/blt), and mbEnabled
// lands at +0x0C"), and said "collapse onto the real cTime home when it lands". It landed;
// the DWARF (cTime.h:217, `S32 mTicks`) agrees with this reading and not with the 8-byte
// copy ParticleBucket.h was carrying.

// ParticleTrigger.h:21
struct cParticleTrigger
{
public:
    // ParticleTrigger.h:31 (DWARF). NO STANDALONE X360 BODY -- inlined into
    // cLionFX::TriggerRegister @0x8290ACA8, which clears the four fields directly
    // (`stb r11, 0xC(r3)` then `stw r11` at 0, 4 and 8, asm 0x8290ACC8..0x8290ACD8).
    // De-inlined back onto the owning type, because the fields have names.
    void Init()
    {
        mTime.BuildZero();
        mTimeStart.BuildZero();
        mTimeStop.BuildZero();
        mbEnabled = false;
    }

    // Advances the trigger by the elapsed time for the given update flags.
    // (declared here; bodied in its own TU)
    void Update(u32 auFlags, const cTime& arTime);

    bool         IsRunning() const     { return mbEnabled; }
    const cTime& GetTime() const       { return mTime; }
    const cTime& GetTimeStart() const  { return mTimeStart; }
    void         SetStartTime(const cTime& arTime) { mTimeStart = arTime; }
    const cTime& GetTimeStop() const   { return mTimeStop; }

    u32  GetStatus(const cTime& arTime, f32 af0, f32 af1, f32 af2, f32 af3,
                   f32 af4, u32 au5, f32& arf6, f32& arf7);

    cTime GetAge() const;

private:
    // ParticleTrigger.h:181..184 -- member set/order from DWARF.
    cTime mTime;        // current time
    cTime mTimeStart;   // start time stamp
    cTime mTimeStop;    // stop time stamp
    bool  mbEnabled;    // running flag
};

// ⛔⛔ AN ODR FORK USED TO END THIS FILE, AND IT IS RETIRED (2026-09-03). It was:
//
//     namespace cLionFX { inline cParticleTrigger* TriggerUpdate(...) { ... } }
//
// cLionFX is a STRUCT (LionFX.h, DWARF LionFX.h:45), not a namespace. Two entities of
// different kinds under one name is a hard redefinition, and the only reason it never
// produced a diagnostic is that no TU had yet included both headers -- the exact shape of
// "ODR forks link silently". cLionFX::TriggerUpdate @0x82908888 now lives on the real
// cLionFX, in LionFX.cpp, beside its four sibling registry entry points.
