#pragma once

// ============================================================================
// BrnWorld::RaceCarEntityModule -- the race-car entity module.
//
// This MINIMAL owning header carries only the surface needed to compile this
// TU's two ledgered accessor bodies:
//   RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex)  X360 0x822A34A8
//   RaceCarEntityModule::GetGlobalRaceCar(EGlobalRaceCarIndex)  X360 0x822A3568
// Both are simple in-range-checked &array[index] accessors. The full
// RaceCarEntityModule class (Feb-2007 leak BrnRaceCarEntityModule.h, ~50 module
// dependencies: ModuleSingleBuffered base, the streamer/boost/near-miss/crash-play
// managers, WorldMap2D, replay serialiser, etc.) is far larger and is NOT
// reconstructed here -- only the layout slice the accessors touch.
//
// Member access is BY NAME at the X360-asm-proven byte offsets; the two array
// offsets are locked with static_assert(offsetof(...)) in the .cpp:
//   maRaceCars        +0x250  (== 592)   stride 0xB0   (176B RaceCar,      35 wide)
//   maActiveRaceCars  +0x1A60 (== 6752)  stride 0x1CD0 (7376B ActiveRaceCar, 8 wide)
// The global array runs 0x250 .. 0x250+35*176 = 0x1A60, i.e. the active array
// starts immediately after it (no gap). X360 asm:
//   GetGlobalRaceCar:  return 176 * a2 + this + 592;   // &maRaceCars[a2]
//   GetActiveRaceCar:  return 7376 * a2 + this + 6752; // &maActiveRaceCars[a2]
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "BrnCommonTypes.h"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"             // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EPlayerScoringIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h" // BrnProgression::ETrainingType

#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<N,A>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<8u> (mabResetThisFrame)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::BaseResourcePtr
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h" // BrnWorld::RaceCarStreamer (by value)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"         // BrnWorld::RaceCar (by value)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"   // BrnWorld::ActiveRaceCar (by value)
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaManager::BrnSubmissionInterface (SubmitCoronasForRaceCar's 1st arg)
#include "GameSource/World/BrnPlaceOnTrackManager.h"                              // BrnWorld::PlaceOnTrackManager (by value, +0x17850)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                           // CgsWorld::WorldMap2D (by value, +0x18300)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h"                 // BrnWorld::BoostManager (by value, +0x17890)
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"  // BrnWorld::CrashPlayManager (by value, +0x180F0)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"     // BrnWorld::PlayerVehicleControls (by value, +0x183A8)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::EPaybackType (meActivePaybackType)

#include <cstddef>                                   // offsetof

namespace CgsGraphics { class DispatchFrame; }
// (BrnWorld::ShadowMap is a `struct` -- BrnShadowMap.h:65; never forward-declare it
//  as `class` here, the class key is part of the MSVC mangling.)
namespace BrnWorld { struct ShadowMap; }


namespace CgsResource { struct ResourceHandle; }
namespace BrnResource { struct VehicleList; class WheelList; enum ECarType : int; }
// The game-action consumer's two argument types (pointer-only here; the .cpp includes both
// owning headers). ResetPlayerCarAction is game action 0's 80-byte payload,
// RaceCarAIInterface is the AI publish surface SpawnRaceCar posts its attach event into.
namespace BrnGameState { namespace GameStateModuleIO {
    struct ResetPlayerCarAction;
    struct PrepareForModeAction;
} }
namespace BrnAI       { namespace AIModuleIO         { struct RaceCarAIInterface;   } }
namespace BrnPhysics  { namespace Vehicle            { struct VehicleInputInterface; } }
// DetachActiveRaceCar's fourth argument (DWARF spells it
// OutputBuffer_PreScene::SceneInputInterface, a typedef for this type). Pointer-only here;
// the .cpp reaches it through BrnRaceCarEntityModuleIO.h.
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface; } }

namespace BrnWorld
{

// The three dispatch-list ids the console's WorldModule::GenerateDispatchLists hands the
// race-car module (`li r6, 0xC` / `li r7, 0x13` / `li r8, 0x14` @0x827D27AC..0x827D27B8):
// the dispatch-frame OBJECT list and the two race-car MESH lists. Compare the world's own
// 2 / 11 / 15 in BrnWorldModule.cpp.
const s32 KI_RACE_CAR_OBJECT_LIST           = 12;
const s32 KI_RACE_CAR_OPAQUE_MESH_LIST      = 19;
const s32 KI_RACE_CAR_TRANSPARENT_MESH_LIST = 20;

// X360-attested pending-training-request ring depth (DWARF BrnRaceCarEntityModule.h:66).
// AddTrainingRequest asserts miPendingRequestCount < this before appending.
const s32 KI_TRAINING_REQUEST_QUEUE_SIZE = 8;

// The active-race-car output interface (real home:
// SharedIO/BrnRaceCarEntityModuleOutputInterface.h). CopyActiveRaceCarToPlayerScoringMappingToOutput
// only takes a pointer to it, so a forward declaration suffices here.
namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; }
namespace RaceCarEntityModuleIO { struct GameEventQueue; class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_GenerateDispatchLists; struct InputBuffer_PreScene; struct OutputBuffer_PreScene; struct InputBuffer_PostPhysics; struct OutputBuffer_PostPhysics; struct OutputBuffer_Prepare; }

// The "CarColours" palette resource LoadGlobalResources acquires (real home
// SharedClasses/Graphics/BrnGlobalColourPalette.h); held by pointer only here.
struct GlobalColourPalette;

// ⭐ ADDED 2026-08-11 (player-input wave). DWARF BrnRaceCarEntityModule.h:95-97 -- this header
// IS its home. One entry of the module's per-frame stomped/leaped car list
// (mStoredStompees[8], below), produced by ProcessLeapedAndStompedCars @0x822BD5B8 and drained
// by ProcessPlayerVehicleInput @0x822FFE30 into
// VehicleDriverInputInterface::AddTargetAssist(Vector3, EntityId) -- whose parameter pair is
// exactly these two members, in this order. The console loop stride is 0x20 with the id read at
// +0x10 (`lwz r18, 0x10(r31) ; lvx128 v127, r0, r31 ; ... ; addi r31, r31, 0x20`), which is what
// the Vector3's 16-byte alignment produces here too.
struct StoredStompeeData
{
    Vector3  mPosition;   // :96  @0x00
    EntityId mEntityId;   // :97  @0x10
};

// ---- element types (2026-07-31: THE ODR FORK IS GONE) ----------------------
// This header used to define its own opaque `class RaceCar { u8 [0xB0]; }` and
// `class ActiveRaceCar { u8 [0x1CD0]; }` so the two array accessors could compile
// against a known stride. Both real types are committed (BrnRaceCar.h /
// BrnActiveRaceCar.h) and are now included above, so those stand-ins -- a genuine
// two-definitions-of-one-class ODR violation the moment any TU saw both headers --
// are deleted. Consequence: on the x64 gate the element strides are the real
// sizeof()s (both types carry pointers the console stored in 4 bytes), so the
// CONSOLE array offsets 0x250 / 0x1A60 no longer hold for maActiveRaceCars and the
// offsetof pins below drop to the members whose offsets are still meaningful.
// Nothing reads this module by offset -- parity is by named member.

class RaceCarEntityModule
{
public:
        // ---- ADDITIVE (attested by WorldModule::Construct @0x827CF540, which
        //      virtual-dispatches the fleet lifecycle) ----
        // Declaration-only; the body lands with this module's own TU.
        void Construct();
        // ---- ADDITIVE (attested by WorldModule::DestructWorld @0x827BD0F0) ----
        // Declaration-only; the body lands with this module's own TU.
        void Destruct();
        // ---- ADDITIVE (attested by WorldModule::ReleaseWorld @0x827BCE58) ----
        // Declaration-only; the body lands with this module's own TU.
        bool Release();

        // ---- ADDITIVE (WorldModule::EntityModulePrePhysicsUpdate @0x827BD5B8) ----
        // Declaration-only; body with this module's own TU.
        void PrePhysicsUpdate( RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                               RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                               BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePostSceneUpdate @0x827C3C58) ----
        void PostSceneUpdate( RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                              RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput,
                              BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePreSceneUpdate @0x827BD1F0) ----
        // Declaration-only; body gated in WorldLinkStubs.cpp until this module's
        // own TU lands.
        void PreSceneUpdate( RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                             RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                             BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::EntityModulePostPhysicsUpdate @0x827D3F10) ----
        // Declaration-only; body gated in WorldLinkStubs.cpp until this module's
        // own TU lands.
        void PostPhysicsUpdate( RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                                RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput,
                                BrnUpdateSet lUpdateSet );

        // ⭐⭐ X360 0x822E87B8 -- THE PHYSICS READBACK (physics-return-path wave 2026-08-11).
        // The console's ONE producer of every active car's pose: it walks the post-physics
        // input buffer's VehicleOutputInterface and hands each published RaceCarState to
        // ActiveRaceCar::UpdatePhysicsState @0x822D4418, then drains the deformation
        // output. Its only caller is PostPhysicsUpdate @0x82307538 (the `bl` at 0x8230761C).
        // Signature from the asm prologue: r3 = this, r4 = the input buffer. See the .cpp
        // banner for the leg-by-leg landed/parked inventory.
        void ReadUpdatedActiveRaceCarDataFromPhysics(
                RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput );

        // ====================================================================
        // THE THREE SCENE LEGS OF PostPhysicsUpdate (wave Q5, cluster G1 -- the car's
        // half of the scene volume-collision middle). All three are console functions
        // called only from PostPhysicsUpdate @0x82307538, at these exact `bl` sites:
        //     0x823075F8  ProcessCreateVehicleEvents(lpInput, lpOutput)
        //     0x82307658  GenerateSceneUpdateEvents(lpOutput)      [inside the paused skip]
        //     0x8230773C  SendRaceCarSceneUpdates(lpOutput)
        // Signatures are the DecFIGS DWARF's own (BrnRaceCarEntityModule.h entries for
        // .cpp:5070 / :5859 / :4980) and match the ARTIST asm register use one for one.
        // ====================================================================

        // X360 0x822FF620 (182). Drain the physics vehicle manager's create-vehicle result
        // queue: for every result whose VolumeInstanceId owner is E_ENTITYTYPE_RACECAR,
        // hand the freshly created handling body to the matching active slot
        // (ActiveRaceCar::OnHandlingModelAdded -> AddToScene, which is what registers the
        // car's BOX VOLUME with the scene), and -- for the player's car only -- publish the
        // new vehicle's attribute key + vehicle-list index to the director.
        // ⚠️ `const` on the input buffer is the DWARF's.
        void ProcessCreateVehicleEvents(
                const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822F6B08 (56). For every ACTIVE slot, run the per-car post-physics scene
        // publish (ActiveRaceCar::SendSceneUpdatesPostPhysics -> UpdateCullingGroup plus the
        // late AddToCollision arm). Runs OUTSIDE the console's paused skip.
        void SendRaceCarSceneUpdates( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822D2500 (100). ⭐ THE CAR'S PER-FRAME SCENE-TRANSFORM PRODUCER: for every
        // ACTIVE slot push InSceneUpdateInterface::SetVolumeInstanceTransform( the slot's
        // handling-body VolumeInstanceId, mPhysicsState.mTransform ) and
        // SetEntityPosition( that id's entity word, the transform's translation ), then
        // SetPaddingForResetRaceCars. Without it a registered car volume never moves off its
        // spawn point, which is the exact signature the wave-Q5 scout names in §6.
        void GenerateSceneUpdateEvents( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822CEEA8 (the tail call of GenerateSceneUpdateEvents). Walk
        // mabResetThisFrame; for every set bit whose slot IsActive(), clear that entity's
        // volume padding, then clear the whole bit array. DWARF declares the parameter as
        // `OutputBuffer_PreScene::SceneInputInterface*` -- the same InSceneUpdateInterface
        // type the PostPhysics output buffer hands out.
        void SetPaddingForResetRaceCars(
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface );

        // ---- X360 0x822E79F8 (called by WorldModule::GenerateDispatchLists @0x827D27C8) ----
        // ⚠ NOTE the argument list. The earlier PC declaration carried only the input
        // buffer, the visible-entity array and the three vector args -- it DROPPED the four
        // GPR arguments the console call site loads right before the branch:
        //     li r6, 0xC   li r7, 0x13   li r8, 0x14   li r9, 0
        // i.e. the dispatch-frame OBJECT list (12) and the two MESH lists (19 = race-car
        // opaque, 20 = race-car transparent) that RenderRaceCar forwards straight into
        // DispatchFrame::GetList / DrawRenderable::AddToBin, plus a bool. Without them the
        // render leg has nowhere to submit. (Same trap as the streamer wave's Prepare:
        // recover a stub's signature from the ASM, not from the existing declaration.)
        // The vector args: v1 = fog scattering, v2 = fog colour + white level,
        // v3 = the camera position (the `vsubfp128 v13, v123, v13` operand).
        void GenerateDispatchLists( RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpInput,
                                    const Array<CgsSceneManager::EntityId, 32u>& lrVisibleEntities,
                                    s32  liObjectList,
                                    s32  liOpaqueMeshList,
                                    s32  liTransparentMeshList,
                                    bool lbEnvironmentMapPass,
                                    Vector4 lvFogScattering,
                                    Vector4 lvFogColourPlusWhiteLevel,
                                    Vector3 lvCameraPosition );

        // ---- X360 0x822CF6A0 -- submit ONE race car's dispatch entries ----
        // Signature recovered from the asm prologue (@0x822CF6C4..0x822CF764: r3->r20 this,
        // r5->r16 render params, r6->r25 graphics ptr, r7/r8/r9/r10 spilled to arg_64/6C/74/7C,
        // r4 spilled to arg_1C) and the call site @0x822E8550 (r5 = &activeCar + 0x7E0,
        // stack arg_84 = the shadow map, stack arg_8F = the "render attached geometry" bool).
        //   lfCameraDistance : v1 -- the camera->car DISTANCE the caller computes with
        //                      vmsum3fp128 + vrsqrtefp + vmulfp (a scalar broadcast into all
        //                      four lanes, NOT a direction vector).
        void RenderRaceCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                            ActiveRaceCar::RenderParams* lpRenderParams,
                            const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>* lpCarGraphics,
                            const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>* lpWheelGraphics,
                            s32 liObjectList,
                            s32 liOpaqueMeshList,
                            s32 liTransparentMeshList,
                            const ShadowMap* lpShadowMap,
                            bool lbRenderAttachedGeometry,
                            f32  lfCameraDistance,
                            Vector4 lvFogScattering,
                            Vector4 lvFogColourPlusWhiteLevel );

        // ---- X360 0x822D1600 -- ONE race car's LAMP FLARES (coronas) ----
        // Signature + `const` are the DWARF's (BrnRaceCarEntityModule.h:788, definition
        // BrnRaceCarEntityModule.cpp:3828) and they match the asm prologue one for one:
        //   r3 = this, r4 = the corona submission interface (asserted non-null, :3905),
        //   r5 = the car's STREAMED DEFORMATION SPEC resource pointer (dereferenced through
        //        the resource-pointer accessor at 0x822D1720 for its
        //        mCarModelSpaceToHandlingBodySpaceTransform at spec+1552),
        //   r6 = the car's RenderParams (asserted non-null, :3906),
        //   r7 = a BYTE (`clrlwi r11, r28, 24`): "this is the LOCAL PLAYER's car", which
        //        selects the eCoronaTypePlayerCar* archetype bank over the eCoronaTypeRaceCar*
        //        one. Its call site computes it as
        //        `(u16)mePlayerActiveRaceCarIndex == liActiveRaceCar` (@0x822E80A8..0x822E80B8).
        // Bodied in BrnRaceCarEntityModule_Render.cpp beside its only caller.
        void SubmitCoronasForRaceCar(
                BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource,
                ActiveRaceCar::RenderParams* lpRenderParams,
                bool lbIsPlayerCar ) const;

        // ---- X360 0x82303E78 (attested by WorldModule::Prepare @0x827D53B0 stage 6) ----
        // NOTE the argument list: the console signature is
        //   Prepare(this, OutputBuffer_Prepare* lpOutput, ResourceHandle lDistrictMapHandle)
        // (asm prologue: r4 -> the `lpOutput != NULL` assert at BrnRaceCarEntityModule.cpp:663,
        // r5 -> the district-map handle asserted at :671). The earlier PC declaration dropped
        // the output buffer, which is exactly the buffer LoadGlobalResources publishes its
        // resource requests into -- without it the module can never ask for anything.
        bool Prepare( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput,
                      const CgsResource::ResourceHandle& lrDistrictMapHandle );

        // X360 0x82300730. The module's own resumable global-resource load: acquire
        // "CarColours" (pool 5), stream "Vehicles/VEHICLETEX.BIN" into the CarShared pool
        // (25), then GET the vehicle list and the wheel list. Returns false while waiting.
        bool LoadGlobalResources( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput );

        // ---- the per-frame STREAMING pump (race-car streamer wave 2026-07-31) ----

        // X360 0x822FEFE0 (DWARF BrnRaceCarEntityModule.h:563). Pump the five component
        // streamers for one frame, then sweep the eight active-car slots for cars whose
        // resources have just completed. Called from PreSceneUpdate when not in replay.
        void UpdateStreaming( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                              RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 0x82304F70. Drain the five component streamers' own GameData request
        // queues onto the PostPhysics output buffer's resource-request interface -- the
        // ONLY way a race-car load request leaves this module. Called from PostPhysicsUpdate.
        void SendStreamerEvents( RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput );

        // X360 0x822CF208 (DWARF BrnRaceCarEntityModule.h:650). THE per-frame PAINT PUBLISH:
        // for every ATTACHED active slot, resolve the paired global RaceCar's
        // {miColourPalette, miColourIndex} (both -1 -> 0) against mCarColoursResource and copy
        // the two Vector4 colours into the slot's render snapshot. It is the ONLY writer of
        // RenderParams::mPaintColour / mPearlescentColour anywhere in the image, and
        // RenderRaceCar uploads exactly those two as shader constants 20 (g_paintColour) and
        // 21 (g_pearlescentColour) -- the body panels' whole colour, since
        // Vehicle_Opaque_BodypartsSkin_EnvMapped_Default declares no diffuse sampler at all.
        // Called from PostPhysicsUpdate (its only caller), unconditionally: the console's
        // paused branch at 0x82307610 jumps to 0x823076C0, i.e. to the instruction that sets
        // up this call, so it runs paused or not.
        void UpdateActiveRaceCarColours();

        // X360 0x822D27B0 (DWARF BrnRaceCarEntityModule.h:824 --
        // `void ChangePlayerCarColour(uint32_t, uint32_t)`). Seed the PLAYER's global
        // RaceCar with the {palette, colour} pair game action 79
        // (CarSelectChangeColourAction) carries, after range-asserting both against
        // mCarColoursResource. It is the ONLY console writer of the pair on the
        // junkyard start-of-game path -- HandleResetPlayerCarAction spawns every fresh
        // car at 0/0 -- so without it every car renders in palette 0 / colour 0
        // regardless of the default the VehicleList authored (367 of 431 entries author
        // something else; PUSMC01 authors colour 13).
        void ChangePlayerCarColour( u32 luPaletteIndex, u32 luColourIndex );

        // X360 0x822F5CF8. THE per-frame OUTPUT PUBLISH: copy every attached active slot's
        // live physics state + identity into the active-race-car output interface, and the
        // player's slot/engine-state into its player-scoped scalars. Called from both
        // PreSceneUpdate and PostPhysicsUpdate on the console; it is the ONLY producer of
        // RCEntityActiveRaceCarOutputInterface anywhere in the image, and therefore the head
        // of the chain that ends at the director's per-car VehicleInfo
        // (UpdateOutputInterfaces -> BridgeRaceCarEntityInfoToOutput_PostPhysics ->
        //  BrnGameModule::BridgeWorldToDirector -> DirectorIO::InputBuffer::SetRaceCarInfo).
        void UpdateOutputInterfaces(
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalCarInterface,
                RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpReplayActiveCarInterface,
                RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpReplayGlobalCarInterface );

        // X360 0x822F4DB0. Bind lpRaceCar to an active-race-car slot and start its asset
        // load. leActiveRaceCarIndex may be E_ACTIVE_RACE_CAR_INDEX_INVALID (-1), in which
        // case the console re-uses the car's own previous slot if it has one and otherwise
        // takes the first slot whose muState is E_STATE_INACTIVE. Returns the slot used.
        //
        // ⚠️ NOT [VMX]: the PC FLAG INVENTORY files this function under [VMX]; the console
        // body contains no vector instruction at all.
        EActiveRaceCarIndex AttachActiveRaceCar( RaceCar* lpRaceCar,
                                                 EActiveRaceCarIndex leActiveRaceCarIndex );

        // ====================================================================
        // THE REMOVE / DETACH PAIR (ghost-car wave 2026-08-17). Both were ledger-`reviewed`
        // and ABSENT from the tree, which is why Car Select's re-spawn left the PREVIOUS
        // player car E_STATE_ACTIVE at the same position (two cars in the
        // `[racecar-lod] banded` probe, coarse-LOD wheel proxies drawing over the real ones,
        // and a ghost left in the junkyard when the player drove off).
        // Signatures are the DecFIGS DWARF's own (BrnRaceCarEntityModule.h:707 / :725) and
        // match the ARTIST asm's register use exactly.
        // ====================================================================

        // X360 0x82304440. Take a global race car out of the world: clear the module's player
        // slot if this WAS the player's car, detach its active slot (if it has one), detach AI
        // control, and RaceCar::RemoveFromWorld it. Eight console callers; only
        // HandleResetPlayerCarAction is live on this build (see the .cpp banner).
        void RemoveRaceCar( EGlobalRaceCarIndex leGlobalRaceCarIndex,
                            RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 0x822FEDF8. The inverse of AttachActiveRaceCar: release the slot's streamed
        // assets (RaceCarStreamer::RemoveVehicleData), tell the AI module the car is no longer
        // simulated, and ActiveRaceCar::Detach the slot -- which is what returns muState to
        // E_STATE_INACTIVE and stops GenerateDispatchLists drawing it.
        void DetachActiveRaceCar(
                RaceCar* lpRaceCar,
                BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface,
                BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface );

        // ====================================================================
        // THE GAME-ACTION CONSUMER (reset-player-car wave 2026-08-01).
        // ====================================================================

        // X360 0x8230BE08. Drain the pre-scene input buffer's game-action queue and apply
        // every action this module owns. PARTIAL SLICE -- see the .cpp banner for exactly
        // which of the ~100 cases are reproduced and why the rest are dropped rather than
        // paraphrased.
        //
        // ⚠️ THREE arguments. Hex-Rays renders one: the console prologue is
        //   r31 = r3 (this); r3 = r4 (the pre-scene INPUT buffer, whose GetGameActionQueue
        //   the very next instruction calls); r20 = r5 (the pre-scene OUTPUT buffer, which
        //   every case forwards to its handler). Recovered from the asm, not the pseudocode.
        void HandleGameActions( RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                                RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // ARTIST 0x822A4700; DecFIGS supplies the BrnResource::ECarType shape.
        void HandleCarStatsUpdate(BrnResource::ECarType leCarType,
                                  s32 liBoostLevel,
                                  s32 liBoostLossLevel);

        // ARTIST 0x823092F0. The currently reconstructed body owns the common
        // non-Showtime mode/boost arming spine; see ModeArming.cpp.
        void HandlePrepareForModeAction(
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction,
            RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);

        // X360 0x82304FE8 -- game action 0. Place (or re-spawn) the player's car. THE
        // record CarSelectManager posts to put the player in a junkyard.
        // ⚠️ THREE arguments again (this / lpAction / the pre-scene output buffer; r22/r16/r25
        // in the prologue).
        void HandleResetPlayerCarAction(
                const BrnGameState::GameStateModuleIO::ResetPlayerCarAction* lpAction,
                RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput );

        // X360 0x822FE5D8. Find a free global race-car slot, Prepare + AddToWorld it at
        // lrTransform, resolve the wheel set when lWheelModelId is null, and publish the AI
        // module's AttachAIControlEvent. Returns the global slot used.
        //
        // ⚠️ The signature is EIGHT parameters and Hex-Rays renders TWENTY-TWO (it counts the
        // callee's own spills into the caller-provided home area). Recovered from the X360
        // home-area slot spacing (r3->sp+0x14, r4->+0x1C, r5->+0x24 ... r10->+0x4C, first
        // stack arg -> +0x54, which is exactly the slot HandleResetPlayerCarAction writes):
        //   r4 lpRaceCarAIInterface  r5 lrTransform  r6 leType  r7 lModelId
        //   r8 lbKeepResetSection    r9 lWheelModelId  r10 lpRivalId (NULLABLE)
        //   sp+0x54 liOpponentIndex
        // lbKeepResetSection is named by the DecFIGS DWARF: it is the fourth member of
        // AttachAIControlEvent (BrnRaceCarAIInterfaces.h:306), which this function is the
        // only producer of.
        EGlobalRaceCarIndex SpawnRaceCar(
                BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
                const Matrix44Affine& lrTransform,
                ERaceCarType leType,
                CgsID lModelId,
                bool lbKeepResetSection,
                CgsID lWheelModelId,
                const CgsID* lpRivalId,
                s32 liOpponentIndex );

        // [FLAG PC bring-up] NOT an X360 function. The world module's bring-up tour camera
        // asks where the spawned car is so it can frame it; false when no slot is active.
        // DELETE with that camera.
        bool GetSpawnedCarPositionBringUp( Vector3& lrPosition ) const;

        // [DIAG] const slot access for the frame-pacing probe in BrnWorldModule.
        const ActiveRaceCar* GetActiveRaceCarConstBringUp( s32 liCar ) const
        { return ( liCar >= 0 && liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT ) ? &maActiveRaceCars[liCar] : 0; }

        // ⚠️ FLAG PC quality-of-life -- NOT an X360 function. Once per rendered frame,
        // publish each active car's display pose as the blend of the last two simulation
        // ticks (the simulation runs at a fixed 60 Hz; the renderer does not). Idempotent.
        // See ActiveRaceCar::ApplyRenderPoseInterpolation.
        void ApplyRenderPoseInterpolationBringUp( f32 lfAlpha );

        // [RETIRED 2026-08-18, wave Q5 finisher] PublishNewVehicleToDirectorWithoutPhysicsBringUp
        // was declared here. The real leg it stood in for -- ProcessCreateVehicleEvents
        // @0x822FF620 -- is complete as of that wave and publishes the director NewVehicle event
        // itself, on the console's own trigger, so the stand-in was deleted rather than left
        // beside it. See the .cpp banner at the function's old seat.

        // ====================================================================
        // THE ATTACHED -> WAITING -> ACTIVE CHAIN (drivable wave 2026-08-01).
        // These four are what retire PromoteAttachedCarToActiveBringUp.
        // ====================================================================

        // X360 0x822FEBF8. A car's five streamed resources have arrived: publish them into
        // the slot, colour it, and ask for it to be placed on the track. PARTIAL SLICE --
        // see the .cpp banner. Only caller: UpdateStreaming.
        void OnRaceCarResourcesLoaded(
                EActiveRaceCarIndex leActiveRaceCarIndex,
                BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface );

        // X360 0x822CE588. Decide WHERE a freshly loaded car goes and post the request.
        // For a PLAYER car outside a game mode that is ActiveRaceCar::RequestPlaceOnTrack
        // at the car's own AddToWorld pose, which is the start-of-game path this build
        // drives; every other arm goes through RaceCar::RequestResetOnTrack.
        void PlaceRaceCarOnLoad( RaceCar* lpRaceCar );

        // X360 0x822F4880. ⭐ THE ONLY WRITER OF ActiveRaceCar::E_STATE_ACTIVE IN THE XEX.
        // Only caller: PlaceOnTrackManager::PrePhysicsUpdate.
        //
        // ⚠️ FOUR arguments + a VECTOR. Hex-Rays renders `(this, index, transform,
        // vehicleInterface)` and DROPS the velocity, which arrives in v1 (`vmr128 v127, v1`
        // at 0x822F4890 and `vmr128 v1, v127` right before each of the two callees).
        // Incident TEN of the dropped-argument rule.
        void ResetActiveRaceCar( EActiveRaceCarIndex leActiveRaceCarIndex,
                                 const Matrix44Affine& lrTransform,
                                 const Vector3& lrVelocity,
                                 BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface );

        // X360 inlined at PlaceOnTrackManager::PrePhysicsUpdate (`*(module + 100041) == 0`
        // -> lbIgnoreFatal). Named accessor rather than an offset read from another class.
        bool IsInCarSelectScreen() const { return mbInCarSelectScreen; }

    // X360 0x822A34A8 -- &maActiveRaceCars[leActiveRaceCarIndex], in-range checked.
    inline ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex);

    // The module's OWN player slot, +0x182F8 (99064) -- the word WorldModule::CalculateVehicleLODs
    // @0x827C3778 reads straight off this object (`lwzx r4, r3, r11`, r3 == WorldModule+0x280 ==
    // mRaceCarEntityModule, r11 == 0x182F8) to force the player's car to LOD 0. DWARF names no
    // accessor (the console reaches the private member directly); this inline is that read given a
    // name, for that one reader. It is NOT the WorldModule mirror meLocalPlayerActiveRaceCarIndex:
    // the mirror is published only for an ATTACHED slot (UpdateOutputInterfaces gate,
    // BrnRaceCarEntityModule.cpp ~:2171), so on the Car Select / junkyard screen -- player car
    // created (mePlayerActiveRaceCarIndex valid, :1699) but not attached to physics -- the mirror
    // was INVALID, nothing lifted RenderParams::mLOD off Reset's 4, and the wheels drew their
    // authored LOD-4 box proxy (car+lights step 1b, boot-verified).
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayerActiveRaceCarIndex; }

    // X360 0x822A3568 -- &maRaceCars[leGlobalRaceCarIndex], in-range checked.
    inline RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // ------------------------------------------------------------------------
    // Active-race-car <-> player-scoring-slot mapping (online scoring). The module
    // keeps maActiveRaceCarForPlayerScoringIndex[player] == the active-race-car slot
    // that the given player is scoring as, or E_ACTIVE_RACE_CAR_INDEX_COUNT (8) as the
    // "no mapping" sentinel (that is the value the X360 stores -- `li r10,8` / `li r27,8`).
    // ------------------------------------------------------------------------

    // X360 0x822A3760 -- set every player slot to the "no mapping" sentinel (8).
    void ClearAllActiveRaceCarToPlayerScoringMappings();

    // X360 0x822BE0A0 -- walk all eight active-race-car slots and put every ATTACHED one
    // into the current game mode. The console INLINES RaceCar::SetInCurrentGameMode
    // @0x822B3F08 at the store pair (`stb r22,0xA6(r31)` / `stb r20,0xA7(r31)`, r22 == the
    // literal 1), so the in-game flag is a CONSTANT true here and only the car-select-allowed
    // flag comes from the argument -- HandlePrepareForModeAction passes its own
    // mbCarSelectAllowedInGameMode (+99143), NOT the online flag.
    void SetAllActiveCarsInGameMode(bool lbCarSelectAllowedInGameMode);

    // X360 0x822A4850 -- put every ATTACHED active-race-car slot into a race-start state.
    // lbIncludePlayer selects whether the PLAYER's own slot (mePlayerActiveRaceCarIndex,
    // +0x182F8) is included; SetupOpponents @0x82307DF0 always passes true.
    void SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState leRaceStartState,
                               bool lbIncludePlayer);

    // X360 0x822A37C8 -- find the player slot currently mapped to leActiveRaceCarIndex
    // and reset it to the sentinel (8). If no slot maps to it, do nothing.
    void ClearActiveRaceCarToPlayerScoringMapping(EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3888 -- map player scoring slot lePlayerScoringIndex to
    // leActiveRaceCarIndex.
    void SetActiveRaceCarForPlayerScoringIndex(
        BrnGameState::GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex,
        EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3918 -- copy the whole player-scoring mapping into the active-race-car
    // output interface (one SetActiveRaceCarIndex per slot).
    void CopyActiveRaceCarToPlayerScoringMappingToOutput(
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpOutputInterface);

    // X360 0x822A3A20 -- (mxGameModeFlags & lxFlagMask) != 0.
    bool GetGameModeFlag(u64 lxFlagMask) const;

    // ------------------------------------------------------------------------
    // Tail-state bookkeeping bodied in BrnRaceCarEntityModule.cpp. These touch only
    // the module's own scalar tail at attested offsets (plus, for UpdateTailgateTimer,
    // a forward-declared sibling query), so they reconstruct BY NAME without modelling
    // the un-homed RaceCar/ActiveRaceCar/manager interiors.
    // ------------------------------------------------------------------------

    // X360 0x822A47A8 -- append leTrainingType to mePendingTrainingRequestQueue (range +
    // capacity asserted), bumping miPendingRequestCount. No-op if the ring is full.
    void AddTrainingRequest(BrnProgression::ETrainingType leTrainingType);

    // X360 0x822CE508 -- if the player car is tailgating any other race car, accumulate
    // lfDeltaTime into mfCurrentTailgateDuration; otherwise reset it to 0. Returns the
    // tailgating predicate. (Calls IsPlayerCarTailgatingOtherRaceCars, declared below.)
    bool UpdateTailgateTimer(f32 lfDeltaTime);

private:
    // FLAG: declaration-only sibling this TU references but does not body here -- it
    // reaches the un-homed ActiveRaceCar interior + a tailgating cone test. Declared so
    // UpdateTailgateTimer links; its body belongs to a later race-car-interior pass.
    bool IsPlayerCarTailgatingOtherRaceCars(
        EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
        const ActiveRaceCar* lpPlayerActiveRaceCar);

    // Compiled-never-called offsetof layout lock (see definition below).
    void LockLayout_();

    // FLAG: opaque leading state. In the full class this span is the
    // ModuleSingleBuffered base plus the early stage/handle/region members; here
    // it exists only to land maRaceCars at the X360-proven +0x250 offset.
    u8 maPrecedingState[0x250];

    // Global race-car slots (player + up to 34 rivals/traffic). +0x250, stride 0xB0.
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];

    // Active race-car slots (local player + rivals). +0x1A60, stride 0x1CD0.
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    // (maActiveRaceCars ends at +0x1A60 + 8*0x1CD0 == +0x100E0 == 65760)

    // FLAG: opaque mid-object state. The full class carries the streamer/boost/near-miss/
    // crash-play managers, timers, RNGs, etc. between the active-car array and the
    // game-mode/scoring tail below; here it is honest padding that lands the named
    // members at their X360-asm-proven byte offsets. The bodied tail functions
    // (scoring map, GetGameModeFlag, AddTrainingRequest, UpdateTailgateTimer) are the
    // only ones in this TU that touch the tail.
    u8 maTailPadA0[0x182F0 - 0x100E0];  // +0x100E0 (65760) .. +0x182F0 (99056)

    // X360 +0x182F0 (99056). Seconds the player has been continuously tailgating another
    // race car; UpdateTailgateTimer accumulates dt into it while tailgating, else zeroes
    // it. DWARF BrnRaceCarEntityModule.h:357 -> float32_t.
    f32 mfCurrentTailgateDuration;      // +0x182F0 (99056) .. +0x182F4 (99060)

    // X360 +0x182F4 (99060). UpdateBoost loads this exact 32-bit enum for
    // BoostStrategy::SetTailgating at 0x82304B50..0x82304B64.
    EActiveRaceCarIndex meIndexOfCarPlayerIsTailgating = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                                        // +0x182F4 (99060) .. +0x182F8 (99064)

    // X360 +0x182F8 (99064). The active-race-car slot the local player is driving, or
    // E_ACTIVE_RACE_CAR_INDEX_INVALID. UpdateTailgateTimer reads it (asm `lwzx` at 0x182F8)
    // to pick the player car. DWARF BrnRaceCarEntityModule.h:360 -> EActiveRaceCarIndex.
    // ⛔ SEEDED 2026-08-01 (reset-player-car wave), and this was a REAL DEFECT with two live
    // consequences. RaceCarEntityModule::Construct @0x822FD898 -- which is where the console
    // stores E_ACTIVE_RACE_CAR_INDEX_INVALID here -- is declaration-only on this build (it is in
    // the [VMX] FLAG INVENTORY), so this member sat at whatever the zeroed module memory held:
    // 0 == E_ACTIVE_RACE_CAR_INDEX_0, i.e. "the player is driving slot 0", from frame one.
    //   * UpdateOutputInterfaces publishes slot 0 as the player's car before any car exists;
    //   * HandleResetPlayerCarAction takes its "remove the EXISTING player car" branch on the
    //     very first record it is ever handed -- MEASURED: the first live action fired the
    //     console's own "Invalid Number of Palettes"/"Invalid car colour" pair, because it read
    //     RaceCar::Prepare's -1 colour sentinels off a slot that had never been a player car.
    // The in-class initialiser is the same seam BrnGameModule's mpOutputBuffer uses; DELETE it
    // when Construct lands.
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                                        // +0x182F8 (99064) .. +0x182FC (99068)

    u8 maTailPadA1b[0x18358 - 0x182FC]; // +0x182FC (99068) .. +0x18358 (99160)

    // X360 +0x18358 (99160). GetGameModeFlag reads it with a 64-bit load (`ldx`) and ANDs
    // it with the caller's mask. DWARF BrnRaceCarEntityModule.h:388 -> uint64_t.
    u64 mxGameModeFlags;                // +0x18358 (99160) .. +0x18360 (99168)

    u8 maTailPadB0[0x18374 - 0x18360];  // +0x18360 (99168) .. +0x18374 (99188)

    // X360 +0x18374 (99188). Ring of training requests queued this frame, drained by the
    // progression handler. AddTrainingRequest appends at miPendingRequestCount; each cell
    // is a 4-byte ETrainingType (the asm `stwx` stores the 32-bit enum). DWARF
    // BrnRaceCarEntityModule.h:401 -> BrnProgression::ETrainingType[8].
    BrnProgression::ETrainingType mePendingTrainingRequestQueue[KI_TRAINING_REQUEST_QUEUE_SIZE];
                                        // +0x18374 (99188) .. +0x18394 (99220)

    // X360 +0x18394 (99220). Number of valid entries in mePendingTrainingRequestQueue.
    // DWARF BrnRaceCarEntityModule.h:402 -> int32_t.
    s32 miPendingRequestCount;          // +0x18394 (99220) .. +0x18398 (99224)

    u8 maTailPadB1[0x187BC - 0x18398];  // +0x18398 (99224) .. +0x187BC (100284)

    // X360 +0x187BC (100284). Player-scoring-slot -> active-race-car-slot map. The X360
    // DWORD index is 0x61EF (25071). Indexed by EPlayerScoringIndex (0..7); each cell is
    // E_ACTIVE_RACE_CAR_INDEX_COUNT (8) when no active car is mapped to that player.
    EActiveRaceCarIndex maActiveRaceCarForPlayerScoringIndex
        [BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT];

    // ========================================================================
    // MODELLED members (global-resource wave).
    //
    // These five are real, named members carrying state that on the console lives
    // INSIDE the opaque spans above. They are APPENDED rather than carved out of
    // those spans deliberately: the pads are a 32-bit-console fiction that only
    // holds while every byte in them is anonymous, and carving would move every
    // following member and break the (still-useful) offset locks on the array
    // strides. Per the project's named-member parity rule the x64 byte offsets are
    // not load-bearing -- the console offsets are recorded per member so the next
    // wave can fold the pads down when the rest of the interior is modelled.
    // ========================================================================

    // X360 +0x22C (556). Prepare's own resumable stage (0..3, `lwz r10,0x22C(r30)`).
    s32 mePrepareStage;

    // X360 +0x230 (560). Cleared to 0 by Prepare stage 3.
    s32 miPrepareCarIndex;

    // X360 +0x234 (564). LoadGlobalResources' resumable stage (an 18-case jump table;
    // only 0..6, 9, 10 and 0x11 are live).
    s32 meLoadGlobalResourcesStage;

    // X360 +0x238 (568). The number of receiver-queue responses the current
    // LoadGlobalResources step is waiting for (always 1 in the shipped path); the
    // stage compares mReceiverQueue's event count against it and yields while short.
    s32 miExpectedResponseCount;

    // X360 +0x100E0 (65760). DWARF BrnRaceCarEntityModule.h:339 `BitArray<8u>
    // mabResetThisFrame` -- ADDED 2026-08-18 (wave Q5, cluster G1). Three-point pin: the
    // ActiveRaceCar[8] array ends at +0x100E0 (see maTailPadA0's own comment above),
    // mReceiverQueue starts at +0x100E8, and SetPaddingForResetRaceCars @0x822CEEA8
    // addresses exactly that 8-byte gap (`addis r19,r3,1 ; addi r19,r19,0xE0`), iterating
    // it as ONE 64-bit BitArray word (`ld r9,0(r10)` with the word count fixed at 1) and
    // clearing it whole at the tail (`std r22,0(r19)` with r22 == 0).
    // ⚠️ NO PRODUCER IN THIS TREE YET. The console's writer of the bits is the reset path
    // (a car reset THIS FRAME wants its scene padding dropped); nothing in this tree sets
    // one, so the walk is a correct no-op rather than a wrong answer. Recorded here rather
    // than left inside a pad so the walk reads a NAMED member.
    CgsContainers::BitArray<8u> mabResetThisFrame;

    // X360 +0x100E8 (65768). The module's own reply queue: every request
    // LoadGlobalResources publishes names it as mpUser, so the GameData module posts
    // the completions straight back here. Capacity 4096 is the console gap
    // (mStreamer @+0x11100 - queue @+0x100E8 == 4120 == 4096 + the queue header).
    CgsModule::EventReceiverQueue<4096, 16> mReceiverQueue;

    // X360 +0x18434 / +0x18438 (99380 / 99384). The two resident data tables, taken
    // from the GetVehicleList / GetWheelList replies (the console stores the reply's
    // +0x20 payload word). Prepare stage 3 hands mpVehicleList to the streamer.
    const BrnResource::VehicleList* mpVehicleList;
    const BrnResource::WheelList*   mpWheelList;

    // X360 +0x1843C (99388). The acquired "CarColours" palette resource
    // (BaseResourcePtr::CreateFromHandle off the AcquireResource reply). The payload type
    // is BrnWorld::GlobalColourPalette -- the same resource BrnDriveThruManager and
    // BrnGuiWorldDataController already hold as ResourcePtr<GlobalColourPalette>.
    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mCarColoursResource;

    // [PC diagnostic] whether the CarColours acquire actually resolved. The console has no
    // such flag (it never inspects the ptr); BaseResourcePtr exposes no null query --
    // IsEqual(0) DEREFERENCES its argument -- so the answer is recorded at bind time.
    bool mbCarColoursBound;

    // (RETIRED 2026-08-01: mbBringUpCarRequested + SpawnFirstUnlockedCarBringUp are gone --
    //  HandleResetPlayerCarAction is the real caller of AttachActiveRaceCar now.)

    // (RETIRED 2026-08-01, drivable wave: PromoteAttachedCarToActiveBringUp is gone. The
    //  ATTACHED -> WAITING -> ACTIVE walk is the console's own OnRaceCarResourcesLoaded ->
    //  ActiveRaceCar::OnResourcesLoaded -> PlaceRaceCarOnLoad -> RequestPlaceOnTrack ->
    //  PlaceOnTrackManager::PrePhysicsUpdate -> ResetActiveRaceCar now. What SURVIVES of
    //  it is the render-pose publish below, which is a different and much smaller thing.)

    // [FLAG PC bring-up] NOT an X360 function. The console's ONLY producer of
    // mRenderParams.mBodyTransform is ActiveRaceCar::UpdatePhysicsState @0x822D4418, whose
    // only caller is ReadUpdatedActiveRaceCarDataFromPhysics -- i.e. the physics READBACK,
    // which does not exist on this build (no physics module). Without it an ACTIVE car has
    // no render pose at all and does not draw, so retiring the promote wholesale would have
    // made the car INVISIBLE rather than drivable. This publishes what UpdatePhysicsState
    // publishes, using the console's own CalcBodyTransform, from the console's own slot
    // (PostPhysicsUpdate). DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics lands.
    void PublishRenderPoseWithoutPhysicsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                                 s32 liActiveRaceCar );

    // [FLAG PC bring-up] NOT an X360 function. The WHEEL half of the pose stand-in.
    // ⭐ Since 2026-08-13 the REAL producer is landed (GetWheelsWorldTransfrom @0x825D8878
    // bodied + WriteOutVehicleStats' SetWheelTransform loop unparked), so this runs ONLY for
    // slots physics does not own (called from PublishRenderPoseWithoutPhysicsBringUp's tail).
    // The old DELETE-WHEN's "mabWheelExists writer" clause resolved to: NO SUCH STORE exists
    // in the XEX -- the console's exists source is the parked deformation leg
    // (UpdateWheelPhysicsState @0x822B8738); see the .cpp banner.
    // DELETE-WHEN the body-pose stand-in is deleted (they retire together).
    void PublishWheelPoseWithoutPhysicsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                                s32 liActiveRaceCar );

    // [FLAG PC bring-up] NOT an X360 function. The REST-POSE LIGHT LOCATORS.
    // SubmitCoronasForRaceCar's whole input is mRenderParams' light-locator block, whose
    // console producer is a three-hop chain that is DEAD end-to-end on this build:
    //   (1) DeformableObject::PrepareLocators @0x825BA010 copies the streamed spec's light
    //       tag list into the live VehicleLocatorData -- committed with its source list
    //       pinned to an EMPTY list (BrnDeformableObject_Lifecycle.cpp, "spec accessors not
    //       exposed"), so miNumLightLocators is 0;
    //   (2) DeformationManager::OutputData @0x826225D8 publishes those tables into the
    //       entity-module output interface -- its committed body emits no locator write at
    //       all and the TU is not even mounted (BrnGame.log: "conductor gate:
    //       DeformationManager::OutputData ... inert");
    //   (3) leg L5 of ReadUpdatedActiveRaceCarDataFromPhysics copies them into RenderParams
    //       -- PARKED (BrnGame.log: "[physics-readback] PARKED deformation legs ...
    //       locator-output copy ...").
    // So with no stand-in the count is 0, the producer's loop runs zero times, and the whole
    // subsystem draws nothing while looking perfectly healthy. This publishes what
    // DeformableObject::UpdateLocator @0x825E0EC8 yields for an UNDAMAGED car -- the streamed
    // spec's own authored locator frames, whose skin-point displacement term is zero at rest
    // -- through the REAL consumer (RenderParams::SetLightLocators). It invents no position:
    // every value comes from the car's shipped StreamedDeformationSpec::mLightTags.
    // DELETE-WHEN leg (3) unparks -- at which point this must go, or it will overwrite the
    // deformed positions with the rest pose every frame.
    void PublishRestPoseLightLocatorsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                              s32 liActiveRaceCar );

    // ========================================================================
    // MODELLED members (pose wave 2026-07-31): the three module flags
    // AttachActiveRaceCar reads. All three are named + placed by the SAME DWARF bool run
    // the render wave fitted (BrnRaceCarEntityModule.h:370..386): mbRenderCarsDuringCrash
    // is DWARF entry :377 at +99147, so entry :370 -- the first bool of the run -- is at
    // 99147 - 7 == +99140, which is exactly the byte AttachActiveRaceCar copies into
    // ActiveRaceCar::mbIsInGameMode. That makes the render wave's two-point fit a
    // three-point fit anchored at both ends of the run.
    // ========================================================================

    // X360 +0x18344 (99140). DWARF BrnRaceCarEntityModule.h:370.
    bool mbIsInGameMode;

    // X360 +0x186C9 (100041) / +0x186D0 (100048). DWARF BrnRaceCarEntityModule.h:444/:447
    // (mbInCarSelectScreen, mbInCarModScreen, meCarSelectResetType(4),
    // mbCarSelectDontStreamAudio -- 100041 + 1 + 2 pad + 4 == 100048, an exact fit).
    // AttachActiveRaceCar forwards mbCarSelectDontStreamAudio into ActiveRaceCar::Attach
    // and uses `(!mbInCarSelectScreen || !mbCarSelectDontStreamAudio) && IsPlayerDriven()`
    // as RaceCarStreamer::AddVehicleData's "stream this car's audio" flag.
    bool mbInCarSelectScreen;
    bool mbCarSelectDontStreamAudio;

    // ========================================================================
    // MODELLED members (race-car streamer wave 2026-07-31). Same additive rule as the
    // block above: the console offsets are recorded per member, the x64 offsets are not
    // load-bearing (named-member parity).
    // ========================================================================

    // X360 +0x11100 (69888). The per-car asset director. Every function that reaches it
    // in the console asm uses `this + 0x11100` as the receiver -- AttachActiveRaceCar,
    // DetachActiveRaceCar, UpdateStreaming, SendStreamerEvents, OnRaceCarResourcesLoaded.
    // DWARF BrnRaceCarEntityModule.h:343 names it mRaceCarStreamer (Feb-2007's mStreamer
    // is drift). The receiver queue's 4096 capacity above was derived from the console
    // gap 0x11100 - 0x100E8 == 4120, so these two are the same layout fact.
    RaceCarStreamer mRaceCarStreamer;

    // X360 +0x17850 (96336). Every console call site uses `this + 96336` as the receiver
    // (RaceCarEntityModule::PostSceneUpdate @0x822FE3F0 and PrePhysicsUpdate @0x82307160).
    // DWARF BrnRaceCarEntityModule.h names it mPlaceOnTrackManager; it owns the
    // request -> line test -> ResetActiveRaceCar walk.
    PlaceOnTrackManager mPlaceOnTrackManager;

    // ========================================================================
    // MODELLED members (render wave 2026-07-31): the three render debug switches the
    // dispatch leg reads. Their console offsets are pinned by a TWO-POINT fit against
    // the DWARF bool run (BrnRaceCarEntityModule.h:370..386 -- 17 bools ending just
    // before the 8-byte-aligned mxGameModeFlags @+0x18358):
    //   +99148 mbRenderWheels        -- `if (*(this+99148) && lbRenderAttachedGeometry)`
    //                                   gates RenderRaceCar's wheel block
    //   +99151 mbRenderRaceCarCoronas-- `if (*(this+99151) && ...)` gates
    //                                   SubmitCoronasForRaceCar in GenerateDispatchLists
    // Both land exactly, and the DWARF's spacing between them (+3) matches, so the
    // member one slot before mbRenderWheels is mbRenderCarsDuringCrash -- which is the
    // switch RenderRaceCar tests to gate its whole BODY-PART loop (`if (*(this+99147))`).
    // (The name reads oddly for that role; the offsets are what is attested, so the
    // DWARF name is kept and the role recorded here.)
    bool mbRenderCarsDuringCrash;   // X360 +0x1834B (99147)
    bool mbRenderWheels;            // X360 +0x1834C (99148)
    bool mbRenderRaceCarCoronas;    // X360 +0x1834F (99151)

    // ========================================================================
    // MODELLED members (car-lights wave 2026-08-17): the SEVEN DEBUG DAMAGE /
    // SELF-ILLUMINATION overrides RenderRaceCar @0x822CF6A0 reads at its top
    // (@0x822CFAB4..0x822CFBA4), i.e. the DWARF run that sits IMMEDIATELY BEFORE the
    // paint block below. Same additive rule as every block in this header: the console
    // offset is recorded per member, the x64 offset is not load-bearing.
    //
    // NAMES: the DecFIGS DWARF's own, in its own declaration order
    // (references/DecFIGS/dwarfdump/.../BrnRaceCarEntityModule.h entries :378..:396):
    //     :378 bool      DEBUG_mbOverrideDamage
    //     :381 float32_t DEBUG_mfVehicleScratchAmount
    //     :384 float32_t DEBUG_mfVehicleDustAmount
    //     :387 float32_t DEBUG_mfVehicleCrumpleAmount
    //     :390 float32_t DEBUG_mfSelfIlluminationR
    //     :393 float32_t DEBUG_mfSelfIlluminationG
    //     :396 float32_t DEBUG_mfSelfIlluminationB
    // and :399 is DEBUG_mbOverrideCarColor, which THIS HEADER ALREADY PINS at +100244.
    //
    // OFFSETS: a SEVEN-POINT ASM FIT, every one an address RenderRaceCar bakes in as a
    // literal, and the run closes exactly onto the already-pinned +100244:
    //     0x822CFAB4  addis r29, r20, 2 ; addi r29, r29, -0x7888 ; lbz r11, 0(r29)
    //                                     -> module + 0x18778 == +100216  the bool
    //     0x822CFAEC  ori r9,  r11, 0x877C -> +0x1877C == +100220  scratch  (the
    //                                        RenderParams::DEBUG_OverrideScratchAmount arg)
    //     0x822CFAE0  ori r10, r11, 0x8780 -> +0x18780 == +100224  dust     (lane Z of c23)
    //     0x822CFADC  ori r11, r11, 0x8784 -> +0x18784 == +100228  crumple  (lane X of c23)
    //     0x822CFB74  ori r10, r11, 0x8788 -> +0x18788 == +100232  selfIllumination R
    //     0x822CFB7C  ori r9,  r11, 0x878C -> +0x1878C == +100236  selfIllumination G
    //     0x822CFB88  ori r8,  r11, 0x8790 -> +0x18790 == +100240  selfIllumination B
    //     (next member) DEBUG_mbOverrideCarColor       +0x18794 == +100244  ALREADY PINNED
    // The bool->float alignment pad (100216 + 1 + 3 == 100220) falls exactly where the
    // console offsets say it must, which is what makes this a fit and not a placement.
    //
    // Like the paint overrides below they are dev switches with NO writer on this build
    // (only the debug menu ever set them on the console), so both console branches that
    // read them are inert at retail -- DEBUG_mbOverrideDamage false means the max() in the
    // self-illumination vector is a no-op and the scratch override never runs. They are
    // MODELLED rather than dropped so RenderRaceCar's constant-24 block is the console's
    // whole block, not a slice.
    // ========================================================================
    bool DEBUG_mbOverrideDamage       = false;  // +0x18778 (100216)
    f32  DEBUG_mfVehicleScratchAmount = 0.0f;   // +0x1877C (100220)
    f32  DEBUG_mfVehicleDustAmount    = 0.0f;   // +0x18780 (100224)
    f32  DEBUG_mfVehicleCrumpleAmount = 0.0f;   // +0x18784 (100228)
    f32  DEBUG_mfSelfIlluminationR    = 0.0f;   // +0x18788 (100232)
    f32  DEBUG_mfSelfIlluminationG    = 0.0f;   // +0x1878C (100236)
    f32  DEBUG_mfSelfIlluminationB    = 0.0f;   // +0x18790 (100240)

    // ========================================================================
    // MODELLED members (paint wave 2026-08-02): the two DEBUG COLOUR OVERRIDES
    // UpdateActiveRaceCarColours reads. Every one is NAMED BY THE DWARF
    // (references/DecFIGS/.../BrnRaceCarEntityModule.h entries :480..:489) and every one
    // lands on an offset the asm of 0x822CF208 bakes in, with no fudging:
    //   +0x18794 (100244) DEBUG_mbOverrideCarColor   `lbzx r11, r27, r20`, r20 = 0x18794
    //   +0x18798 (100248) DEBUG_mfOverridePaintColorR `lfsx f0, r27, 0x18798`
    //   +0x1879C (100252) DEBUG_mfOverridePaintColorG
    //   +0x187A0 (100256) DEBUG_mfOverridePaintColorB
    //   +0x187A4 (100260) DEBUG_mfOverridePearlColorR
    //   +0x187A8 (100264) DEBUG_mfOverridePearlColorG
    //   +0x187AC (100268) DEBUG_mfOverridePearlColorB
    //   +0x187B0 (100272) DEBUG_mbOverrideCarPalette `lbzx r11, r27, 0x187B0`
    //   +0x187B4 (100276) DEBUG_miPaletteIndex       `addis r11,r27,2 ; addi r11,r11,-0x784C`
    //   +0x187B8 (100280) DEBUG_miColourIndex        `addis r31,r27,2 ; addi r31,r31,-0x7848`
    // The DWARF run is contiguous and the two bool->float alignment pads fall exactly where
    // the console offsets say they must (100244 + 4 == 100248; 100268 + 4 == 100272), so this
    // is a ten-point fit, not a placement.
    //
    // They are dev switches with NO writer on this build (nothing outside the debug menu
    // ever set them on the console either), so both blocks are inert and the palette path
    // runs. They are modelled rather than dropped so the reconstruction is the console's
    // whole function: dropping them would have made the body a partial slice for two
    // branches that cost nothing.
    // ========================================================================
    bool DEBUG_mbOverrideCarColor    = false;   // +0x18794 (100244)
    f32  DEBUG_mfOverridePaintColorR = 0.0f;    // +0x18798 (100248)
    f32  DEBUG_mfOverridePaintColorG = 0.0f;    // +0x1879C (100252)
    f32  DEBUG_mfOverridePaintColorB = 0.0f;    // +0x187A0 (100256)
    f32  DEBUG_mfOverridePearlColorR = 0.0f;    // +0x187A4 (100260)
    f32  DEBUG_mfOverridePearlColorG = 0.0f;    // +0x187A8 (100264)
    f32  DEBUG_mfOverridePearlColorB = 0.0f;    // +0x187AC (100268)
    bool DEBUG_mbOverrideCarPalette  = false;   // +0x187B0 (100272)
    s32  DEBUG_miPaletteIndex        = 0;       // +0x187B4 (100276)
    s32  DEBUG_miColourIndex         = 0;       // +0x187B8 (100280)

    // ========================================================================
    // MODELLED member (physics-return-path wave 2026-08-11). Same additive rule as the
    // blocks above -- the console offset is recorded, the x64 offset is not load-bearing.
    // ========================================================================

    // X360 +0x18300 (99072). The module's own district map. Every console call site reaches
    // it as a bare `this + 0x18300`: ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8
    // computes it into r25 (`lis r11,1 ; ori r28,r11,0x8300 ; add r25,r26,r28`) and passes
    // it as ActiveRaceCar::UpdatePhysicsState's third argument, which forwards it untouched
    // to RaceCar::UpdatePositioningData -> WorldMap2D::GetValue -> the car's EDistrict.
    // UpdateOutputInterfaces @0x822F5CF8 copies the same 48-byte span into both active
    // output interfaces. It used to fall inside maTailPadA1b as anonymous filler, which is
    // exactly why UpdateOutputInterfaces' step 1 is recorded there as "nothing to copy FROM".
    //
    // ⚠️ IT IS NEVER Construct()ed ON THIS BUILD. Prepare stage 0 -- the console's only
    // WorldMap2D::Construct call site -- is not reproduced (see Prepare's banner), so this
    // member holds the module's zeroed storage. That is SAFE and it is checked, not assumed:
    // WorldMap2D::GetValue bounds-tests `liX >= muWidth` BEFORE it dereferences mpValues, so
    // a zero-width map returns KU_INVALID_WORLD_MAP_VALUE and UpdatePositioningData maps that
    // to E_DISTRICT_INVALID without touching the null grid pointer. The consequence is real
    // but bounded: every car reports district INVALID until stage 0 lands.
    // DELETE-WHEN Prepare stage 0's WorldMap2D::Construct against the district-map resource
    // lands -- then this member is simply the thing it constructs.
    CgsWorld::WorldMap2D mWorldMap2D;

    // ========================================================================
    // MODELLED members + the one private method of the PLAYER-INPUT wave (2026-08-11), for
    // ProcessPlayerVehicleInput @0x822FFE30 -- the hop that turns the pre-scene pad state into
    // the BrnPlayerDriverControls event the vehicle sim consumes.
    //
    // Same additive rule as every block above: the CONSOLE offset is recorded per member, the
    // x64 offset is not load-bearing (named-member parity). Every name below is the DecFIGS
    // DWARF's own (references/DecFIGS/.../BrnRaceCarEntityModule.h, decl lines noted) and every
    // console offset is one the ARTIST asm of 0x822FFE30 (or a corroborating sibling) bakes in.
    // ========================================================================

    // X360 0x822FFE30. Build this frame's player driver-controls record from the latched pad
    // state + the car/mode state, and AddEvent it into the output buffer's driver queue.
    // ⚠️ SIGNATURE FROM THE ASM, NOT THE PSEUDOCODE (the PPC float-arg trap): the caller
    // PrePhysicsUpdate @0x8230732C sets up `lfs f1, 0(r31) ; mr r5, r24 ; mr r6, r26 ; mr r3,
    // r29` -- the float takes f1 and SKIPS r4, so the parameter list is
    // (f32 lfTimeStep, InputBuffer_PrePhysics*, OutputBuffer_PrePhysics*), NOT the eight ints
    // Hex-Rays prints. lfTimeStep is passed and never read by the callee (no `f1` reference
    // anywhere in the 572-instruction body); it is kept in the signature because it is what the
    // call site passes.
    void ProcessPlayerVehicleInput( f32 lfTimeStep,
                                    const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                                    RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput );

    // X360 0x82304690. ABI is r3=this, f1=time step, r5=input and r6=event queue;
    // the scalar float consumes the skipped r4 GPR argument position on PPC.
    void UpdateBoost( f32 lfTimeStep,
                      const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                      RaceCarEntityModuleIO::GameEventQueue* lpEventQueue );

    // ⭐ X360 0x822FF250 (73 insns) -- PARTIAL SLICE (engine wave 2026-08-12). The eight-slot
    // active-car tick: `for (i = 0; i < 8; ++i) if (maActiveRaceCars[i].IsActive())
    // maActiveRaceCars[i].Update(...)`, then SendAddedForCollisionStateToPhysics.
    // It is the ONLY caller of ActiveRaceCar::Update, which is the ONLY caller of
    // ActiveRaceCar::UpdateEngineState -- i.e. this is the frame slot the ignition hangs off.
    // The two bools Update forwards to UpdateEngineState are loaded HERE, from `this`:
    //     0x822FF318  lbzx r10, r31, 0x18345  ->  mbIsInOnlineGameMode
    //     0x822FF304  lbzx r8,  r31, 0x186C9  ->  mbInCarSelectScreen
    // so they are read from the members directly rather than threaded through the signature.
    // Only the three floats this slice forwards are declared; see the .cpp banner for the
    // console's other five arguments and for SendAddedForCollisionStateToPhysics.
    void UpdateActiveCars( f32 lfTimeStep, f32 lfAcceleration, f32 lfBraking );

    // X360 +0x17890 (96400). DWARF :347. The receiver of
    // `BoostManager::SetBoostEarningEnabled(module + 96400, 1)` in this TU's dirty-trick arm; it
    // also owns the BoostStrategy* at +0x450 (module+97504) that the same body virtual-dispatches
    // IsBoosting() through.
    BoostManager mBoostManager;

    // X360 +0x180F0 (98544). DWARF :355. Asm-literal base:
    // HandlePrepareForModeAction @0x823092F0 calls `CrashPlayManager::Activate(module + 98544,
    // lpActiveRaceCar, lfDifficulty)`. ProcessPlayerVehicleInput reads three of its members
    // through this base (see the manager's own re-seated banner).
    CrashPlayManager mCrashPlayManager;

    // X360 +0x18345/+0x18346 (99141/99142) and +0x1834D/+0x1834E (99149/99150). DWARF :371/:372
    // and :379/:380 -- four more entries of the SAME seventeen-bool run this header already
    // fitted at both ends (mbIsInGameMode :370 @+99140 ... mbRenderRaceCarCoronas :381 @+99151).
    // Each lands on a byte 0x822FFE30 reads:
    //   +99141 mbIsInOnlineGameMode     gates the whole online catch-up / dirty-trick block
    //   +99142 mbOnlineModeJustFinished forces the "park the car" controls (gas 0, handbrake 1,
    //                                   steering hard to +/-1)
    //   +99149 mbSixaxisSteeringEnabled \ together they arm the tilt-steering remap
    //   +99150 mbPaybackSixaxisSteering / (and +99150 is what the SIX_AXIS payback arm clears)
    bool mbIsInOnlineGameMode;          // +0x18345 (99141)
    bool mbOnlineModeJustFinished;      // +0x18346 (99142)
    bool mbCarSelectAllowedInGameMode;  // +0x18347 (99143)
    bool mbSixaxisSteeringEnabled;      // +0x1834D (99149)
    bool mbPaybackSixaxisSteering;      // +0x1834E (99150)

    // X360 +0x18368 (99176). DWARF :395. Read as `lwzx r11, r28, 0x18368` and compared against
    // 0xA / 0xD -- E_MODE_ONLINE_RACE / E_MODE_ONLINE_BURNING_HOME_RUN, which is what identifies
    // the member (the two online modes with per-mode control tweaks).
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;

    // X360 +0x183A8 (99240). DWARF :409. The module's latched copy of this frame's pad state --
    // PreSceneUpdate @0x8230D928 fills it with `memcpy(module + 99240, <pre-scene controls>, 60)`
    // (the memcpy itself is at 0x8230E278) -- ⚠ this citation used to read "@0x822FE3F0", which is
    // PostSceneUpdate's address (see :621), not PreSceneUpdate's; corrected 2026-08-11 --
    // which is BOTH the base and the 60-byte size proof. Eleven of its thirteen floats and three
    // of its eight bools are read by name in ProcessPlayerVehicleInput and every one lands
    // exactly (mfXSensor +16 -> 99256, mfAcceleration +32 -> 99272, mfSteering +44 -> 99284,
    // mbHorn +52 -> 99292, mbReset +55 -> 99295, mbToggle +56 -> 99296, mbIsWheel +58 -> 99298).
    PlayerVehicleControls mPlayerVehicleControls;

    // X360 +0x183E4 / +0x183E8 (99300 / 99304). DWARF :410 / :411. PreSceneUpdate seeds both from
    // the pre-scene input buffer (GetActivePaybackType / GetActivePaybackAggressor) and Prepare
    // seeds them to 3 / -1; ProcessPlayerVehicleInput switches on the type and, for
    // E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM, publishes the aggressor as the driver
    // record's miVehicleIDToMerge.
    BrnNetwork::EPaybackType meActivePaybackType;
    EActiveRaceCarIndex      meActivePaybackAggressor;

    // X360 +0x184C4 / +0x184C8 (99524 / 99528). DWARF :422 / :423. Prepare, Release and Destruct
    // all seed the pair to (-1.0f, false), and the SIX_AXIS payback arm re-seeds them to exactly
    // that alongside `mBoostManager.SetBoostEarningEnabled(true)` -- i.e. "cancel the random
    // boost and let the car earn boost again".
    // ⚠️ FLAG: these two NAMES are a DWARF-ORDER FIT, not a direct attestation. What IS attested
    // is the shape (a f32 seeded -1.0f at 99524 with a `stb`-width flag seeded 0 at 99528 -- the
    // asm is `stfsx f28` / `stbx r26`) and that the DWARF's :421..:431 run closes on
    // mStoredStompees @+99568 with exactly one 12-byte alignment gap when the pair is placed
    // here. No single X360 site names them. Revisit if a body turns up that does.
    f32  mfRandomBoostTime;             // +0x184C4 (99524)
    bool mbRandomBoostOn;               // +0x184C8 (99528)

    // X360 +0x184F0 / +0x185F0 (99568 / 99824). DWARF :433 / :434. The frame's stomped/leaped
    // car list ProcessPlayerVehicleInput forwards to the physics side one entry at a time
    // (VehicleDriverInputInterface::AddTargetAssist). THREE independent pins:
    //   * the loop stride is 0x20 with the id at +0x10 -- exactly StoredStompeeData's
    //     {Vector3 @0, EntityId @16} with the Vector3's 16-byte alignment;
    //   * 99568 + 8 * 32 == 99824, so the count sits immediately after an EIGHT-entry array
    //     (KI_MAX_TARGET_ASSIST_CARS == E_ACTIVE_RACE_CAR_INDEX_COUNT == 8, which is also the
    //     bound AddTargetAssist asserts);
    //   * ProcessLeapedAndStompedCars @0x822BD5B8 -- the producer -- writes `module + 99584`,
    //     i.e. mStoredStompees[0].mEntityId.
    StoredStompeeData mStoredStompees[E_ACTIVE_RACE_CAR_INDEX_COUNT];  // +0x184F0 (99568)
    s32               miStoredStompeeCount;                            // +0x185F0 (99824)

    // X360 +0x18398 (99224). The SIM time step latched once per frame by PreSceneUpdate
    // (`mfTimeStep = lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->
    //  GetCurrentTimeStep()`, asm `*(v52+28) * *(v52+32)`), zeroed when the update set's
    // bit 0 says the sim is paused. UpdateStreaming accumulates it into the streamer's
    // mfTimeSinceLastLoad.
    f32 mfTimeStep;
};

// X360 0x822A34A8. Asserts the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT) (the asm only emits the upper-bound branch since
// the lower bound on a non-negative enum is trivially true), then returns the
// element address: this + 6752 + 7376*index == &maActiveRaceCars[index].
inline ActiveRaceCar*
RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return &maActiveRaceCars[leActiveRaceCarIndex];
}

// X360 0x822A3568. Asserts the index is in [E_GLOBAL_RACE_CAR_INDEX_0,
// E_GLOBAL_RACE_CAR_INDEX_COUNT), then returns the element address:
// this + 592 + 176*index == &maRaceCars[index].
inline RaceCar*
RaceCarEntityModule::GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
               "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    return &maRaceCars[leGlobalRaceCarIndex];
}

// Layout lock. offsetof on the private array members needs member-scope access
// under MSVC, so the X360-proven offsets are asserted here (compiled, never
// called). maRaceCars @+0x250 and maActiveRaceCars @+0x1A60 are the offsets the
// accessor asm bakes in (this + 592 / this + 6752).
inline void RaceCarEntityModule::LockLayout_()
{
    // maRaceCars still lands on the console offset (its leading span is exact and RaceCar
    // has no leading pointer). NOTHING AFTER maActiveRaceCars can be pinned any more: with
    // the ODR fork retired the element is the REAL ActiveRaceCar, whose mpRaceCar widens
    // 4->8 on the x64 gate, so the array stride (console 0x1CD0) and every member the array
    // pushes downstream shift. That is exactly the project's named-member parity rule --
    // the console offsets survive as the per-member comments, which is what a later wave
    // needs; no code reads this module by offset.
    static_assert(offsetof(RaceCarEntityModule, maRaceCars) == 0x250,
                  "maRaceCars @+0x250 (== 592)");
}

}
