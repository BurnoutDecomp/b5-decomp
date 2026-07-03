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
// VoicePoolBase manages an array of PooledVoice slots (CgsVoicePool.h). The pool
// bodies reach the DEFERRED VoiceWrapper (PooledVoice::mVoice) at raw X360-attested
// sub-offsets (state @+0x48, Voice @+0x34, handle @+0x38) exactly as the guest does;
// VoiceWrapper::Release()/Update() and Voice::SetGain()/SetParameter() are called BY
// NAME on the sub-objects. See CgsVoicePool.h for the layout + the KU_POOLED_VOICE_*
// raw-offset constants.
//
// NOTE (promotion): the local anonymous VoicePoolBase{mPad[8]; mpVoices; muVoiceCount}
// that previously lived here has been promoted into CgsVoicePool.h as the coherent
// class home. mpVoices/muVoiceCount == mpaPooledVoices/muPooledVoiceCount (same
// +0x08/+0x0C offsets); IsPlaying's behaviour is unchanged.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsVoicePool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

// ----------------------------------------------------------------------------
// VoicePoolBase::IsPlaying  @ 0x82685A10
//   Scans the slot array for any voice whose state (+0x48) is "playing" (neither
//   0 == free nor 7 == stopped). Returns true on the first such voice.
//     count = muPooledVoiceCount;  if (!count) return false;
//     for each slot: if (state != 7 && state != 0) return true;
// ----------------------------------------------------------------------------
bool VoicePoolBase::IsPlaying() const
{
    const u32 luCount = muPooledVoiceCount;
    if (luCount == 0)
        return false;

    const u8* lpVoice = reinterpret_cast<const u8*>(mpaPooledVoices);
    for (u32 luIndex = 0; ; )
    {
        const s32 liState = *reinterpret_cast<const s32*>(lpVoice + KU_POOLED_VOICE_STATE_OFFSET);
        if (liState != KI_VOICE_STATE_STOPPED && liState != KI_VOICE_STATE_FREE)
            return true;
        if (++luIndex >= luCount)
            return false;
        lpVoice += KU_POOLED_VOICE_STRIDE;
    }
}

// ----------------------------------------------------------------------------
// VoicePoolBase::Prepare(lpaPooledVoices, luNumVoiceProxies)  @ 0x826B6528
//   Bind the caller-supplied PooledVoice array and reset every slot to a clean free
//   state. Asserts a non-zero count ('luNumVoiceProxies > 0') and non-null array
//   ('lapPooledVoices'). Per slot: zero the VoiceWrapper words (+0x04..+0x2C), set
//   name-field (+0x30) := -1, clear +0x44 / +0x48(state) / +0x4C, mfSecondaryGain
//   (+0x50) := 1.0f, muAge (+0x54) := 0, mbInUse (+0x58) := 0. Always returns true.
// FLAG: per-slot inits reach the DEFERRED VoiceWrapper (mVoice) at raw byte offsets
// exactly as the X360; offsets X360-attested store-for-store.
// ----------------------------------------------------------------------------
bool VoicePoolBase::Prepare(PooledVoice* lpaPooledVoices, u32 luNumVoiceProxies)
{
    CGS_ASSERT(luNumVoiceProxies != 0, "luNumVoiceProxies > 0");
    CGS_ASSERT(lpaPooledVoices, "lapPooledVoices");

    mpaPooledVoices = lpaPooledVoices;

    if (luNumVoiceProxies != 0)
    {
        u32 luRemaining  = luNumVoiceProxies;
        u32 luByteOffset = 0;
        do
        {
            --luRemaining;
            u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices) + luByteOffset;
            luByteOffset += KU_POOLED_VOICE_STRIDE;

            *reinterpret_cast<f32*>(lpVoice + 0x50) = 1.0f;   // mfSecondaryGain
            *reinterpret_cast<u8*>(lpVoice + 0x4C)  = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x44) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x48) = 0;      // state
            *reinterpret_cast<u32*>(lpVoice + 0x04) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x08) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x0C) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x10) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x14) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x18) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x1C) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x20) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x24) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x28) = 0;
            *reinterpret_cast<u32*>(lpVoice + 0x2C) = 0;
            *reinterpret_cast<s32*>(lpVoice + 0x30) = -1;
            *reinterpret_cast<u32*>(lpVoice + 0x54) = 0;      // muAge
            *reinterpret_cast<u8*>(lpVoice + 0x58)  = 0;      // mbInUse
        }
        while (luRemaining != 0);
    }

    muPooledVoiceCount = luNumVoiceProxies;
    return true;
}

// ----------------------------------------------------------------------------
// VoicePoolBase::Release  @ 0x826CF9C0
//   Tear the pool down: Release() every pooled voice's wrapper and clear its mbInUse
//   (+0x58) / muAge (+0x54), then unbind (muPooledVoiceCount := 0, mpaPooledVoices
//   := null). Always returns true.
// FLAG: VoiceWrapper::Release() is a member on the DEFERRED VoiceWrapper (mVoice) at
// PooledVoice+0x00.
// ----------------------------------------------------------------------------
bool VoicePoolBase::Release()
{
    u32 luIndex = 0;
    if (muPooledVoiceCount != 0)
    {
        u32 luByteOffset = 0;
        do
        {
            u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices) + luByteOffset;
            reinterpret_cast<VoiceWrapper*>(lpVoice)->Release();
            *reinterpret_cast<u8*>(lpVoice + KU_POOLED_VOICE_INUSE_OFFSET) = 0;
            *reinterpret_cast<u32*>(lpVoice + KU_POOLED_VOICE_AGE_OFFSET)  = 0;
            ++luIndex;
            luByteOffset += KU_POOLED_VOICE_STRIDE;
        }
        while (luIndex < muPooledVoiceCount);
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
// FLAG: state read (+0x48) + VoiceWrapper::Update() reach the DEFERRED VoiceWrapper
// (mVoice) at PooledVoice+0x00.
// ----------------------------------------------------------------------------
void VoicePoolBase::Update()
{
    u32 luIndex = 0;
    if (muPooledVoiceCount != 0)
    {
        u32 luByteOffset = 0;
        do
        {
            u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices) + luByteOffset;

            ++*reinterpret_cast<u32*>(lpVoice + KU_POOLED_VOICE_AGE_OFFSET);
            reinterpret_cast<VoiceWrapper*>(lpVoice)->Update();

            const s32 liState = *reinterpret_cast<const s32*>(lpVoice + KU_POOLED_VOICE_STATE_OFFSET);
            const bool lbActive = (liState != KI_VOICE_STATE_STOPPED) && (liState != KI_VOICE_STATE_FREE);
            if (!lbActive && *reinterpret_cast<const u8*>(lpVoice + KU_POOLED_VOICE_INUSE_OFFSET))
            {
                *reinterpret_cast<u32*>(lpVoice + KU_POOLED_VOICE_AGE_OFFSET)  = 0;
                *reinterpret_cast<u8*>(lpVoice + KU_POOLED_VOICE_INUSE_OFFSET) = 0;
            }

            ++luIndex;
            luByteOffset += KU_POOLED_VOICE_STRIDE;
        }
        while (luIndex < muPooledVoiceCount);
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
//
// FLAG: the state field (+0x48) and VoiceWrapper::Release live in the DEFERRED
// VoiceWrapper (CgsVoiceWrapper.*); reached as a member on PooledVoice+0x00. Offsets
// X360-attested.
// ----------------------------------------------------------------------------
PooledVoice* VoicePoolBase::GetFreeVoice()
{
    CGS_ASSERT(mpLogicModule, "mpLogicModule");

    u8* lpBest      = 0;
    u32 luOldestAge = 0;

    const u32 luCount = muPooledVoiceCount;
    if (luCount != 0)
    {
        u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices);
        for (u32 luIndex = 0; ; )
        {
            const s32 liState = *reinterpret_cast<const s32*>(lpVoice + KU_POOLED_VOICE_STATE_OFFSET);
            const bool lbInUse = (liState != KI_VOICE_STATE_STOPPED) && (liState != KI_VOICE_STATE_FREE);
            if (!lbInUse)
                return reinterpret_cast<PooledVoice*>(lpVoice);

            const u32 luAge = *reinterpret_cast<const u32*>(lpVoice + KU_POOLED_VOICE_AGE_OFFSET);
            if (luAge >= luOldestAge)
            {
                lpBest      = lpVoice;
                luOldestAge = luAge;
            }

            if (++luIndex >= luCount)
                break;
            lpVoice += KU_POOLED_VOICE_STRIDE;
        }
    }

    CGS_ASSERT(lpBest, "lpOldestPooledVoice");

    reinterpret_cast<VoiceWrapper*>(lpBest)->Release();
    *reinterpret_cast<u8*>(lpBest + KU_POOLED_VOICE_INUSE_OFFSET) = 0;
    *reinterpret_cast<u32*>(lpBest + KU_POOLED_VOICE_AGE_OFFSET)  = 0;
    return reinterpret_cast<PooledVoice*>(lpBest);
}

// ----------------------------------------------------------------------------
// VoicePoolBase::SetGain(liSendNameHash, lfGain, liReserved, lpSendName)  @ 0x8269A9D0
//   Broadcast a gain change to every PLAYING pooled voice (state @+0x48 == 6) whose
//   wrapped handle (+0x38) is live. Applied gain is scaled by the slot's
//   mfSecondaryGain (+0x50), then forwarded to the logic Voice sub-object (+0x34) via
//   Voice::SetGain with a stack copy of the send name.
// FLAG: state/handle/Voice reads reach the DEFERRED VoiceWrapper at X360-attested
// byte offsets. Voice::SetGain itself is a Playback-dependent stub (CgsVoice.cpp).
// ----------------------------------------------------------------------------
void VoicePoolBase::SetGain(s32 liSendNameHash, f32 lfGain, s32 liReserved, const u32* lpSendName)
{
    u32 luIndex = 0;
    if (muPooledVoiceCount != 0)
    {
        u32 luByteOffset = 0;
        do
        {
            u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices) + luByteOffset;
            const s32 liState  = *reinterpret_cast<const s32*>(lpVoice + KU_POOLED_VOICE_STATE_OFFSET);
            const void* lpObj  = *reinterpret_cast<void* const*>(lpVoice + KU_POOLED_VOICE_HANDLE_OFFSET);
            if (liState == KI_VOICE_STATE_PLAYING && lpObj != 0)
            {
                const f32 lfSecondaryGain = *reinterpret_cast<const f32*>(lpVoice + 0x50);
                const f32 lfScaledGain    = lfSecondaryGain * lfGain;
                u32 luSendNameCopy = *lpSendName;
                Voice* lpVoiceObj = reinterpret_cast<Voice*>(lpVoice + KU_POOLED_VOICE_VOICE_OFFSET);
                lpVoiceObj->SetGain(static_cast<u32>(liSendNameHash), lfScaledGain, liReserved, &luSendNameCopy);
            }
            ++luIndex;
            luByteOffset += KU_POOLED_VOICE_STRIDE;
        }
        while (luIndex < muPooledVoiceCount);
    }
}

// ----------------------------------------------------------------------------
// VoicePoolBase::SetParameter(liSendNameHash, lfValue, liReserved, lpSendName)  @ 0x826B6628
//   Broadcast a parameter change to every pooled voice whose wrapped handle (+0x38)
//   is live -- regardless of play state (contrast SetGain, which also gates on
//   state==PLAYING). Forwards to the logic Voice sub-object (+0x34) via
//   Voice::SetParameter with a stack copy of the send name and the raw value.
// FLAG: handle (+0x38) / Voice (+0x34) reach the DEFERRED VoiceWrapper at X360-attested
// offsets. Voice::SetParameter is a Playback-dependent stub (CgsVoice.cpp).
// ----------------------------------------------------------------------------
void VoicePoolBase::SetParameter(s32 liSendNameHash, f32 lfValue, s32 liReserved, const u32* lpSendName)
{
    u32 luIndex = 0;
    if (muPooledVoiceCount != 0)
    {
        u32 luByteOffset = 0;
        do
        {
            u8* lpVoice = reinterpret_cast<u8*>(mpaPooledVoices) + luByteOffset;
            const void* lpObj = *reinterpret_cast<void* const*>(lpVoice + KU_POOLED_VOICE_HANDLE_OFFSET);
            if (lpObj != 0)
            {
                u32 luSendNameCopy = *lpSendName;
                Voice* lpVoiceObj = reinterpret_cast<Voice*>(lpVoice + KU_POOLED_VOICE_VOICE_OFFSET);
                lpVoiceObj->SetParameter(static_cast<u32>(liSendNameHash), lfValue, liReserved, &luSendNameCopy);
            }
            ++luIndex;
            luByteOffset += KU_POOLED_VOICE_STRIDE;
        }
        while (luIndex < muPooledVoiceCount);
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
    Prepare(maVoices, N);
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
