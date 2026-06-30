#include "GameSource/Director/BrnReplayDirector.h"

#include "SDKs/Packages/ICE/ICEData.hpp"               // ICE::ICETake (two owned playback takes)
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // BrnGameState::GameStateModuleIO::CarScoreData

#include <new>   // placement new (construct owned sub-objects at their byte offsets)

// ============================================================================
// BrnDirector::ReplayDirector member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Only the constructor is bodied. ReplayDirector is a very large, mostly-opaque object whose
// full layout is not yet modelled, so -- exactly like the sibling BrnDirectorModule.cpp -- the
// constructor addresses each touched field by byte offset through a char*/u32* view of `this`
// rather than as a named member. Every offset below is recovered directly from the X360 ctor
// body @0x827E2020 (its stw/addi sequence).
//
// The other three recovered functions (FillInSharedInfo / PreSceneQueryUpdate / Update) are
// declaration-only -- see the per-function FLAG blocks at the bottom of this file.
// ============================================================================

namespace BrnDirector
{
    // Vtable symbols the constructor installs into the two owned camera-behaviour sub-objects
    // (defined elsewhere -- external read-only data). Named from the X360 LIS/ADDI references:
    //   gpExternalBehaviourVTable -> off_8200A560 @ ReplayDirector +0x690
    //   gpBumperBehaviourVTable   -> off_8200A150 @ ReplayDirector +0x6E0
    //   gpIceWrapperVTable        -> off_82008D38 @ ReplayDirector +0x1200
    extern void* const gpExternalBehaviourVTable;
    extern void* const gpBumperBehaviourVTable;
    extern void* const gpIceWrapperVTable;

    // ---- byte offsets recovered from the ctor asm @0x827E2020 --------------------------------
    // (a1[N] in the Hex-Rays view == byte 4*N; the asm uses addi/stw at these byte offsets.)
    static const s32 KI_EXTERNAL_BEHAVIOUR_VTABLE = 0x0690;   // a1[420], stw r10, 0(r31+0x690)
    static const s32 KI_BUMPER_BEHAVIOUR_VTABLE   = 0x06E0;   // a1[440], stw r9, 0x50(r31+0x690)
    static const s32 KI_EXTERNAL_ICE_TAKE         = 0x09B0;   // ICETake @ (r31+0x690)+0x320
    static const s32 KI_ICE_WRAPPER_VTABLE        = 0x1200;   // a1[1152], stw r10, 0(r31+0x1200)
    static const s32 KI_BUMPER_ICE_TAKE           = 0x1290;   // ICETake @ (r31+0x1200)+0x90
    static const s32 KI_INDEX_FIELD_A             = 0x1C44;   // a1[1809] = -1
    static const s32 KI_PLAYER_SCORE_DATA         = 0x1FC0;   // CarScoreData @ r31+0x1FC0
    static const s32 KI_INDEX_FIELD_B             = 0x2788;   // a1[2530] = -1

    ReplayDirector::ReplayDirector()
    {
        char* lpBase = reinterpret_cast<char*>(this);

        // Two owned camera-behaviour sub-object vtables (external + bumper replay cameras).
        *reinterpret_cast<void**>(lpBase + KI_EXTERNAL_BEHAVIOUR_VTABLE) = gpExternalBehaviourVTable;
        *reinterpret_cast<void**>(lpBase + KI_BUMPER_BEHAVIOUR_VTABLE)   = gpBumperBehaviourVTable;

        // The external camera's owned ICE playback take.
        new (reinterpret_cast<void*>(lpBase + KI_EXTERNAL_ICE_TAKE)) ICE::ICETake();

        // The ICE-wrapper sub-object vtable, then the bumper camera's owned ICE playback take.
        *reinterpret_cast<void**>(lpBase + KI_ICE_WRAPPER_VTABLE) = gpIceWrapperVTable;
        new (reinterpret_cast<void*>(lpBase + KI_BUMPER_ICE_TAKE)) ICE::ICETake();

        // First index/handle field preset to -1 (li r30,-1 reused for both stores).
        *reinterpret_cast<s32*>(lpBase + KI_INDEX_FIELD_A) = -1;

        // The owned per-player score block.
        new (reinterpret_cast<void*>(lpBase + KI_PLAYER_SCORE_DATA))
            BrnGameState::GameStateModuleIO::CarScoreData();

        // Second index/handle field preset to -1.
        *reinterpret_cast<s32*>(lpBase + KI_INDEX_FIELD_B) = -1;
    }

    // ========================================================================================
    // FLAG -- DECLARATION-ONLY (no body): BrnDirector::ReplayDirector::FillInSharedInfo
    //   @0x82250558.
    //
    // Not bodied. The X360 body:
    //   * indexes the not-yet-modelled ReplayDirector layout entirely by raw byte offset
    //     (mGameState @+0x7248, mVehicleTracker @+0x7760, mAllVehicleData @+0x6928, the two
    //     owned camera behaviours, a ~+0x690-region camera-utils block, etc.);
    //   * runs a multi-stage VMX pipeline the decompiler could not even allocate locals for
    //     ("local variable allocation has failed"): a vector LCG (the 1284865837 / 0x4C957F2D
    //     multipliers driving v27/v28 with vspltisw seeds) that fills an 8-entry shuffle/jitter
    //     table at +0x8448, plus a run of lvx128/stvx128 affine-matrix copies feeding the
    //     ICE::CameraSpaceHandler::Construct call. The project rules forbid paraphrasing such a
    //     VMX pipeline to scalar -- doing so would be a fabrication.
    //   * depends on a cone of not-yet-reconstructed callees: AllVehicleData::Construct/Update/
    //     GetPlayer, ICE::CameraSpaceHandler::Construct, BrnDirector::Camera::VehicleInfo::
    //     operator_, BrnDirector::Camera::Utils::SineLerp, BrnDirector::DebugPrinter::Construct,
    //     and the sub_82207040 external.
    // Body when the ReplayDirector layout, the VMX home, and those deps are reconstructed.
    // ========================================================================================

    // ========================================================================================
    // FLAG -- DECLARATION-ONLY (no body): BrnDirector::ReplayDirector::PreSceneQueryUpdate
    //   @0x8225BD28.
    //
    // Not bodied. Calls FillInSharedInfo (above) and, on a car-asset change, rebuilds the two
    // replay cameras from the car's camerabumperbehaviour / cameraexternalbehaviour attribs
    // (Attrib::Gen::*, Attrib::RefSpec::GetCollection) and drives them through their virtual
    // SetParameters / pre-query slots. It indexes the unmodelled ReplayDirector layout by raw
    // offset and dispatches through the not-yet-reconstructed BrnDirector::Camera::Camera and
    // BehaviourGameplay* vtables. Body when that layout and dep cone are reconstructed.
    // ========================================================================================

    // ========================================================================================
    // FLAG -- DECLARATION-ONLY (no body): BrnDirector::ReplayDirector::Update @0x8225C298.
    //
    // Not bodied. The per-frame replay update: fills the shared info, ticks the two replay
    // cameras through their virtual Update slots, blends/selects between the external and
    // bumper cameras per the serialiser-frame flags, then publishes the result via
    // BrnDirector::Camera::Camera::CopyToCgsCamera and the director OutputBuffer
    // (SetCameraOutput / SetCgsCamera). It indexes the unmodelled ReplayDirector layout by raw
    // offset and depends on the not-yet-reconstructed Camera / OutputBuffer homes. Body when
    // that layout and dep cone are reconstructed.
    // ========================================================================================
}
