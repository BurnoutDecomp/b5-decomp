// Compile-gate embed check for the princeton_digital reverb DSP family.
// Exercises every bodied ledger function across the explicit instantiations.

#include "SDKs/XAudio/princeton_digital.h"

namespace
{
using namespace princeton_digital;

f32 gSink = 0.0f;

template <typename Delay>
void touch_delay()
{
    Delay d{};
    d.mFillValue = 0.25f;
    d.reset();
    gSink += d.maBuffer[0] + static_cast<f32>(d.muIndex);
}

template <typename Allpass>
void touch_allpass()
{
    Allpass a{};
    a.mFeedbackGain = 0.5f;
    a.mFeedforwardGain = -0.5f;
    a.muIndex = 0;
    f32 io[256] = {};
    a.preprocess(io);
    gSink += io[0] + a.mState;
}

template <typename ThreeTap>
void touch_threetap()
{
    ThreeTap t{};
    t.mGainA = 0.5f;
    t.mGainB = 0.25f;
    t.mGainC = 0.1f;
    t.muIndex = 0;
    f32 in[256] = {};
    f32 outA[257] = {};
    f32 outB[257] = {};
    t.preprocess3(in, outA, outB);
    gSink += outA[1] + outB[1];
}

template <typename VarDelay>
void touch_vardelay()
{
    VarDelay v{};
    v.mMix = 0.0f;
    v.mTapNew = 4;
    v.mTapNew2 = 2;
    v.mTapOld = 3;
    f32 in[256] = {};
    f32 out[257] = {};
    v.preprocess(in, out);
    gSink += out[0] + v.mState;
}

void touch_occlusion()
{
    occlusion_t<f32, 2> o{};
    o.mCoefA = 0.9f;
    o.mCoefB = 1.0f;
    f32 in[8] = {};
    f32 out[9] = {};
    o.preprocess(in, out, 8);
    gSink += out[0] + o.mState0 + o.mState1;
}
} // namespace

extern "C" int princeton_digital_embed_check_main()
{
    // nearest_prime
    gSink += static_cast<f32>(nearest_prime(0));
    gSink += static_cast<f32>(nearest_prime(1));
    gSink += static_cast<f32>(nearest_prime(50));
    gSink += static_cast<f32>(nearest_prime(9973));
    gSink += static_cast<f32>(nearest_prime(100000));

    touch_delay<delay_t<f32, 128>>();
    touch_delay<delay_t<f32, 256>>();
    touch_delay<delay_t<f32, 512>>();
    touch_delay<delay_t<f32, 1024>>();
    touch_delay<delay_t<f32, 2048>>();
    touch_delay<delay_t<f32, 4096>>();
    touch_delay<delay_t<f32, 16384>>();

    touch_allpass<allpass_t<f32, 128>>();
    touch_allpass<allpass_t<f32, 256>>();
    touch_allpass<allpass_t<f32, 512>>();

    touch_threetap<threetap_t<f32, 512>>();
    touch_threetap<threetap_t<f32, 2048>>();

    touch_vardelay<vardelay_t<f32, 256>>();
    touch_vardelay<vardelay_t<f32, 16384>>();

    touch_occlusion();

    stereo_room_t<f32>::properties_t props;
    gSink += props.f13 + static_cast<f32>(props.a0) + props.f18 + props.f19;

    stereo_room_t<f32> room;
    room.wet_dry_mix_set(75.0f);
    room.input_mode_set(2);
    gSink += room.mWetDryMix + static_cast<f32>(room.mInputMode);

    return static_cast<int>(gSink);
}
