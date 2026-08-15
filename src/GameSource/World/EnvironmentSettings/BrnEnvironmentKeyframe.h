#ifndef BRN_ENVIRONMENT_KEYFRAME_H
#define BRN_ENVIRONMENT_KEYFRAME_H

#include "types.hpp"

#include "SharedClasses/Graphics/BrnEffectsData.h"                       // BrnEffects::BloomData / VignetteData
#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"

// ============================================================================
// GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h
//
// BrnWorld::EnvironmentSettings::Keyframe -- the runtime, 0x240-byte environment
// keyframe (the in-memory form of the 0x10012 "Environment Settings Keyframe"
// resource; its serialised size is the 0x240 the KeyframeResourceType descriptor
// reports). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Only the sub-blocks that this batch's EnvironmentManager code actually touches
// are named, at their asm-attested offsets; the intervening regions are explicit
// padding (layout-recovery-with-padding). The named offsets are cross-validated
// two ways:
//   * EnvironmentManager::SetupUpdateFromToolBlend (@0x827C5018) builds a Keyframe
//     on the stack and passes &kf[16]/&kf[48]/&kf[144]/&kf[320]/&kf[464] to
//     ParseEnvironmentFile as the Bloom / Vignette / Scattering / Lighting /
//     Clouds out-params (r-offsets 0x10 / 0x30 / 0x90 / 0x140 / 0x1D0).
//   * EnvironmentManager::PerformBlend (@0x827B0EB8) reads each source keyframe's
//     scattering(+0x90) / lighting(+0x140) / clouds(+0x1D0) sub-block.
//   * EnvironmentManager::GenerateEffects (@0x827BE698) reads the tint COLOUR-CUBE id
//     at +0x80 (`lwz r11, 0x80(kf)` -> `stw r11, 0x110(effectsFrame)`, i.e. straight
//     into BrnEffectsFrame::mTintData::muColourCube). NAMED 2026-08-15 (bloom wave).
// The +0x00..+0x10 header, the rest of the +0x80..+0x90 gap (DoF/blur, deferred) and the
// small inter-block gaps are opaque padding rather than fabricated members.
// ============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{

struct Keyframe
{
    // @ external (Keyframe::Construct, called by SetupUpdateFromToolBlend); its body
    // is its own TU, not reconstructed in this batch. Sets every sub-block to its
    // default template.
    void Construct();

    u8                        mPad0[0x10];    // 0x000  version/header (deferred)
    BrnEffects::BloomData     mBloom;         // 0x010  (0x20)
    BrnEffects::VignetteData  mVignette;      // 0x030  (0x50)
    // 0x080  the keyframe's tint colour-cube resource id -- copied verbatim into the
    // effects frame's BrnEffects::TintData by GenerateEffects @0x827BE698 (see the
    // banner above). The remaining 0xC bytes are the still-deferred DoF/blur sub-blocks.
    u32                       muColourCube;   // 0x080
    u8                        mPad84[0xC];    // 0x084  DoF/blur sub-blocks (deferred)
    ScatteringData            mScattering;    // 0x090  (0xA8)
    u8                        mPad138[0x8];   // 0x138
    LightingData              mLighting;      // 0x140  (0x84)
    u8                        mPad1C4[0xC];   // 0x1C4
    CloudsData                mClouds;        // 0x1D0  (0x6C)
    u8                        mPad23C[0x4];   // 0x23C
};

// The pad arithmetic above depends on the exact sizeof of each embedded sub-block;
// pin them so a future layout change to those types breaks here loudly.
static_assert(sizeof(BrnEffects::BloomData)    == 0x20, "BloomData size drift breaks Keyframe layout");
static_assert(sizeof(BrnEffects::VignetteData) == 0x50, "VignetteData size drift breaks Keyframe layout");
static_assert(sizeof(ScatteringData)           == 0xA8, "ScatteringData size drift breaks Keyframe layout");
static_assert(sizeof(LightingData)             == 0x84, "LightingData size drift breaks Keyframe layout");
static_assert(sizeof(CloudsData)               == 0x6C, "CloudsData size drift breaks Keyframe layout");
static_assert(sizeof(Keyframe)                 == 0x240, "Keyframe serialised size must be 0x240");

}
}

#endif // BRN_ENVIRONMENT_KEYFRAME_H
