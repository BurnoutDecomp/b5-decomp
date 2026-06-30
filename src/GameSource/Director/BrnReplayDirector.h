#ifndef GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H
#define GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H

#include "types.hpp"

// ============================================================================
// GameSource/Director/BrnReplayDirector.h
//
// BrnDirector::ReplayDirector -- the camera director that drives camera selection and
// playback while a replay is running. The DirectorModule owns one ReplayDirector sub-object
// (constructed in place at DirectorModule +0x35F50, see BrnDirectorModule.cpp) and dispatches
// its PreSceneQueryUpdate / Update to it from the module's own per-frame spine.
//
// HOME for the ReplayDirector class. Source-of-truth filename recovered from the asserts the
// X360 bodies stream ("..\\..\\..\\GameSource\\Director/BrnReplayDirector.cpp").
//
// ----------------------------------------------------------------------------
// LAYOUT STATUS. ReplayDirector is a very large, mostly-opaque object (the constructor reaches
// fields up to +0x2788 and the Update/PreSceneQueryUpdate bodies index much further -- past
// +0x35F50-relative members such as the two owned camera behaviours at +0x1424/+0x1680 and
// +0x1600/+0x4608, the GameState at +0x7248, the VehicleTracker at +0x7760, the AllVehicleData
// at +0x6928 and many scalar/flag fields out to ~+0x29D0). That full multi-megabyte layout is
// NOT yet modelled. Only the constructor is reconstructed here, and -- exactly as the sibling
// BrnDirectorModule.cpp does -- it addresses each touched field by byte offset through a
// char*/u32* view of `this` rather than as a named member.
//
// The other three recovered functions (FillInSharedInfo, PreSceneQueryUpdate, Update) are
// declaration-only: see the FLAGs in BrnReplayDirector.cpp. They cannot be bodied faithfully
// yet because they (a) index the not-yet-modelled ReplayDirector layout entirely by raw offset,
// (b) drive a multi-stage VMX pipeline in FillInSharedInfo (a vector LCG that seeds a shuffle
// table, plus several lvx128/stvx128 matrix copies the decompiler could not even allocate
// locals for -- "local variable allocation has failed"), which the project rules forbid
// paraphrasing to scalar, and (c) dispatch through a cone of not-yet-reconstructed deps
// (BrnDirector::Camera::Camera, the camera OutputBuffer, Camera::Utils::SineLerp,
// ICE::CameraSpaceHandler, AllVehicleData::Construct/Update/GetPlayer, and several sub_*
// externals). Grow the class body (real bases + named members) and body those three functions
// when that layout and dep cone are reconstructed.
// ----------------------------------------------------------------------------

namespace BrnDirector
{

class ReplayDirector
{
public:
    // Constructor @0x827E2020 (the only function executed in the boot-trace goal milestone).
    // Installs two camera-behaviour sub-object vtables, default-constructs two owned ICE::ICETake
    // playback objects and one CarScoreData, and presets two index/handle fields to -1. Bodied.
    ReplayDirector();

    // @0x82250558 -- assembles the per-frame DirectorSharedInfo the camera behaviours consume
    // (player/used-car indices, vehicle data, game state, vehicle tracker, the camera-space
    // handler, and the bumper/external behaviour parameters). Declaration-only (FLAG: VMX
    // pipeline + unmodelled layout + unreconstructed deps).
    s32 FillInSharedInfo(s32 liSharedInfoOut, s32 liShotInfoOut, s32 liSerialiserFrame, void* lpInput);

    // @0x8225BD28 -- called by DirectorModule::PreSceneQueryUpdate before the scene query: when
    // the player car asset changes it rebuilds the two replay cameras from the car's camera-
    // behaviour attribs, then fills the shared info and runs each camera's pre-query pass.
    // Declaration-only (FLAG).
    void* PreSceneQueryUpdate(s32 liSerialiserFrame, void* lpInput);

    // @0x8225C298 -- the per-frame replay update: fills the shared info, ticks the two replay
    // cameras, blends/selects between the external and bumper cameras per the serialiser frame
    // flags, and publishes the resulting camera to the director output buffer. Declaration-only
    // (FLAG).
    void* Update(s32 liSerialiserFrame, void* lpInput);
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H
