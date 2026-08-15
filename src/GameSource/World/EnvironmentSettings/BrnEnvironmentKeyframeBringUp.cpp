// =============================================================================
// BrnEnvironmentKeyframeBringUp.cpp  (GameSource/World/EnvironmentSettings)
//
// [FLAG PC bring-up] THE SHIPPED NOON KEYFRAME, EMBEDDED -- so the WORLD effects layer carries the
// game's own daytime bloom / vignette / tint while the environment manager's streamer is not live.
//
// WHAT THIS STANDS IN FOR. On the console EnvironmentManager::Prepare @0x827D49A8 streams the season's
// timeline + keyframes (BrnEnvironmentTimeLineResourceType / BrnEnvironmentKeyframeResourceType, from
// ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE) and Update @0x827D6060 -> SetupBlend / PerformBlend
// fills EnvironmentManager::mBlendFrame (the four bracketing keyframes + weights) from the time of day.
// On this build both are inert gates (WorldLinkStubs.cpp), so mBlendFrame stays empty and
// GenerateEffects @0x827BE698 -- REAL, and the only consumer -- takes its no-keyframe arm (weight 0),
// which leaves the base layer's fallback asset (bloom 1.8, the kv*Def* vignette) on screen. That is
// the console's "environment disabled" picture, not its in-game one.
//
// WHAT IT IS. The 0x240-byte EnvironmentKeyframe resource ENV_KF_Paradise_ingame_junk_city_1200
// (id 0x7A9D3780) from build/game/ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE (the platform-4
// conversion; byte-identical to the word-swapped X360 resource in PARADISE_INGAME_JUNK.BUNDLE.x360 --
// checked 2026-08-15), copied verbatim as 144 little-endian words. Recipe: bnd2 header -> resource
// entry 2 -> zlib -> 576 B; the resource IS the serialised Keyframe (version word 8 at +0, then the
// sub-blocks BrnEnvironmentKeyframe.h names). Decoded, for the reader:
//   bloom lum 1.13 thr 0.56 scale (0.95844 0.93089 0.85513 1); vignette angle 0 sharpness 0.27 amount (0.3 0.94) centre (0.5 0.55) inner (0.8865 0.9797 0.8692) outer (0.2953 0.6083 0.7315); colour-cube id 0x00000000
// This is the SAME keyframe WorldModule::PublishWorldShadingConstantsBringUp already hard-codes the
// key light / irradiance / scattering from, so lighting and post-fx agree on the time of day.
//
// DELETE-WHEN EnvironmentManager::Prepare + Update are reconstructed and the timeline streams; the
// staging call in WorldModule::GenerateDispatchListsBringUp goes with it.
// =============================================================================
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframeBringUp.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{

namespace
{
    // The resource bytes, 16-aligned so the Vector4 members inside the sub-blocks are addressable.
    alignas(16) const u32 kauKeyframeCity1200[0x240 / 4] =
    {
    0x00000008u, 0x40061500u, 0x7C80C277u, 0x0080C277u, 0x3F90A3D7u, 0x3F0F5C29u, 0x18421500u, 0x78011500u,   // +0x000
    0x3F755C53u, 0x3F6E4EE1u, 0x3F5AE9DFu, 0x3F800000u, 0x00000000u, 0x3E8A3D71u, 0x08000000u, 0x44934100u,   // +0x020
    0x3E99999Au, 0x3F70A3D7u, 0x00000000u, 0x00000000u, 0x3F000000u, 0x3F0CCCCDu, 0x00000000u, 0x00000000u,   // +0x040
    0x3F62F249u, 0x3F7ACEEEu, 0x3F5E8657u, 0x00000000u, 0x3E972C3Eu, 0x3F1BBBD3u, 0x3F3B4395u, 0x3F800000u,   // +0x060
    0x00000000u, 0x65006400u, 0xC04E8700u, 0x0A000000u, 0x3D59F73Du, 0x3ED42938u, 0x3F37A248u, 0x00000000u,   // +0x080
    0x3F81F4A1u, 0x3F61FD6Du, 0x3F4EA1BCu, 0x00000000u, 0x3F8020C5u, 0x3F8020C5u, 0x3F7A1E4Eu, 0x00000000u,   // +0x0A0
    0x3F000000u, 0x4151999Au, 0x00000000u, 0x40A00000u, 0x4089999Au, 0x40D00000u, 0x6D05917Cu, 0xFCFE1200u,   // +0x0C0
    0x00000000u, 0x3DCCCCCDu, 0x3E99999Au, 0x00000000u, 0x3EC49236u, 0x3EDFFDFCu, 0x3F06E979u, 0x00000000u,   // +0x0E0
    0x3F800000u, 0x3F800000u, 0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3C23D70Au,   // +0x100
    0x3C23D70Au, 0x3C23D70Au, 0x41C80000u, 0x44BB8000u, 0x3F800000u, 0x3F5EB852u, 0x30000000u, 0x00100000u,   // +0x120
    0x3FD9999Au, 0x3FD9999Au, 0x3F86E978u, 0x00000000u, 0x3FCBC16Au, 0x3FA4AF37u, 0x3F744405u, 0x00000000u,   // +0x140
    0x3F59C4C9u, 0x3F78FEEFu, 0x3F753431u, 0x00000000u, 0x3EC9EA81u, 0x3F09D68Cu, 0x3F0DE00Du, 0x00000000u,   // +0x160
    0x3F20655Bu, 0x3F3634E1u, 0x3F397247u, 0x00000000u, 0x3F23FEB1u, 0x3F397247u, 0x3F38FBFEu, 0x00000000u,   // +0x180
    0x3E83126Eu, 0x3E7CBA1Au, 0x3E6C8DE3u, 0x00000000u, 0x3F3165C1u, 0x3F5695BCu, 0x3F6505D1u, 0x00000000u,   // +0x1A0
    0x3ECCCCCDu, 0x3D8B1378u, 0x995E8F13u, 0x1DB14813u, 0x3E529DC7u, 0x3E529DC7u, 0x3E529DC7u, 0x00000000u,   // +0x1C0
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x3F7C38E8u, 0x3F6C8B44u, 0x00000000u,   // +0x1E0
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x3F8CCCCDu, 0x3DCCCCCDu,   // +0x200
    0x3F000000u, 0x00000000u, 0x4089999Au, 0x40C00000u, 0x3F800000u, 0x45DAC000u, 0x00000000u, 0xFFFFFFFFu,   // +0x220
    };
    static_assert(sizeof(kauKeyframeCity1200) == sizeof(Keyframe), "the embedded keyframe must be exactly one serialised Keyframe");
}

const Keyframe& GetBringUpKeyframeCity1200()
{
    // The struct is plain data (floats / u32 / 16-byte vectors -- BrnEnvironmentKeyframe.h pins every
    // sub-block size and the whole 0x240), so the serialised bytes ARE the object, exactly as the
    // resource type hands them to the manager on the console.
    return *reinterpret_cast<const Keyframe*>(kauKeyframeCity1200);
}

}
}
