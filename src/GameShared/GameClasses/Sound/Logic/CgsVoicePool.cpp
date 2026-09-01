// ============================================================================
// CgsVoicePool.cpp -- CgsSound::Logic::VoicePoolBase + VoicePool<4> runtime bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   VoicePoolBase::IsPlaying     @ 0x82685A10
//   VoicePoolBase::Prepare       @ 0x826B6528
//   VoicePoolBase::Release       @ 0x826CF9C0
//   VoicePoolBase::Update        @ 0x826E5280
//   VoicePoolBase::GetFreeVoice  @ 0x826CFA28
//   VoicePoolBase::SetGain       @ 0x8269A9D0
//   VoicePoolBase::SetParameter  @ 0x826B6628
//   VoicePool<4>::VoicePool<4>   @ 0x826E5328
//   VoicePool<4>::~VoicePool<4>  @ 0x826E5370
//
// VoicePoolBase manages an array of PooledVoice slots (CgsVoicePool.h). BY NAME
// (2026-08-25, audio-faithfulness wave 4): the former raw byte-offset walk (the
// console `+0x5C` stride arithmetic and the KU_POOLED_VOICE_*_OFFSET reaches into
// an opaque wrapper span) is retired -- the slots are subscripted as real
// PooledVoice elements and the wrapper fields are the named CgsVoiceWrapper.h
// members/accessors (state == mWrapper.Get/SetState, live test ==
// mWrapper.HasLiveVoice, the logic Voice == mWrapper.GetVoice). The console byte
// offsets are kept in the per-function comments as the asm anchors.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsVoicePool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

// ----------------------------------------------------------------------------
// VoicePoolBase::IsPlaying  @ 0x82685A10
//   Scans the slot array for any voice whose state (console +0x48) is "playing"
//   (neither 0 == free nor 7 == stopped). Returns true on the first such voice.
// ----------------------------------------------------------------------------
bool VoicePoolBase::IsPlaying() const
{
    const u32 luCount = muPooledVoiceCount;
    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
    {
        const s32 liState = mpaPooledVoices[luIndex].mWrapper.GetState();
        if (liState != KI_VOICE_STATE_STOPPED && liState != KI_VOICE_STATE_FREE)
            return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::Prepare(lpaPooledVoices, luNumVoiceProxies)  @ 0x826B6528
//   Bind the caller-supplied PooledVoice array and reset every slot to a clean free
//   state. Asserts a non-zero count ('luNumVoiceProxies > 0') and non-null array
//   ('lapPooledVoices'). Per slot the X360 stores: mfSecondaryGain (+0x50) := 1.0f;
//   the wrapper's un-attested spans zeroed (+0x4C, +0x44, +0x48 state, the eleven
//   +0x04..+0x2C words) with the +0x30 name word := -1 (== ResetDeferredState, the
//   same store set by name); muAge (+0x54) := 0; mbInUse (+0x58) := 0. Always true.
// ----------------------------------------------------------------------------
bool VoicePoolBase::Prepare(PooledVoice* lpaPooledVoices, u32 luNumVoiceProxies)
{
    CGS_ASSERT(luNumVoiceProxies != 0, "luNumVoiceProxies > 0");
    CGS_ASSERT(lpaPooledVoices, "lapPooledVoices");

    mpaPooledVoices = lpaPooledVoices;

    for (u32 luIndex = 0; luIndex < luNumVoiceProxies; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];
        lrSlot.mfSecondaryGain = 1.0f;          // stfs +0x50
        lrSlot.mWrapper.ResetDeferredState();   // the +0x04..+0x30 / +0x44 / +0x48 / +0x4C store set
        lrSlot.muAge   = 0;                     // stw +0x54
        lrSlot.mbInUse = 0;                     // stb +0x58
    }

    muPooledVoiceCount = luNumVoiceProxies;
    return true;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::Release  @ 0x826CF9C0
//   Tear the pool down: Release() every pooled voice's wrapper and clear its mbInUse
//   (+0x58) / muAge (+0x54), then unbind (muPooledVoiceCount := 0, mpaPooledVoices
//   := null). Always returns true.
// FLAG: VoiceWrapper::Release() is declared-only (DEFERRED slice) -- unresolved
// external if this TU is mounted before it lands.
// ----------------------------------------------------------------------------
bool VoicePoolBase::Release()
{
    for (u32 luIndex = 0; luIndex < muPooledVoiceCount; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];
        lrSlot.mWrapper.Release();
        lrSlot.mbInUse = 0;
        lrSlot.muAge   = 0;
    }

    muPooledVoiceCount = 0;
    mpaPooledVoices    = 0;
    return true;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::Update  @ 0x826E5280
//   Per-frame tick. For each pooled voice: bump muAge (+0x54), Update() its wrapper,
//   then if the voice has gone idle (state (+0x48) is FREE (0) or STOPPED (7)) while
//   still flagged mbInUse (+0x58), retire it -- clear muAge and mbInUse. Finally bump
//   the pool's debug frame counter (muDebugFrameIndex, +0x10).
// FLAG: VoiceWrapper::Update() is declared-only (DEFERRED slice).
// ----------------------------------------------------------------------------
void VoicePoolBase::Update()
{
    for (u32 luIndex = 0; luIndex < muPooledVoiceCount; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];

        ++lrSlot.muAge;
        lrSlot.mWrapper.Update();

        const s32 liState = lrSlot.mWrapper.GetState();
        const bool lbActive = (liState != KI_VOICE_STATE_STOPPED) && (liState != KI_VOICE_STATE_FREE);
        if (!lbActive && lrSlot.mbInUse)
        {
            lrSlot.muAge   = 0;
            lrSlot.mbInUse = 0;
        }
    }

    ++muDebugFrameIndex;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::GetFreeVoice  @ 0x826CFA28
//   Return a pooled voice to (re)use. Assert the pool is bound to a logic module
//   (mpLogicModule @+0x04). Walk the pool: the FIRST slot whose VoiceWrapper state
//   (+0x48) is FREE (0) or STOPPED (7) is taken as-is; otherwise remember the OLDEST
//   in-use slot (highest muAge @+0x54, '>=' comparison). If the whole pool is in use,
//   assert one was found ('lpOldestPooledVoice'), Release() its wrapper and clear its
//   mbInUse (+0x58) / muAge (+0x54). Returns the chosen PooledVoice*.
// ----------------------------------------------------------------------------
PooledVoice* VoicePoolBase::GetFreeVoice()
{
    CGS_ASSERT(mpLogicModule, "mpLogicModule");

    PooledVoice* lpBest      = 0;
    u32          luOldestAge = 0;

    for (u32 luIndex = 0; luIndex < muPooledVoiceCount; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];

        const s32 liState = lrSlot.mWrapper.GetState();
        const bool lbInUse = (liState != KI_VOICE_STATE_STOPPED) && (liState != KI_VOICE_STATE_FREE);
        if (!lbInUse)
            return &lrSlot;

        if (lrSlot.muAge >= luOldestAge)
        {
            lpBest      = &lrSlot;
            luOldestAge = lrSlot.muAge;
        }
    }

    CGS_ASSERT(lpBest, "lpOldestPooledVoice");

    lpBest->mWrapper.Release();
    lpBest->mbInUse = 0;
    lpBest->muAge   = 0;
    return lpBest;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::SetGain(liSendNameHash, lfGain, liReserved, lpSendName)  @ 0x8269A9D0
//   Broadcast a gain change to every PLAYING pooled voice (state @+0x48 == 6) whose
//   wrapped handle (+0x38) is live. Applied gain is scaled by the slot's
//   mfSecondaryGain (+0x50), then forwarded to the logic Voice sub-object (+0x34) via
//   Voice::SetGain with a stack copy of the send name.
// FLAG: Voice::SetGain itself is a Playback-dependent stub (CgsVoice.cpp).
// ----------------------------------------------------------------------------
void VoicePoolBase::SetGain(s32 liSendNameHash, f32 lfGain, s32 liReserved, const u32* lpSendName)
{
    for (u32 luIndex = 0; luIndex < muPooledVoiceCount; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];
        if (lrSlot.mWrapper.GetState() == KI_VOICE_STATE_PLAYING && lrSlot.mWrapper.HasLiveVoice())
        {
            const f32 lfScaledGain = lrSlot.mfSecondaryGain * lfGain;
            u32 luSendNameCopy = *lpSendName;
            lrSlot.mWrapper.GetVoice().SetGain(
                static_cast<u32>(liSendNameHash), lfScaledGain, liReserved, &luSendNameCopy);
        }
    }
}

// ----------------------------------------------------------------------------
// VoicePoolBase::SetParameter(liSendNameHash, lfValue, liReserved, lpSendName)  @ 0x826B6628
//   Broadcast a parameter change to every pooled voice whose wrapped handle (+0x38)
//   is live -- regardless of play state (contrast SetGain, which also gates on
//   state==PLAYING). Forwards to the logic Voice sub-object (+0x34) via
//   Voice::SetParameter with a stack copy of the send name and the raw value.
// FLAG: Voice::SetParameter is a Playback-dependent stub (CgsVoice.cpp).
// ----------------------------------------------------------------------------
void VoicePoolBase::SetParameter(s32 liSendNameHash, f32 lfValue, s32 liReserved, const u32* lpSendName)
{
    for (u32 luIndex = 0; luIndex < muPooledVoiceCount; ++luIndex)
    {
        PooledVoice& lrSlot = mpaPooledVoices[luIndex];
        if (lrSlot.mWrapper.HasLiveVoice())
        {
            u32 luSendNameCopy = *lpSendName;
            lrSlot.mWrapper.GetVoice().SetParameter(
                static_cast<u32>(liSendNameHash), lfValue, liReserved, &luSendNameCopy);
        }
    }
}

// ----------------------------------------------------------------------------
// VoicePool<4>::VoicePool<4>  @ 0x826E5328
//   The concrete pool ctor: the embedded PooledVoice array (maVoices) is bound into
//   the base and reset to a clean free state via VoicePoolBase::Prepare(maVoices, N).
//   (On X360 the compiler default-constructs the four embedded wrapper sub-objects
//   and installs the pool vtable; the observable state-machine seed is the Prepare.)
// ----------------------------------------------------------------------------
template <u32 N>
VoicePool<N>::VoicePool()
{
    VoicePoolBase::Prepare(maVoices, N);
}

// ----------------------------------------------------------------------------
// VoicePool<4>::~VoicePool<4>  @ 0x826E5370
//   The concrete pool dtor: Release() the bound slot array (per-slot wrapper teardown
//   + unbind). On X360 the four embedded VoiceWrapper sub-objects are destroyed in
//   reverse; that per-element teardown is the wrapper's own ~VoiceWrapper (reached
//   through Release here). The compiler-emitted vtable installs + operator-delete
//   tail are the deleting-destructor synthesis.
// ----------------------------------------------------------------------------
template <u32 N>
VoicePool<N>::~VoicePool()
{
    Release();
}

// Explicit instantiation: emits VoicePool<4>::VoicePool<4> @ 0x826E5328 and
// ~VoicePool<4> @ 0x826E5370.
template class VoicePool<4>;

} // namespace Logic
} // namespace CgsSound
