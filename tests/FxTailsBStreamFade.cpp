// FX-TAILS-B (crash parity 2026-09-24, item 3): StreamingEffect::Detach @0x826EEA68 against the
// ARTIST machine code -- the fade-out of a stopped music / speech / presentation stream.
//
//   case 0 (0x826EEAC0..0x826EEAE4)  mfTimeThroughFade = 0.0; mfGainPreFade = VoiceWrapper::GetGain of
//                                    the "Send01" send (dword_8300A6E0 = MakeHash("Send01"), CRT
//                                    0x82C61A18..0x82C61A34)
//   case 1 (0x826EEAE8..0x826EEBE8)  both clocks += dt; f = t / GetFadeOut() (`fdivs`, no guard),
//                                    `fneg ; fsel` + `fsubs ; fsel` clamp (NaN -> 1.0); gain =
//                                    GetOutput(1 - f, E_ONE_MINUS_EQPWR) * pre-fade -- inlined:
//                                    `fmsubs x*511 - 511` (flt_820AA7B0), `fctiwz`, `slwi 2 ; subf` into
//                                    gafArraySinTable (0x82F2D920), `fsubs 1.0 - entry`, `fmuls * pre`;
//                                    SetGain(the stream's send index / name); GetFadeOut() again;
//                                    t < fade -> false, else VoiceWrapper::Release
//   case 2 (0x826EEBEC..0x826EEBFC)  mbBufferReleased -> case 3, else false
//   case 3 (0x826EEC00..0x826EEC1C)  BrnEffectObject::Detach
//
// run_fxtailsb_stream_fade.py extracts the PRODUCTION StreamingEffect::Detach (BrnStreamingEffect.cpp)
// and the production Curve::GetOutput with its table, constant and read helper (CgsSoundUtils.cpp),
// and compiles them here against a fixture effect / voice. The expected gains are computed from the
// IMAGE's table words (x360rd 0x82F2D920 + 4i), independent of the production table.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "rw/math/fpu/scalar_operation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSound { namespace Utils {
#include "fxtailsb_curve_bodies.inc"
} }

// ---- the fixture name hash and stream diag (the witness is off) ------------------------------------
namespace CgsSound { namespace Playback {
class Name
{
public:
    static uintptr_t MakeHash(const char* lpcName)
    {
        u32 lu = 2166136261u;
        for (const char* lpc = lpcName; *lpc; ++lpc)
            lu = (lu ^ static_cast<u8>(*lpc)) * 16777619u;
        return lu;
    }
};
} }
namespace CgsSound { namespace Diag {
inline bool StreamDiagEnabled() { return false; }
inline void StreamDiagPrintf(const char*, ...) {}
} }

// ---- the fixture effect -----------------------------------------------------------------------------
namespace BrnSound { namespace Logic {
// What Detach reaches through its bases: EffectBase's detach state and frame step, and
// BrnEffectObject::Detach (case 3).
struct BrnEffectObject
{
    enum EDetachState
    {
        E_DETACH_STATE_NONE     = 0,
        E_DETACH_STATE_BEGIN    = 1,
        E_DETACH_STATE_UPDATING = 2,
        E_DETACH_STATE_FINISHED = 3,
    };
    EDetachState meDetachState = E_DETACH_STATE_NONE;
    f32 mfDeltaTime = 0.0f;
    int miBaseDetachCalls = 0;
    bool mbBaseDetachResult = true;
    bool Detach() { ++miBaseDetachCalls; return mbBaseDetachResult; }
};

namespace Streaming {

struct VoiceFixture
{
    f32 mfGain = 0.8f;                   // what GetGain answers
    mutable s32 miGetGainName = 0;
    mutable int miGetGainCalls = 0;
    u32 muSetIndex = 0xFFFFFFFFu;
    u32 muSetName = 0;
    f32 mfSetGain = -1.0f;
    int miSetGainCalls = 0;
    int miReleaseCalls = 0;
    f32 GetGain(const s32* lpName) const { ++miGetGainCalls; miGetGainName = *lpName; return mfGain; }
    void SetGain(u32 luIndex, f32 lfGain, const u32* lpName)
    {
        ++miSetGainCalls; muSetIndex = luIndex; mfSetGain = lfGain; muSetName = *lpName;
    }
    void Release() { ++miReleaseCalls; }
};

struct CreateParamsFixture
{
    u32 mContentSpecName = 0x12345678u;
    u32 mSendName = 0;
    s32 miSendIndex = -1;
};

struct StreamingEffect : public BrnEffectObject
{
    CreateParamsFixture mCreateParams;
    VoiceFixture mVoice;
    f32 mfElapsedTime = 0.0f;
    f32 mfTimeThroughFade = 0.0f;
    f32 mfGainPreFade = 0.0f;
    bool mbBufferReleased = false;
    f32 mfFadeOut = 1.0f;
    mutable int miFadeOutCalls = 0;
    f32 GetFadeOut() const { ++miFadeOutCalls; return mfFadeOut; }
    bool Detach();
};

#include "fxtailsb_stream_detach.inc"

} } }

// ---- checks ----------------------------------------------------------------------------------------
using BrnSound::Logic::Streaming::StreamingEffect;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}
static f32 F(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

// gafArraySinTable entries the fade reaches, the image's words (0x82F2D920 + 4i).
static const f32 KF_T127 = F(0x3EC27BB3u);   // 0.37985
static const f32 KF_T255 = F(0x3F34764Bu);   // 0.70493
static const f32 KF_T383 = F(0x3F6C3611u);   // 0.92270

static StreamingEffect MakeEffect(f32 lfFade, f32 lfDt)
{
    StreamingEffect l;
    l.mfFadeOut = lfFade;
    l.mfDeltaTime = lfDt;
    l.mfTimeThroughFade = 99.0f;      // stale: case 0 must reset it
    l.mfElapsedTime = 10.0f;
    l.mCreateParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send02"));
    l.mCreateParams.miSendIndex = 3;
    return l;
}

int main()
{
    const u32 kuSend01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 kuSend02 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send02"));

    // ---- a 1.0 s fade in four 0.25 s frames -------------------------------------------------------
    StreamingEffect lEffect = MakeEffect(1.0f, 0.25f);
    const bool lbFrame1 = lEffect.Detach();
    Check(lEffect.mVoice.miGetGainCalls == 1 && static_cast<u32>(lEffect.mVoice.miGetGainName) == kuSend01,
          "case 0: the pre-fade gain is read from the \"Send01\" send (dword_8300A6E0), not the stream's own send name");
    Check(lEffect.mfGainPreFade == 0.8f, "case 0: mfGainPreFade = what GetGain answers");
    Check(lEffect.mfTimeThroughFade == 0.25f && lEffect.mfElapsedTime == 10.25f,
          "case 0 -> 1: the fade clock restarts at 0.0 and both clocks advance by dt");
    Check(Bits(lEffect.mVoice.mfSetGain) == Bits((1.0f - KF_T127) * 0.8f) && !lbFrame1,
          "f = 0.25: gain = (1 - sin table[127]) * pre = 0.49612 (the equal-power fade; linear would be 0.6)");
    Check(lEffect.mVoice.muSetIndex == 3u && lEffect.mVoice.muSetName == kuSend02,
          "case 1: the gain goes to the stream's own send (index +0x60, name +0x50)");
    Check(lEffect.miFadeOutCalls == 2, "case 1: GetFadeOut is read twice per frame (0x826EEB14, 0x826EEBD4)");
    lEffect.Detach();
    Check(Bits(lEffect.mVoice.mfSetGain) == Bits((1.0f - KF_T255) * 0.8f),
          "f = 0.5: gain = (1 - sin table[255]) * pre = 0.23606 (linear would be 0.4)");
    lEffect.Detach();
    Check(Bits(lEffect.mVoice.mfSetGain) == Bits((1.0f - KF_T383) * 0.8f) && lEffect.mVoice.miReleaseCalls == 0,
          "f = 0.75: gain = (1 - sin table[383]) * pre = 0.06184 (linear would be 0.2), voice kept");
    const bool lbFrame4 = lEffect.Detach();
    Check(lEffect.mVoice.mfSetGain == 0.0f && lEffect.mVoice.miReleaseCalls == 1 && !lbFrame4 &&
              lEffect.meDetachState == StreamingEffect::E_DETACH_STATE_UPDATING,
          "f = 1: gain 0 (table[511] = 1.0), the voice released once, waiting for the buffer");
    Check(guAsserts == 0, "no GetOutput range assert over a normal fade");
    const bool lbWait = lEffect.Detach();
    Check(!lbWait && lEffect.miBaseDetachCalls == 0, "case 2: no buffer release yet -> false, no base detach");
    lEffect.mbBufferReleased = true;
    const bool lbDone = lEffect.Detach();
    Check(lbDone && lEffect.miBaseDetachCalls == 1 && lEffect.meDetachState == StreamingEffect::E_DETACH_STATE_FINISHED,
          "case 2 -> 3: buffer released -> BrnEffectObject::Detach, its result returned");

    // ---- the clamp and the missing guard -----------------------------------------------------------
    {
        // A negative fade length: t / fade < 0 clamps to 0 -> x = 1 -> table[0] = 0 -> the send keeps
        // the pre-fade gain (the PC's `fade > 0 ? .. : 1.0` guard silenced it).
        StreamingEffect l = MakeEffect(-1.0f, 0.25f);
        l.Detach();
        Check(l.mVoice.mfSetGain == 0.8f && l.mVoice.miReleaseCalls == 1,
              "negative fade length: no guard -- f clamps to 0, the gain stays at the pre-fade 0.8, then release");
    }
    {
        // A NaN step: t = NaN, f = NaN -> the fsel pair gives 1.0 -> gain 0 (std::min/max gave 0 -> pre).
        StreamingEffect l = MakeEffect(1.0f, std::numeric_limits<f32>::quiet_NaN());
        l.Detach();
        Check(l.mVoice.mfSetGain == 0.0f && l.mVoice.miReleaseCalls == 1,
              "NaN fade position: the fsel clamp makes it 1.0 -> gain 0; `blt` on NaN goes on to release");
    }
    {
        // A zero fade length: t / 0 = +inf -> 1.0 -> gain 0, released at once (both bodies agree).
        StreamingEffect l = MakeEffect(0.0f, 0.25f);
        l.Detach();
        Check(l.mVoice.mfSetGain == 0.0f && l.mVoice.miReleaseCalls == 1,
              "zero fade length: +inf clamps to 1.0 -> gain 0, released at once");
    }

    std::printf("FxTailsBStreamFade: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
