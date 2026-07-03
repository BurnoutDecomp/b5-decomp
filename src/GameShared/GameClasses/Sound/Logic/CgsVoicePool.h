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
//     +0x00  mVoice                      VoiceWrapper  (opaque here -- DEFERRED)
//              within mVoice the bodies reach these X360-attested sub-offsets:
//                +0x34  the logic Voice sub-object (Voice::SetGain / SetParameter)
//                +0x38  the wrapped voice handle ptr (non-null == live)
//                +0x48  state word (0==free, 6==playing, 7==stopped)
//     +0x50  mfSecondaryGain             f32   (Prepare sets 1.0; SetGain multiplier)
//     +0x54  muAge                       u32   (bumped by Update, reset on retire)
//     +0x58  mbInUse                     bool
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): pointers widen on the 64-bit host, so
// members are pinned BY NAME + SEQUENCE; the X360 offsets are recorded in comments.
//
// FLAG: PooledVoice's internal VoiceWrapper (mVoice) layout is DEFERRED (home:
// CgsVoiceWrapper.*). The pool bodies reach the state/handle/Voice sub-object fields
// at raw byte offsets exactly as the X360 does. VoiceWrapper::Release()/Update() are
// invoked as members on the mVoice sub-object -- their decls live additively in
// CgsVoiceWrapper.h. Voice::SetParameter lives additively in CgsVoice.h. Nothing here
// is fabricated beyond those attested method calls.
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
// PooledVoice -- one slot in the pool. mVoice's internal layout is DEFERRED, so it is
// materialised here as a correctly-sized (X360-attested) opaque byte span up to +0x50
// so the trailing scalar members land at their true offsets and the struct is 0x5C.
// -----------------------------------------------------------------------------
struct PooledVoice
{
    u8  mVoice[0x50];       // [0x00] VoiceWrapper (opaque; state @+0x48, Voice @+0x34, handle @+0x38)
    f32 mfSecondaryGain;    // [0x50] per-voice gain multiplier (Prepare sets 1.0f)
    u32 muAge;              // [0x54] frames since last (re)use; ++ per Update
    u8  mbInUse;            // [0x58] slot currently allocated
    u8  mPad[3];            // [0x59] pad to the attested 0x5C stride
};

// -----------------------------------------------------------------------------
// PooledVoice raw sub-offsets the pool bodies reach through mVoice. X360-attested
// byte offsets into the DEFERRED VoiceWrapper; NOT named members.
// -----------------------------------------------------------------------------
static const u32 KU_POOLED_VOICE_STRIDE        = 0x5C;  // 92 bytes / slot
static const u32 KU_POOLED_VOICE_VOICE_OFFSET  = 0x34;  // logic Voice sub-object
static const u32 KU_POOLED_VOICE_HANDLE_OFFSET = 0x38;  // wrapped-voice handle ptr (live == non-null)
static const u32 KU_POOLED_VOICE_STATE_OFFSET  = 0x48;  // VoiceWrapper state word
static const u32 KU_POOLED_VOICE_AGE_OFFSET    = 0x54;  // muAge
static const u32 KU_POOLED_VOICE_INUSE_OFFSET  = 0x58;  // mbInUse

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

private:
    // +0x14. The N embedded pool slots (stride 0x5C). Bound into the base by the ctor
    // and released by the dtor.
    PooledVoice maVoices[N];
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICEPOOL_H
