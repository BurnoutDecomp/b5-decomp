// =============================================================================
// CgsVoicePool.h -- CgsSound::Logic::VoicePoolBase + PooledVoice + VoicePool<N>.
//   GameShared/GameClasses/Sound/Logic/CgsVoicePool.h  (X360 source path, from the
//   FireAssert 2nd args: "..\\..\\..\\GameShared\\GameClasses\\Sound/Logic/CgsVoicePool.h")
//   + GameShared/GameClasses/Sound/Logic/CgsVoicePool.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. VoicePoolBase manages an array of
// PooledVoice slots (VoiceWrapper + secondary-gain + age + in-use bookkeeping). It
// hands out reusable voices (GetFreeVoice), broadcasts gain/param changes (SetGain /
// SetParameter), ages/retires idle slots each frame (Update), and binds/unbinds its
// slot array (Prepare / Release). Used by sound effect leaves (e.g.
// BrnSound::Vehicles::Wheels::InAirEffect). VoicePool<N> is the concrete pool that
// embeds the N-slot array and forwards Prepare/Release to the base.
//
// NOTE: the previously-committed CgsVoicePool.cpp carried an INLINE anonymous
// VoicePoolBase{mPad[8]; mpVoices; muVoiceCount} + IsPlaying only. This header
// promotes that to the coherent class home; the .cpp now includes this header and
// drops its local struct. mpVoices/muVoiceCount here == mpaPooledVoices/
// muPooledVoiceCount (same +0x08 / +0x0C offsets); IsPlaying keeps working unchanged.
//
// -----------------------------------------------------------------------------
// X360 LAYOUT (32-bit guest, from the reconstructed bodies):
//   VoicePoolBase:
//     +0x00  vptr                        (virtual: IsPlaying is virtual)
//     +0x04  mpLogicModule               Module*  (asserted non-null in GetFreeVoice)
//     +0x08  mpaPooledVoices             PooledVoice*  (base of the slot array)
//     +0x0C  muPooledVoiceCount          u32
//     +0x10  muDebugFrameIndex           u32       (++ at end of Update)
//
//   PooledVoice (stride 0x5C == 92 bytes, from the +0x5C loop increment in all six
//   bodies):
//     +0x00  mWrapper                    VoiceWrapper (console 0x50; REAL layout by
//              name in CgsVoiceWrapper.h -- the logic Voice @+0x34, live-handle test
//              @+0x38, state word @+0x48 the bodies use are its named members now)
//     +0x50  mfSecondaryGain             f32   (Prepare sets 1.0; SetGain multiplier)
//     +0x54  muAge                       u32   (bumped by Update, reset on retire)
//     +0x58  mbInUse                     bool
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): pointers widen on the 64-bit host, so
// members are pinned BY NAME + SEQUENCE; the X360 offsets are recorded in comments.
//
// (2026-08-25 wave 4: the former `u8 mVoice[0x50]` opaque byte model + the
// KU_POOLED_VOICE_*_OFFSET raw-reach constants are RETIRED -- VoiceWrapper's real
// 0x50 layout is modelled by name in CgsVoiceWrapper.h and the pool bodies go
// through its named members/accessors.)
// =============================================================================

#ifndef CGS_SOUND_LOGIC_CGSVOICEPOOL_H
#define CGS_SOUND_LOGIC_CGSVOICEPOOL_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"          // CgsSound::Logic::Voice
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"   // CgsSound::Logic::VoiceWrapper

namespace CgsSound
{
namespace Logic
{

// Forward-declared owning module (only ever held as a pointer here).
class Module;

// -----------------------------------------------------------------------------
// PooledVoice -- one slot in the pool: the real VoiceWrapper (console 0x50, modelled
// by name in CgsVoiceWrapper.h) plus the pool bookkeeping scalars. Console stride
// 0x5C == 0x50 + 4 + 4 + 1(+pad3), matching the +0x5C loop increment in all six
// pool bodies.
// -----------------------------------------------------------------------------
struct PooledVoice
{
    VoiceWrapper mWrapper;          // [0x00] the per-slot voice wrapper (0x50 console)
    f32          mfSecondaryGain;   // [0x50] per-voice gain multiplier (Prepare sets 1.0f)
    u32          muAge;             // [0x54] frames since last (re)use; ++ per Update
    u8           mbInUse;           // [0x58] slot currently allocated (pad to the 0x5C stride)
};

// VoiceWrapper state values the pool tests.
static const s32 KI_VOICE_STATE_FREE    = 0;
static const s32 KI_VOICE_STATE_PLAYING = 6;
static const s32 KI_VOICE_STATE_STOPPED = 7;

// -----------------------------------------------------------------------------
// VoicePoolBase -- the base owning the (bind-time supplied) slot array.
// -----------------------------------------------------------------------------
class VoicePoolBase
{
public:
    VoicePoolBase()
        : mpLogicModule(nullptr)
        , mpaPooledVoices(nullptr)
        , muPooledVoiceCount(0)
        , muDebugFrameIndex(0)
    {
    }

    // Bind + reset a caller-supplied PooledVoice array. @ 0x826B6528.
    bool Prepare(PooledVoice* lpaPooledVoices, u32 luNumVoiceProxies);

    // Release every wrapper and unbind the array. @ 0x826CF9C0.
    bool Release();

    // Per-frame tick: age + retire idle slots, bump the debug frame counter. @ 0x826E5280.
    void Update();

    // Hand out a free/stopped slot, or evict the oldest in-use one. @ 0x826CFA28.
    PooledVoice* GetFreeVoice();

    // Broadcast gain (scaled by each slot's mfSecondaryGain) to PLAYING slots. @ 0x8269A9D0.
    void SetGain(s32 liSendNameHash, f32 lfGain, s32 liReserved, const u32* lpSendName);

    // Broadcast a parameter to every live slot. @ 0x826B6628.
    void SetParameter(s32 liSendNameHash, f32 lfValue, s32 liReserved, const u32* lpSendName);

    // true iff any slot is currently playing. @ 0x82685A10 (committed in CgsVoicePool.cpp).
    virtual bool IsPlaying() const;

protected:
    Module*      mpLogicModule;       // +0x04  owning logic module (asserted by GetFreeVoice)
    PooledVoice* mpaPooledVoices;     // +0x08  base of the bound slot array
    u32          muPooledVoiceCount;  // +0x0C  slot count
    u32          muDebugFrameIndex;   // +0x10  bumped once per Update()
};

// -----------------------------------------------------------------------------
// VoicePool<N> -- the concrete pool: it embeds the N-slot PooledVoice array and,
// on construction, binds it into the base (Prepare), tearing it down (Release) at
// destruction. The X360 emits VoicePool<4>::VoicePool<4> @ 0x826E5328 and
// ~VoicePool<4> @ 0x826E5370 (embedded by BrnSound::Vehicles::Wheels::InAirEffect).
// The ctor/dtor bodies live in CgsVoicePool.cpp (explicit N==4 instantiation).
// -----------------------------------------------------------------------------
template <u32 N>
class VoicePool : public VoicePoolBase
{
public:
    VoicePool();            // @ 0x826E5328 for N==4  (binds maVoices via Prepare)
    virtual ~VoicePool();   // @ 0x826E5370 for N==4  (Release + per-slot teardown)

    // InAirEffect::Attach supplies the owning logic module after the effect has
    // been prepared.  The console stores that pointer in VoicePoolBase and then
    // re-prepares the embedded slots in one operation.
    bool Prepare(Module* apLogicModule)
    {
        mpLogicModule = apLogicModule;
        return VoicePoolBase::Prepare(maVoices, N);
    }

private:
    // +0x14. The N embedded pool slots (stride 0x5C). Bound into the base by the ctor
    // and released by the dtor.
    PooledVoice maVoices[N];
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICEPOOL_H
