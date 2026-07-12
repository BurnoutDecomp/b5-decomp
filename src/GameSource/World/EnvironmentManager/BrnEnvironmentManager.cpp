#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"  // ParseEnvironmentFile
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::EnvironmentManager::UpdateFromTool             @ 0x827B0DA8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::DiscardCurrSeason          @ 0x827B0E50
//   BrnWorld::EnvironmentSettings::EnvironmentManager::ResourcePointerAssertThingy@ 0x827C30E8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::PerformBlend               @ 0x827B0EB8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::SetupUpdateFromToolBlend   @ 0x827C5018
//
// The baked asserts fire the plain-string Begin/Fire/End sequence (rendered here as
// CGS_ASSERT); the X360-baked file/line are discarded per project convention. The typo
// "Tyring" is reproduced verbatim from the ARTIST rodata.

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x827B0DA8. Tool-driven blend/pause transition.
//   miBlendState >= 3  -> already in a blocking op: on unpause (lbPause == false) restore
//                         the saved state (asserting the state really is the reserved 3),
//                         and report the blocking state (true).
//   miBlendState <  3  -> on pause (lbPause == true) stash the current state, enter the
//                         reserved blocking state 3, and reset the tool-update frame gate;
//                         report not-blocking (false).
bool EnvironmentManager::UpdateFromTool(bool lbPause)
{
    if (miBlendState >= 3)
    {
        if (!lbPause)
        {
            CGS_ASSERT(miBlendState == 3, "Tyring to unpause a blocking op");
            miBlendState = miSavedBlendState;
        }
        return true;
    }

    if (lbPause)
    {
        miSavedBlendState        = miBlendState;
        miBlendState             = 3;
        miToolUpdateFrameCounter = 0;
    }
    return false;
}

// @ 0x827B0E50. Commit the pending season swap once the stream-in has finished.
void EnvironmentManager::DiscardCurrSeason()
{
    CGS_ASSERT(meStreamInStage == E_STREAMIN_DONE, "meStreamInStage == E_STREAMIN_DONE");

    const u32 luDiscardSeason = muDiscardSeason;
    muCurrSeasonRef = 0;
    mbCurrSeason    = static_cast<u8>(luDiscardSeason);
}

// @ 0x827C30E8. An out-of-line copy of CgsResource::ResourcePtr<T>::GetMemoryResource()
// (CgsResourcePtr.h line 581) that the X360 build emitted separately for the manager's
// per-season keyframe resource pointers: assert the main-memory resource is non-null,
// then return it. The X360 asm reads offset 0 of the passed ResourcePtr and returns it
// after the null-assert -- exactly GetMemoryResource() inlined -- so forwarding to it is
// behaviourally identical (no offset hack, no added behaviour). The plain-string assert
// message matches the baked "Can not instance resource pointer ..." rodata.
Keyframe* EnvironmentManager::ResourcePointerAssertThingy(
        CgsResource::ResourcePtr<Keyframe>& lrResourcePtr)
{
    return lrResourcePtr.GetMemoryResource();
}

// @ 0x827B0EB8. Blend the four source keyframes of a blend frame into the manager's
// current-environment sub-blocks. Each of scattering / lighting / clouds is the
// per-element weighted sum A0*w0 + A1*w1 + A2*w2 + A3*w3 of the four sources'
// corresponding sub-block (the four keyframes are the bracketing time-of-day/season
// pair pairs; the weights come from the blend frame). The X360 loads all four weights
// fresh for each sub-block call; the interleaved (value, weight) argument order matches
// the four GPR (value) + four FPR (weight) register split in the asm.
void EnvironmentManager::PerformBlend(BlendFrame& lrBlendFrame)
{
    const Keyframe& lrK0 = *lrBlendFrame.mapKeyframes[0];
    const Keyframe& lrK1 = *lrBlendFrame.mapKeyframes[1];
    const Keyframe& lrK2 = *lrBlendFrame.mapKeyframes[2];
    const Keyframe& lrK3 = *lrBlendFrame.mapKeyframes[3];

    const float lfW0 = lrBlendFrame.mafWeights[0];
    const float lfW1 = lrBlendFrame.mafWeights[1];
    const float lfW2 = lrBlendFrame.mafWeights[2];
    const float lfW3 = lrBlendFrame.mafWeights[3];

    mScattering.SetToBlend(lrK0.mScattering, lfW0, lrK1.mScattering, lfW1,
                           lrK2.mScattering, lfW2, lrK3.mScattering, lfW3);
    mLighting.SetToBlend(lrK0.mLighting, lfW0, lrK1.mLighting, lfW1,
                         lrK2.mLighting, lfW2, lrK3.mLighting, lfW3);
    mClouds.SetToBlend(lrK0.mClouds, lfW0, lrK1.mClouds, lfW1,
                       lrK2.mClouds, lfW2, lrK3.mClouds, lfW3);
}

// @ 0x827C5018. Tool blend setup. Once every 30 frames (miToolUpdateFrameCounter gate)
// re-parse the light-setup text file into a fresh keyframe and stamp four copies of it
// into the manager's tool-keyframe slots; then point the blend frame at those four slots
// with an equal 0.25 weight each. Returns whether the file was re-read this frame.
bool EnvironmentManager::SetupUpdateFromToolBlend(BlendFrame& lrBlendFrame)
{
    const bool lbReloaded = (miToolUpdateFrameCounter == 0);
    miToolUpdateFrameCounter = (miToolUpdateFrameCounter + 1) % 30;

    if (lbReloaded)
    {
        Keyframe lKeyframe;
        lKeyframe.Construct();

        // Scratch out-params the parser fills; only the keyframe is kept (the colour-cube
        // name/weight arrays and the debug name are discarded on the tool path).
        char  lacColourCubes[4][256];
        float lafColourCubeWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        char  lacName[256];
        memset(lacColourCubes, 0, sizeof(lacColourCubes));

        ParseEnvironmentFile(mfCurrTimeOfDay,
                             lacColourCubes,
                             lafColourCubeWeights,
                             lKeyframe.mBloom,
                             lKeyframe.mVignette,
                             lacName,
                             lKeyframe.mScattering,
                             lKeyframe.mLighting,
                             lKeyframe.mClouds,
                             "d:\\LightSetup.txt");

        maToolKeyframes[0] = lKeyframe;
        maToolKeyframes[1] = lKeyframe;
        maToolKeyframes[2] = lKeyframe;
        maToolKeyframes[3] = lKeyframe;
    }

    for (int liIndex = 0; liIndex < 4; ++liIndex)
    {
        lrBlendFrame.mapKeyframes[liIndex] = &maToolKeyframes[liIndex];
        lrBlendFrame.mafWeights[liIndex]   = 0.25f;
    }

    return lbReloaded;
}

}
}
