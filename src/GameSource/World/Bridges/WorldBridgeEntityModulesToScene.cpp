#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"        // BrnPhysics::Props::PropInputInterface (Append)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint ([Q5-restage] diag only)

#include <stdlib.h>                                                               // getenv (BRN_PROP_DIAG, host only)

// WorldModule entity-modules -> scene/physics prepare-phase bridges, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX.
//
// The null tripwires are NON-gating (the X360 falls through after firing the assert). The
// X360-baked d:\p4 file/line pairs are intentionally not reproduced -- CGS_ASSERT stamps
// __FILE__/__LINE__; the X360 source line is noted in a trailing comment. Every function
// tail-forwards the merge call's result, but these bridges are logically void (the returned
// register is an artifact of the tail branch).
//
// FLAG cross-home casts -- ⚠️ NARROWED 2026-08-19 (wave Q5 cluster F3), because as written it
// was no longer true and was being used to justify a cast that is not safe.
// It IS still true for BridgePropModuleToPhysicsModule_Prepare's SOURCE: PropEntityIO::
// OutputBuffer_Prepare exposes its prop-input interface as opaque *Storage, so that merge is
// driven through the named PropInputInterface by reinterpret_cast.
// It is NOT true of the scene-input interfaces any more. RaceCarEntityModuleIO,
// PropEntityIO, WorldEntityIO and TriggerEntityModuleIO all now TYPEDEF their
// SceneInputInterface / SceneInputInterfaceStorage directly to
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (BrnRaceCarEntityModuleIO.h:379,
// BrnPropEntityModuleIO.h:211/:334/:453, BrnWorldEntityModuleIO.h:60,
// BrnTriggerEntityModuleIO.h:41), so their getters already return the destination's own type
// and the casts on those legs were no-ops hiding behind a stale justification. They are gone.
// ⛔ The ONE place where the storage really is opaque -- BrnTrafficIO's buffers -- is exactly
// where the cast must NOT be used: see the park banners on the three traffic legs. A 1-byte /
// 44-byte span reinterpreted as the 25-queue aggregate makes Append walk foreign memory as
// queue headers, which is the retired PhysicsModuleIO::mPropManagerInputInterface bug (below)
// in the opposite direction.

namespace WorldModule
{

// @ 0x827AB410
void BridgePropModuleToPhysicsModule_Prepare(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    // The X360 checks only the prop output buffer, firing the same tripwire twice (the
    // compiler CSE'd two identical consecutive null-asserts into a single branch on a3).
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :76
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :77

    // asm order: fetch the prop output's prop-input interface first, then the physics
    // prop-manager's, then merge source into destination.
    // ⭐ 2026-08-10 (root-cause wave): the DESTINATION cast is GONE. PhysicsModuleIO::
    // InputBuffer::mPropManagerInputInterface was an `unsigned char[1]` slice and this Append
    // wrote the real ~12 KB interface straight through it and out into mGameActionQueue --
    // latent only because no prop is physically registered yet. The member now holds the real
    // type, so the destination is same-type. (The SOURCE stays a documented cross-home cast:
    // PropEntityIO::OutputBuffer_Prepare still exposes its interface as opaque *Storage.)
    const BrnPhysics::Props::PropInputInterface* lpPropSource =
        reinterpret_cast<const BrnPhysics::Props::PropInputInterface*>(
            lpPropOutputBuffer_Prepare->GetPropInputInterface());
    lpPhysicsModuleInputBuffer->GetPropManagerInputInterface()->Append(lpPropSource);
}

// @ 0x827AB388
void BridgePropModuleToSceneModule_Prepare(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnWorld::PropEntityIO::OutputBuffer_Prepare* lpPropOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneInputBuffer != NULL");                   // :58
    CGS_ASSERT(lpPropOutputBuffer_Prepare != 0, "lpPropOutputBuffer_Prepare != NULL");   // :59

    // asm order: fetch the prop output's scene-input interface first, then the scene manager's
    // in-scene-update aggregate, then merge source into destination.
    // (The reinterpret_cast this leg used to carry is dropped: PropEntityIO::
    // OutputBuffer_Prepare::SceneInputInterfaceStorage is a typedef OF
    // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (BrnPropEntityModuleIO.h:334)
    // and OutputBuffer_Prepare::Construct really Constructs + Clears it
    // (BrnPropEntityModuleIO_OutputBuffer_Prepare.cpp:90), so source and destination are the
    // same type and the cast was a no-op. Behaviour unchanged.)
    lpSceneInputBuffer->GetInSceneUpdateInterface()->Append(
        *lpPropOutputBuffer_Prepare->GetSceneInputInterface());
}

// @ 0x827AB300
void BridgeTrafficModuleToSceneModule_Prepare(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutputBuffer_Prepare)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneInputBuffer != NULL");                       // :40
    CGS_ASSERT(lpTrafficOutputBuffer_Prepare != 0, "lpTrafficOutputBuffer_Prepare != NULL"); // :41

    // asm order: fetch the traffic output's scene-input interface first, then the scene manager's
    // in-scene-update aggregate, then merge source into destination.
    //
    // ⛔ PARKED 2026-08-19 (wave Q5 cluster F3) -- THIS LEG WAS A LANDMINE, and it is the same
    // defect class as the two per-frame traffic legs parked below, found while auditing them.
    // It used to be:
    //     Append(reinterpret_cast<const InSceneUpdateInterface&>(
    //                *lpTrafficOutputBuffer_Prepare->GetSceneInputInterface()));
    // BrnTrafficIO::OutputBuffer_Prepare::SceneInputInterface is `struct { unsigned char
    // maBytes[1]; }` sitting at +16 in front of a raw `maPad16To818784` span
    // (BrnTrafficEntityModuleIO.h:526/:542) -- an UNTYPED, NEVER-CONSTRUCTED byte span, not the
    // aggregate. There is no traffic OutputBuffer_Prepare::Construct anywhere in the tree, so
    // the 25 embedded EventQueue headers in that span are whatever the IO stack's previous
    // tenant left (CreateIOBuffer stopped zero-filling in the 2026-08-15 perf pass), and
    // InSceneUpdateInterface::Append reads every one of their mpEvents/miLength pairs.
    // It has never fired only because WorldModule::Prepare reaches it solely on the
    // `!mTrafficEntityModule.Prepare(...)` retry arm and that gate returns true
    // (WorldLinkStubs.cpp:859-873). The moment the real traffic Prepare lands, this Appends a
    // garbage length from a garbage pointer. Parked rather than left armed.
    // RESTORE IN ONE LINE once BrnTrafficEntityModuleIO.h models mSceneInputInterface at +16 as
    // the real InSceneUpdateInterface and OutputBuffer_Prepare::Construct constructs it:
    //     lpSceneInputBuffer->GetInSceneUpdateInterface()->Append(
    //         *lpTrafficOutputBuffer_Prepare->GetSceneInputInterface());
    // [park owner: BrnTrafficEntityModuleIO.h / the traffic IO TU]
    (void)lpSceneInputBuffer;
    (void)lpTrafficOutputBuffer_Prepare;
}

// @ 0x827AB490 -- the PER-FRAME pre-scene merge: every entity module's staged scene
// updates (adds / removes / position + radius updates) go into the scene manager's
// update input buffer, which SceneManagerModule::UpdateScene then fans out to the
// spatial partition and the overlap generator. This is the hop that puts the world's
// streamed instances into the broad-phase, so the frustum query has something to
// return.
//
// The X360 merge ORDER is traffic -> race car -> world entity -> prop -> trigger.
// ⚠️ CITATION CORRECTED 2026-08-19 (wave Q5 cluster F3): the range this banner used to
// cite for that order, "@0x827AB4F4..0x827AB584", is inside the FIVE null-assert blocks,
// not the merges. The five Append legs are at 0x827AB570 / 0x827AB58C / 0x827AB5A8 /
// 0x827AB5C4 / 0x827AB5E0, each `mr r3,<src> ; bl <src GetSceneInputInterface const> ;
// mr r30,r3 ; mr r3,r31 ; bl InputBuffer_Update::GetInSceneUpdateInterface ; mr r4,r30 ;
// bl InSceneUpdateInterface::Append`, running to the epilogue at 0x827AB5FC.
// The order is reproduced, though the merges are independent per-queue appends.
void BridgeEntityModulesToSceneModule_PreScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene* lpTriggerOutputBuffer_PreScene,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
    const BrnWorld::PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene,
    const BrnWorld::WorldEntityIO::OutputBuffer_PreScene* lpWorldEntityOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneModuleInputBuffer != NULL");                       // :98
    CGS_ASSERT(lpTriggerOutputBuffer_PreScene != 0, "lpTriggerOutputBuffer_PreScene != NULL");     // :99
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene != NULL");     // :100
    CGS_ASSERT(lpPropOutputBuffer_PreScene != 0, "lpPropOutputBuffer_PreScene != NULL");           // :101
    CGS_ASSERT(lpWorldEntityOutputBuffer_PreScene != 0, "lpWorldOutputBuffer_PreScene != NULL");   // :102

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
    InSceneUpdateInterface* lpScene = lpSceneInputBuffer->GetInSceneUpdateInterface();

    // ---- LEG 1/5: TRAFFIC (0x827AB570, `mr r3,r25 ; bl sub_8279FD58`) -------------------
    // ⛔ PARKED -- NOT a fabrication-safe leg. The console accessor is recovered (headless
    // IDA this cluster, scratchpad/waveQ5/ida_f3/): sub_8279FD58 is
    // BrnTrafficIO::OutputBuffer_PreScene::GetSceneInputInterface() const -- read-lock
    // (`extrwi r11,r11,1,27`), baked BrnTrafficEntityModuleIO.h line 0xBA == 186, epilogue
    // `addi r3, r28, 0x10` == this + 16. The in-tree OutputBuffer_PreScene
    // (BrnTrafficEntityModuleIO.h:443) has NO member at +16: its first modelled member is
    // mTrafficToRaceCarInterface_PreScene at +63424 behind a pad span.
    // The console layout that the recovered ladder actually implies is
    // mSceneInputInterface@+16 (a real 818768-byte InSceneUpdateInterface) followed by
    // mTriggerManagementInputInterface@+818784 == 16 + 818768 EXACTLY -- so +63424 falls
    // INSIDE the scene interface's span and cannot be a sibling member (that accessor,
    // 0x827A00B0, bakes line 262, not the :185 the header cites, i.e. it belongs to
    // OutputBuffer_PostScene).
    // Landing this leg therefore needs the traffic buffer RE-LAID-OUT in its own header --
    // a member insert plus a pad re-split on a shared, _AssertLayout-pinned class -- which is
    // outside this cluster's "GetSceneInputInterface accessor bodies only" mandate on the
    // traffic IO TU. Casting the existing +63424/+818784 storage to InSceneUpdateInterface
    // would make Append read 25 queue headers out of foreign memory: the same slice-cast
    // corruption class as the retired PhysicsModuleIO mPropManagerInputInterface bug above.
    // COSTS NOTHING TODAY: TrafficEntityModule::PreSceneUpdate is a WorldLinkStubs gate, so
    // nothing stages into that interface.  [park owner: BrnTrafficEntityModuleIO.h]
    (void)lpTrafficOutputBuffer_PreScene;

    // ---- LEG 2/5: RACE CAR (0x827AB58C, `mr r3,r28 ; bl sub_8279D458`) ------------------
    // ⭐ RESTORED 2026-08-19 (wave Q5 cluster F3). The FLAG that stood here claimed the
    // race-car buffer either did not model a scene-input interface or had only a
    // declaration-only accessor whose body was "not reconstructed", and that the module was
    // "inert on this build". All three are now false: OutputBuffer_PreScene::mSceneInputInterface
    // is the real InSceneUpdateInterface, it IS Constructed (X360 +142192 leg of
    // OutputBuffer_PreScene::Construct), and the const read-lock accessor is bodied this
    // cluster in BrnRaceCarEntityModuleIO.cpp from sub_8279D458's own asm.
    // ActiveRaceCar::RemoveFromScene posts into this copy of the interface, so the ghost-car
    // removals only reach the broad phase through this leg.
    lpScene->Append(*lpRaceCarOutputBuffer_PreScene->GetSceneInputInterface());

    // ---- LEG 3/5: WORLD ENTITY (0x827AB5A8, `mr r3,r26 ; bl 0x827A2938`) ---------------
    // (The reinterpret_cast this leg used to carry is gone: WorldEntityIO::SceneInputInterface
    // is a typedef OF CgsSceneManager::SceneManagerIO::InSceneUpdateInterface
    // (BrnWorldEntityModuleIO.h:60), so the getter already returns the destination's own type
    // and the cast was a no-op whose banner justification -- "exposed as opaque *Storage by
    // its own home" -- had gone stale.)
    lpScene->Append(*lpWorldEntityOutputBuffer_PreScene->GetSceneInputInterface());

    // ---- LEG 4/5: PROP (0x827AB5C4, `mr r3,r27 ; bl sub_827A18C8`) ---------------------
    // ⭐ PROP LEG RESTORED 2026-08-12 (prop-BOOT wave, agent B8). The FLAG that stood here
    // said the prop buffer had no reconstructed scene-input accessor and that "no prop is
    // registered with the scene manager" -- both were true when it was written and both are
    // now stale. PropZoneManager::LoadProp -> PropCellManager::AddPropToScene stages a real
    // AddEntity/AddDynamicVolume per spawned prop into this very interface (the boot log's
    // "PROPS-BOOT prop instances arrived: zone 52 -> LoadZone done" proves it runs), the
    // interface is the real InSceneUpdateInterface now and is Constructed by
    // OutputBuffer_PreScene::Construct, and its const read-lock getter is bodied.
    // Dropping this leg is exactly why "[props-gdl] visible=0" -- nothing the prop module
    // staged ever reached the broad phase, so the frustum query could never return a prop
    // entity for the render list.
    // (Cast dropped for the same reason as the world-entity leg: PropEntityIO::
    // OutputBuffer_PreScene::SceneInputInterfaceStorage is a typedef of the real aggregate,
    // BrnPropEntityModuleIO.h:211.)
    lpScene->Append(*lpPropOutputBuffer_PreScene->GetSceneInputInterface());

    // ---- LEG 5/5: TRIGGER (0x827AB5E0, `mr r3,r29 ; bl 0x827A31C8`) --------------------
    // ⭐ RESTORED 2026-08-19 (wave Q5 cluster F3). Same stale FLAG as the race-car leg:
    // TriggerEntityModuleIO::OutputBuffer_PreScene::mSceneInputInterface is the real
    // aggregate (BrnTriggerEntityModuleIO.h:41 typedef), OutputBuffer_PreScene::Construct
    // @0x822EED90 Constructs it (BrnTriggerEntityModuleIO.cpp:29, mounted), and the const
    // read-lock accessor @0x827A31C8 has had a real body all along
    // (BrnTriggerEntityModuleIO_Accessors.cpp:57, mounted) -- nothing was missing but the call.
    // Producer: TriggerEntityModule::ProcessAddTriggerEvents @0x822D8F48 stages
    // AddDynamicVolume + AddEntity + AddVolumeInstance per trigger, bounded by
    // KU_MAX_TRIGGERS == 512 (well inside mAddEntityQueue's 5120 / mAddDynamicVolumeQueue's
    // 1280). It is reached only once TriggerEntityModule::PreSceneUpdate stops being a
    // WorldLinkStubs gate, so this leg is a no-op today and a live one the moment that lands.
    lpScene->Append(*lpTriggerOutputBuffer_PreScene->GetSceneInputInterface());
}

// @ 0x827AB608 -- the post-physics restage (traffic -> race car -> prop -> world
// entity): the position/radius updates the physics step produced, merged into the
// same scene update input.
//
// ⭐⭐ THIS IS THE HOP THE PLAYER'S CAR REACHES THE BROAD PHASE THROUGH. Until 2026-08-19
// every leg here was `(void)`, so ActiveRaceCar::AddToScene / AddToCollision /
// RaceCarEntityModule::GenerateSceneUpdateEvents staged AddEntity / AddDynamicVolume /
// AddVolumeInstance / AddForCollision / SetVolumeInstanceTransform into
// RaceCarEntityModuleIO::OutputBuffer_PostPhysics::mSceneInputInterface every frame and
// NOTHING ever appended them into SceneManagerIO::InputBuffer_Update -- the car had no
// volume instance, so no car-vs-prop pair could ever exist however good the sweeper was.
//
// The four merges are at 0x827AB6C0 (traffic) / 0x827AB6DC (race car) / 0x827AB6F8 (prop) /
// 0x827AB714 (world entity), each the same seven-instruction shape as the pre-scene bridge's,
// running to the epilogue at 0x827AB730. Note the console asserts only FOUR of the five
// pointers (:243/:244/:245/:246) -- there is no world-entity tripwire -- which is reproduced.
void BridgeEntityModulesToScene_PostPhysics(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutputBuffer_PostPhysics,
    const BrnWorld::PropEntityIO::OutputBuffer_PostPhysics* lpPropOutputBuffer_PostPhysics,
    const BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutputBuffer_PostPhysics)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneInputBuffer != 0, "lpSceneModuleInputBuffer != NULL");                            // :243
    CGS_ASSERT(lpTrafficOutputBuffer_PostPhysics != 0, "lpTrafficOutputBuffer_PostPhysics != NULL");    // :244
    CGS_ASSERT(lpRaceCarOutputBuffer_PostPhysics != 0, "lpRaceCarOutputBuffer_PostPhysics != NULL");    // :245
    CGS_ASSERT(lpPropOutputBuffer_PostPhysics != 0, "lpPropOutputBuffer_PostPhysics != NULL");          // :246

    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface InSceneUpdateInterface;
    InSceneUpdateInterface* lpScene = lpSceneInputBuffer->GetInSceneUpdateInterface();

    // ---- LEG 1/4: TRAFFIC (0x827AB6C0, `mr r3,r29 ; bl sub_827A0C20`) ------------------
    // ⛔ PARKED for the same reason as the pre-scene traffic leg, and this one is a MEASURED
    // header defect rather than an absence. sub_827A0C20 is
    // BrnTrafficIO::OutputBuffer_PostPhysics::GetSceneInputInterface() const -- read-lock,
    // baked BrnTrafficEntityModuleIO.h line 0x199 == 409, epilogue `addi r3, r28, 0x2C70`
    // == this + 11376. The tree instead models mSceneInputInterface at +834784 as a 44-byte
    // SceneInputInterfaceStorage, on the strength of 0x827A0B78 (line 406).
    // The full read ladder recovered this cluster settles it:
    //   391 -> +8      394 -> +3488   397 -> +3632   400 -> +6208   403 -> +9824
    //   406 -> +834784 409 -> +11376  412 -> +830144 415 -> +830672 418 -> +834828
    // and 11376 + 818768 (the console sizeof InSceneUpdateInterface) == 830144 EXACTLY --
    // i.e. the scene interface occupies [+11376, +830144) and the header's +834784/44-byte
    // member is something else. Appending a 44-byte span reinterpreted as the 25-queue
    // aggregate would walk foreign memory as queue headers.
    // COSTS NOTHING TODAY (nothing traffic-side stages scene updates on this build); it needs
    // the traffic buffer relaid out in its own header, which this cluster does not own.
    // [park owner: BrnTrafficEntityModuleIO.h -- move mSceneInputInterface to +11376 as the
    //  real aggregate, then this leg is one line.]
    (void)lpTrafficOutputBuffer_PostPhysics;

    // ---- LEG 2/4: RACE CAR (0x827AB6DC, `mr r3,r28 ; bl sub_8279E5D0`) -----------------
    // ⭐ THE LEG THIS WHOLE CLUSTER EXISTS FOR. The const read-lock accessor is bodied this
    // cluster (BrnRaceCarEntityModuleIO.cpp, from sub_8279E5D0's own asm: read-lock bit 4,
    // baked line 575, returns this+8224 == the offset OutputBuffer_PostPhysics::Construct
    // names for its InSceneUpdateInterface::Construct leg).
    {
        const InSceneUpdateInterface& lrRaceCarScene =
            *lpRaceCarOutputBuffer_PostPhysics->GetSceneInputInterface();

        // [DIAG] NOT IN THE X360 BINARY. Opt-in wave-Q5 bring-up probe: set BRN_PROP_DIAG to
        // answer "does the player's car actually reach the scene's update input, and with
        // what?". Read off the SOURCE interface immediately before the Append, so the numbers
        // are exactly what this leg is about to hand the scene manager.
        // Fires at most TWICE: once on the very first restage (which proves the leg runs at
        // all -- expect all zeros) and once on the first restage carrying a non-empty payload
        // (which proves ActiveRaceCar::AddToScene's four posts arrived). If the second line
        // never appears, the fault is upstream in the car's own registration, NOT here.
        // The getenv latch is evaluated once: a per-frame getenv would be a per-frame syscall.
        {
            static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static bool       sbLoggedAny = false;
            static bool       sbLoggedNonEmpty = false;
            if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0
                 && ( !sbLoggedAny || !sbLoggedNonEmpty ) )
            {
                const s32 liAddEntity  = lrRaceCarScene.mAddEntityQueue.GetLength();
                const s32 liAddDynVol  = lrRaceCarScene.mAddDynamicVolumeQueue.GetLength();
                const s32 liAddVolInst = lrRaceCarScene.mAddVolumeInstanceQueue.GetLength();
                const s32 liAddForColl = lrRaceCarScene.mAddForCollisionQueue.GetLength();
                const bool lbNonEmpty =
                    ( liAddEntity | liAddDynVol | liAddVolInst | liAddForColl ) != 0;
                if ( !sbLoggedAny || ( lbNonEmpty && !sbLoggedNonEmpty ) )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[Q5-restage] race-car scene queue lengths: addEntity " << liAddEntity
                        << " addDynVol "  << liAddDynVol
                        << " addVolInst " << liAddVolInst
                        << " addForColl " << liAddForColl
                        << "\n";
                    sbLoggedAny = true;
                    if ( lbNonEmpty )
                        sbLoggedNonEmpty = true;
                }
            }
        }

        lpScene->Append( lrRaceCarScene );
    }

    // ---- LEG 3/4: PROP (0x827AB6F8, `mr r3,r27 ; bl sub_827A1CB8`) ---------------------
    // ⛔ PARKED on ONE declaration + ONE body in a file this cluster does not own.
    // sub_827A1CB8 is PropEntityIO::OutputBuffer_PostPhysics::GetSceneInputInterface() const
    // -- read-lock (`extrwi r11,r11,1,27`), baked BrnPropEntityModuleIO.h line 0x2DC == 732,
    // epilogue `addi r3, r28, 0x860` == this + 2144, which is the SAME member
    // (mSceneInputInterface) and the SAME real InSceneUpdateInterface type the tree already
    // models from Construct's roll-call. Only the const overload is missing: the header
    // declares just the write twin 0x822B9B28 (line 741) at :468, and
    // BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp bodies only that one. Reaching the
    // writer through a const_cast would fire "Not locked for writing" every frame, because
    // WorldModule::Update read-locks every source of this bridge -- so the leg is parked
    // rather than faked.
    // COSTS NOTHING TODAY: every prop producer in the tree writes the PREPARE or PRE-SCENE
    // buffer (PropZoneManager::LoadProp / UpdateInstance, PropCellManager::AddPropToScene /
    // AddPropToContactGeneration, PropEntityModule's Prepare + PreScene stages), and both of
    // those legs are live. It becomes load-bearing the moment a post-physics prop producer
    // lands (a smashed gate's parts moving after the physics step).
    // [park owner: BrnPropEntityModuleIO.h:468 + BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp
    //  -- add `const SceneInputInterface* GetSceneInputInterface() const;` and its read-lock
    //  body, then this leg is one line.]
    (void)lpPropOutputBuffer_PostPhysics;

    // ---- LEG 4/4: WORLD ENTITY (0x827AB714, `mr r3,r26 ; bl sub_827A2F20`) -------------
    // ⭐ RESTORED 2026-08-19 (wave Q5 cluster F3), and the FLAG that parked it was WRONG,
    // not merely stale. It said this getter "guards on the WRITE lock and this bridge runs
    // with the buffer read-locked (the X360 orders its locks differently across the
    // post-physics spine)". The console emits BOTH overloads for this member -- the writer
    // 0x822BA960 (bit 3) and the reader sub_827A2F20 (bit 4, baked BrnWorldEntityModuleIO.h
    // line 251, returns this+0x1020 == 4128) -- and the bridge calls the reader. Only the writer had
    // ever been modelled; there is no lock-order divergence. The const overload is added +
    // bodied this cluster in the world-entity IO TU.
    lpScene->Append( *lpWorldEntityOutputBuffer_PostPhysics->GetSceneInputInterface() );
}

}   // namespace WorldModule
