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
// HONEST PLACEHOLDER (flagged -- see dep_flags): cTime is the engine time stamp.
// Modelled opaque here (mirrors the sibling ParticleBucket.h placeholder); not
// dereferenced by this TU's bodied shim. Replace with / collapse onto the real
// cTime home when it lands.
// ============================================================================

#include "types.hpp"

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: engine time stamp. Opaque (8 bytes); only referenced by
// const-reference here and not dereferenced by the bodied shim. Mirrors the
// sibling placeholder in ParticleBucket.h -- collapse both onto the real home.
// ----------------------------------------------------------------------------
struct cTime
{
    u64 muTicks;
};

// ParticleTrigger.h:21
struct cParticleTrigger
{
public:
    void Init();

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

// ----------------------------------------------------------------------------
// cLionFX -- Lion FX dispatch helpers (free functions).
// ----------------------------------------------------------------------------
namespace cLionFX
{
    // cLionFX::TriggerUpdate @ 0x82908888
    //
    //   cmplwi r3,0 ; beqlr   -- if (apTrigger == 0) return apTrigger;
    //   b cParticleTrigger__Update -- tail-call Update, forwarding the already
    //                                 set-up r4 (flags) / r5 (time ref).
    //
    // Returns the trigger pointer (r3 is preserved through the tail-call), matching
    // the Hex-Rays "return result" on the null path and the b/blr tail on the other.
    inline cParticleTrigger* TriggerUpdate(cParticleTrigger* apTrigger,
                                           u32 auFlags,
                                           const cTime& arTime)
    {
        if (apTrigger != nullptr)
            apTrigger->Update(auFlags, arTime);
        return apTrigger;
    }
}
