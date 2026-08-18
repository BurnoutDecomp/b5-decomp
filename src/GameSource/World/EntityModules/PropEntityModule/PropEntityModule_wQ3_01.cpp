// ============================================================================
// LANDED 2026-08-18 (wave Q round 3, conductor): the two round-2 PARKED bodies
// scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdate{Props,Parts}InScene.cpp merged
// verbatim into this partfile. The ONE blocker both named -- InSceneUpdateInterface::
// SetEntityPosition(EntityId, Vector3), the DWARF-spelled overload -- now exists in
// GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h/.cpp. The two park
// banners below are kept as the derivation record; their "THE EXACT LINE THAT UNBLOCKS"
// sections are historical.
// ============================================================================
// ============================================================================
// PARKED (round 2) -- scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp
//
//   BrnWorld::PropEntityModule::ReplayUpdatePropsInScene @0x822DB370  (355 insns, counted:
//   0x822DB370..0x822DB8F8 inclusive, /4, +1)
//   ledger TUs: GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//               class:BrnWorld::PropEntityModule
//   Sibling: PropEntityModule_01_ReplayUpdatePartsInScene.cpp @0x822DB900
//   Supersedes scratchpad/waveQ/parked/PropEntityModule_03_ReplayUpdatePropsInScene.cpp
//   (whose banner is now STALE -- see "WHAT CHANGED" below).
//
// ============================================================================
// THE EXACT LINE THAT UNBLOCKS THIS FILE  (ONE line; everything else is landed)
// ============================================================================
// File: b5-decomp/src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h
// In `struct InSceneUpdateInterface`, beside the committed
//   `void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);`  (:194)
// add:
//
//     void SetEntityPosition( CgsSceneManager::EntityId lEntityId, Vector3 lPosition );
//
// GROUND (round-2 re-derivation; the round-1 request was asm-only, this is DWARF-backed):
//  * DWARF, and it is DECISIVE -- the DecFIGS dump declares exactly ONE SetEntityPosition
//    on this struct and it is this one:
//      references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
//      CgsSceneManagerIO_SceneUpdate.h:446  ->  `void SetEntityPosition(EntityId, Vector3);`
//      (the DWARF's own recorded source line is CgsSceneManagerIO_SceneUpdate.h:346).
//    It sits between `void RemoveEntity(EntityId, bool);` (:340) and
//    `void SetEntityRadius(EntityId, float32_t);` (:352), both of which this tree already
//    carries in the EntityId form. The committed `(u32, const Matrix44Affine&)` spelling is
//    a MODEL the tree fitted to its matrix-holding callers, not a second overload the
//    original had; its own banner in CgsSceneManagerIO_SceneUpdate.cpp:41-43 says as much
//    ("the X360 receives the position pre-extracted in the v1 vmx arg; the committed
//    (u32, const Matrix44Affine&) signature the consumers call extracts it here via Pos()").
//  * X360: the producer @0x822B1398 takes the position in v1 and stages
//    InEventSetEntityPosition{ mPosition = v1, mEntityId = r4 } -- no instruction in it
//    reads a matrix. Both call sites here hand it a BARE recorded position straight out of
//    the frame (0x822DB894 `lvx128 v0,[element]` -> 0x822DB8B0 `lvx128 v1`), so there is no
//    transform in scope to give the committed form.
//  * ADDITIVE: no committed call site changes meaning (EntityId has an `operator u32`, so
//    the existing `(u32, const Matrix44Affine&)` calls still bind to the matrix overload;
//    the new one differs in the second parameter, which no existing caller supplies).
//
// COMPILE-PROVEN, not asserted. `scratchpad/waveQ2/probe_wq2lander/probe_replayupdate.cpp`
// is this body plus its sibling verbatim -> STATUS=fail with EXACTLY TWO diagnostics, one
// per body, both C2664 on that one call. `probe_replayupdate_stub.cpp` is the same text
// with a probe-local `ProbeSetEntityPosition(InSceneUpdateInterface*, EntityId, Vector3)`
// substituted for those two calls and nothing else changed -> STATUS=pass. So this ONE
// declaration is the whole remaining blocker.
//
// ============================================================================
// WHAT CHANGED SINCE THE ROUND-1 PARK (re-derived per the round-2 brief -- the old banner
// was wrong or stale in FOUR places)
// ============================================================================
//  1. THE FRAME BLOCKER IS GONE. BrnReplays::PropSerialiserFrame now carries nine real
//     BrnReplayArray members (BrnReplayPropSerialiserFrame.h:195-222) with all nineteen
//     offsets pinned by static_assert. No pad, no `bool mbAddedFlagNNNN`.
//  2. `maPropTypeIds` IS NOT THE NAME -- it is `maTypes`, ATTESTED VERBATIM by the streamed
//     assert literal " maTypes.GetLength(): " that PropSerialiserFrame::Read/Write/
//     KeyFrameRead build (BrnReplayPropEntitySerialiser.h:420/:421). The round-1 banner
//     guessed it.
//  3. THE ROUND-1 BANNER ASKED FOR FIVE ARRAYS; THERE ARE NINE. It missed both ORIENTATION
//     arrays (frame +0x1600 and +0x3000), whose muLength bytes are the `mbAddedFlag25E0` /
//     `mbAddedFlag3800` it left sitting in the pad ladder -- so its recipe could not have
//     produced the right sizeof. (Its own list of "the other three flags" does name those
//     two offsets as further 16-byte-lane arrays, so the evidence was there; the REQUEST
//     was short.) Not a defect in this body: it reads neither.
//  4. ⚠️ `Vector3( 0.0f, 0.0f, 0.0f )` IN THE ROUND-1 PARKED TEXT DOES NOT COMPILE, and no
//     round-1 verifier caught it because the parked file was never put through cl. This
//     tree's `Vector3` (b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24) is the
//     aggregate `struct alignas(16) Vector3 { float x, y, z, w; void SetZero(); }` -- it has
//     no three-float constructor. Fixed below to a `SetZero()`d local, which is also the
//     exact console value: `vspltisw128 v126, 0` zeroes ALL FOUR lanes (0x822DB560), and
//     SetZero() zeroes all four. A three-float ctor would have left w undefined.
//
// ============================================================================
// EVERYTHING BELOW IS DECODED FROM THE RAW ASM
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822DB370.json (`assembly`), instruction by
// instruction. Hex-Rays renders this function as an int blob and is not used.
// ============================================================================
//
// ASM SHAPE, in order (prologue r25 = this, r17 = lpOutput -- TWO parameters; 0x822DB390/94
// move exactly r3 and r4 and nothing else):
//
//  1  0x822DB3A8  GetStaticLayout(); `lbz r11, 0x5010(r3)` == the live frame's prop
//                 POSITION-array muLength (static 0x5010 == 0x3A20 mLiveFrame + 0x15F0).
//     0x822DB3C0  `subf r30, r10, r11` == frameCount - muReplayPropsInScene.
//     0x822DB3CC  if negative it becomes the REMOVE count (`neg r26`) and the ADD count
//                 clamps to 0; otherwise REMOVE is 0.
//     0x822DB3F8  assert "liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0"
//                 (module .cpp line 0x914 == 2324). Unreachable-true by construction -- one
//                 of the two is always 0 -- but it is in the shipped source.
//  2  0x822DB414  assert `inScene - remove + add == frameCount` (line 0x919 == 2329), whose
//                 failure path streams "Num already in scene: " (aNumAlreadyInSc, NO leading
//                 space -- verified against the parts sibling's distinct aNumAlreadyInSc_0
//                 which DOES have one) / " Num to add: " / " NumToRemove: " / " Num wanted: ".
//                 Per project convention the streamed message collapses to one CGS_ASSERT on
//                 the leading literal.
//  3  0x822DB558  ADD loop: for each new prop, build the scene id from the CURRENT
//                 muReplayPropsInScene + i (re-read from the member every iteration,
//                 0x822DB574 `lwz r11,0(r14)`) and AddEntity it with a ZERO centre
//                 (0x822DB560 `vspltisw128 v126,0`) and a ZERO radius (flt_82001CC0; its raw
//                 image bytes were dumped == 0x00000000 == 0.0f by the ROUND-1 G3 VERIFIER
//                 in headless IDA -- scratchpad/waveQ/probe_PropEntityModule_3/ -- not by
//                 me this round). The flag word is `li r28, 0x490` ==
//                 PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG, the same one
//                 PropCellManager::AddPropToScene pushes.
//  4  0x822DB5E0  REMOVE loop: peel from the TOP of the range,
//                 `muReplayPropsInScene - i - 1` (0x822DB5EC..0x822DB5F4), RemoveEntity(id, 0).
//  5  0x822DB644  latch muReplayPropsInScene = frameCount (`stw r11, 0(r14)`), then for every
//                 prop in the frame push its recorded position and its type's bounding
//                 radius. Order inside the loop, MEASURED: type id (0x822DB744, frame
//                 +0x25F0) -> ResourcePtr null check + GetType's two asserts + the type
//                 lookup (0x822DB758..0x822DB810) -> position (0x822DB874, frame +0x610) ->
//                 SetEntityPosition -> SetEntityRadius. The loop bound is re-read from the
//                 member every iteration (0x822DB8D0).
//
// ⚠️ FAITHFUL RE-EVALUATION: the console calls GetStaticLayout() again at every use rather
// than caching it, and GetStaticLayout() carries the "Static buffer size is too small"
// assert (its `lwz 0x24(this)` vs 0x7480 test is inlined at each in-loop site, 0x822DB6E4
// and 0x822DB814). The repeated calls below are deliberate -- caching would drop firings.
//
// ⚠️ The scene interface is likewise re-fetched per call (`bl sub_822B9738` ==
// OutputBuffer_PreScene::GetSceneInputInterface, the WRITE-lock handle whose "Not locked for
// writing" tripwire is part of the path).
//
// ---------------------------------------------------------------------------
// RESIDUAL, NOT A BLOCKER: the scene owner byte 0x22 (34) has no enumerator
// ---------------------------------------------------------------------------
// GameSource/World/BrnEntityTypes.h names E_ENTITYTYPE_FIRST_REPLAY_TYPE = 32 and
// E_ENTITYTYPE_REPLAY_RACECAR = 33, and 34 collides with E_ENTITYTYPE_COUNT = 34. The
// literal stands with its citation, in the SAME spelling round-1 group 1 already committed
// at PropEntityModule_wQ_01.cpp:90-91. The part-field values 0 (prop) / 1 (part) are the
// Feb-2007 file-local `{ E_PROP = 0, E_PART = 1 }`.
//
// ---------------------------------------------------------------------------
// CONSOLE-OFFSET DISCIPLINE (gotcha 1)
// ---------------------------------------------------------------------------
// No console offset, stride or size appears in the code. The decode used to READ the asm:
//     this + 0xD3180  mPropEntitySerialiser    (r18)
//     this + 0xD3338  muReplayPropsInScene     (r14; re-loaded every loop iteration)
//     this + 0xCDD88  mpPropPhysicsDataHeader  (r15 = this + 0xD0000 - 0x2278)
//     PropTypeData + 0x44  mfSphereRadius      -> GetBoundingRadius()  (host +0x50)
//     serialiser + 0x20 / +0x24  static buffer / its size (GetStaticLayout's own assert)
//
// ---------------------------------------------------------------------------
// gotchas 3 and 4, checked
// ---------------------------------------------------------------------------
// gotcha 3 (a float rides an FPR and SKIPS its GPR slot): not applicable -- this function
// takes no float parameter, and the two floats it PASSES (AddEntity's 0.0f radius in f1,
// SetEntityRadius's f1) are the last argument of an already-committed signature.
// gotcha 4 (PPC NaN polarity): not applicable -- there is no floating-point compare in the
// function. Every branch is an integer `cmpwi`/`cmplwi`/`cmpw`. The only float values are
// loaded and passed, never tested.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET ------------------------------------------
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"

#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"
#include "GameSource/Replays/BrnReplayArray.h"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
// ---------------------------------------------------------------------------------

namespace BrnWorld
{
    namespace
    {
        // The replay-prop scene entity OWNER byte. 0x22 == 34; see the banner -- the enum
        // home (GameSource/World/BrnEntityTypes.h) does not name it and 34 currently
        // collides with E_ENTITYTYPE_COUNT, so the literal stands with its citation.
        // Attested by LeaveReplay @0x822C4810 and by both ReplayUpdate*InScene bodies
        // (`oris rN, rM, 0x2200` on an already-shifted entity index).
        const u32 KU_REPLAY_PROP_SCENE_OWNER = 0x22u;

        // The scene entity id's part field: 0 selects the whole prop, 1 selects a shed
        // part (the Feb-2007 file-local `{ E_PROP = 0, E_PART = 1 }`). This body only ever
        // builds prop ids; the part sibling uses 1.
        const u32 KU_REPLAY_SCENE_PART_FIELD_PROP = 0u;
        // The part sibling builds PART ids: 1 (0x822DBAD4 `ori r15, r11, 1` on 0x22000000
        // -> 0x22000001). Feb-2007 file-local { E_PROP = 0, E_PART = 1 }.
        const u32 KU_REPLAY_SCENE_PART_FIELD_PART = 1u;
    }

    // ========================================================================
    // PropEntityModule::ReplayUpdatePropsInScene @0x822DB370
    //
    // Reconcile the scene's replay-prop population against the playback frame the
    // serialiser just Read. Called only from ReplayPreSceneUpdate @0x822EF878, and only
    // while the serialiser IsPlaying().
    // ========================================================================
    void PropEntityModule::ReplayUpdatePropsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        // ---- 1. how many props does the playback frame want in the scene? -----------
        s32 liNumPropsToAddToScene =
            static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPropPositions.muLength ) -
            static_cast<s32>( muReplayPropsInScene );
        s32 liNumPropsToRemoveFromScene = 0;

        if ( liNumPropsToAddToScene < 0 )
        {
            liNumPropsToRemoveFromScene = -liNumPropsToAddToScene;
            liNumPropsToAddToScene      = 0;
        }

        CGS_ASSERT( liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0,
                    "liNumPropsToAddToScene == 0 || liNumPropsToRemoveFromScene == 0" );

        // ---- 2. the two must reconcile exactly ---------------------------------------
        CGS_ASSERT( static_cast<s32>( muReplayPropsInScene )
                        - liNumPropsToRemoveFromScene
                        + liNumPropsToAddToScene
                    == static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()
                                             ->mLiveFrame.maPropPositions.muLength ),
                    "Num already in scene: " );

        // ---- 3. grow the scene population -------------------------------------------
        for ( s32 liEntity = 0; liEntity < liNumPropsToAddToScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPropsInScene + static_cast<u32>( liEntity ),
                           KU_REPLAY_SCENE_PART_FIELD_PROP );

            // 0x822DB560 `vspltisw128 v126, 0` -- ALL FOUR lanes zero. SetZero() is the
            // tree's Vector3 spelling for that; the type has no 3-float constructor.
            Vector3 lCentre;
            lCentre.SetZero();

            lpOutput->GetSceneInputInterface()->AddEntity(
                lEntityId,
                PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG,   // li r28, 0x490
                lCentre,
                0.0f );                                            // flt_82001CC0 == 0.0f
        }

        // ---- 4. shrink it, peeling from the top of the range -------------------------
        for ( s32 liEntity = 0; liEntity < liNumPropsToRemoveFromScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPropsInScene - static_cast<u32>( liEntity ) - 1u,
                           KU_REPLAY_SCENE_PART_FIELD_PROP );

            lpOutput->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );
        }

        // ---- 5. push every recorded prop's position and radius ------------------------
        muReplayPropsInScene = mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPropPositions.muLength;

        for ( u32 luProp = 0; luProp < muReplayPropsInScene; ++luProp )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER, luProp, KU_REPLAY_SCENE_PART_FIELD_PROP );

            // The frame indexes its arrays with a u8 (BrnReplayArray<T,N>::operator[](u8)
            // -- `clrlwi r28, r26, 24` at 0x822DB748).
            const u16 lu16TypeId =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maTypes[ static_cast<u8>( luProp ) ];

            const BrnPhysics::Props::PropTypeData* lpPropTypeData =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lu16TypeId );

            const Vector3& lrPosition =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPropPositions[ static_cast<u8>( luProp ) ];

            lpOutput->GetSceneInputInterface()->SetEntityPosition( lEntityId, lrPosition );
            // PropTypeData console +0x44 == mfSphereRadius (host +0x50) -- by accessor.
            lpOutput->GetSceneInputInterface()->SetEntityRadius( lEntityId,
                                                                 lpPropTypeData->GetBoundingRadius() );
        }
    }


// ---- (from the parts park file's banner) ----------------------------------------
// ============================================================================
// PARKED (round 2) -- scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp
//
//   BrnWorld::PropEntityModule::ReplayUpdatePartsInScene @0x822DB900  (393 insns, counted:
//   0x822DB900..0x822DBF20 inclusive, /4, +1)
//   ledger TUs: GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//               class:BrnWorld::PropEntityModule
//   Sibling: PropEntityModule_01_ReplayUpdatePropsInScene.cpp @0x822DB370
//   Supersedes scratchpad/waveQ/parked/PropEntityModule_03_ReplayUpdatePartsInScene.cpp.
//
// ============================================================================
// THE EXACT LINE THAT UNBLOCKS THIS FILE  (the SAME single line as the sibling)
// ============================================================================
// File: b5-decomp/src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h
// In `struct InSceneUpdateInterface`, beside the committed
//   `void SetEntityPosition(u32 luEntityId, const Matrix44Affine& lTransform);`  (:194)
// add:
//
//     void SetEntityPosition( CgsSceneManager::EntityId lEntityId, Vector3 lPosition );
//
// The full grounding (DWARF CgsSceneManagerIO_SceneUpdate.h:446 declaring exactly this one
// form, source line :346; the v1-only X360 producer @0x822B1398; why it is additive; and the
// two-probe compile proof) is in the SIBLING file's banner -- ONE declaration unblocks both.
// This body's own two call sites are 0x822DBE98..0x822DBED8.
//
// ============================================================================
// WHAT CHANGED SINCE THE ROUND-1 PARK (re-derived per the round-2 brief)
// ============================================================================
//  1. THE FRAME BLOCKER IS GONE -- the nine real BrnReplayArray members landed at
//     BrnReplayPropSerialiserFrame.h:195-222.
//  2. RENAMES APPLIED from the owner spec's paste table (scratchpad/waveQ2/replays.owner.md
//     section 6): `maPartTypeIds` -> `maPartTypes`, `maPartIndices` -> `maPartIds`.
//     Neither of those two names is attested; they are the owner's descriptive spelling and
//     are what the header carries. (Only maPropPositions / maPropOrientations / maTypes are
//     assert-attested.)
//  3. ⚠️ MUST_FIX FROM ROUND 1, APPLIED: the streamed leading fragment of the second assert
//     is " Num already in scene: " WITH A LEADING SPACE for the PARTS function -- a
//     genuinely different rodata literal from the props function's "Num already in scene: ".
//     RE-MEASURED here, not taken on trust: this body loads `aNumAlreadyInSc_0` at
//     0x822DBA10/0x822DBA18 (the export's own inline comment renders it as
//     " Num already in scene: "), where the props sibling loads `aNumAlreadyInSc` at
//     0x822DB47C/0x822DB484 ("Num already in scene: "). IDA's `_0` suffix here is a name
//     collision between two DISTINCT literals, not an alias. Same for the third fragment:
//     this body uses `aNumToRemove` == " Num to remove: " (0x822DBA6C), the props sibling
//     uses `aNumtoremove` == " NumToRemove: " (0x822DB4D8).
//  4. ⚠️ `Vector3( 0.0f, 0.0f, 0.0f )` IN THE ROUND-1 PARKED TEXT DOES NOT COMPILE -- this
//     tree's Vector3 (b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24) is the
//     aggregate `{ float x, y, z, w; void SetZero(); }` with no three-float constructor.
//     Fixed to a SetZero()d local, which is also the exact console value (`vspltisw128
//     v126, 0` at 0x822DBAFC zeroes all four lanes; a 3-float ctor would leave w undefined).
//
// ============================================================================
// DECODED FROM THE RAW ASM .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822DB900.json
// ============================================================================
//
// Structurally identical to ReplayUpdatePropsInScene, with FOUR differences, all measured:
//   * the frame count is the PART position array's muLength (`lbz 0x6A10` at 0x822DB940 /
//     0x822DB9B4 / 0x822DBBE8, static 0x6A10 == 0x3A20 mLiveFrame + 0x2FF0) vs `0x5010`;
//   * the scene id carries part field 1, not 0 (0x822DBAD4 `ori r15, r11, 1` on the
//     0x22000000 word) -- the export's own comment renders the result as 0x22000001;
//   * the assert literals are the "Parts" spellings, at module .cpp lines 0x95C == 2396
//     (0x822DB994) and 0x961 == 2401 (0x822DBAB8), vs 2324 / 2329, plus the two rodata
//     differences in note 3 above;
//   * the radius comes from the PART descriptor, reached through the frame's separate
//     part-INDEX array -- the prop version reads the prop type's own radius.
//
// Prologue: r25 = this, r22 = lpOutput. TWO parameters -- 0x822DB920/24 move exactly r3 and
// r4 and nothing else.
//
// Order inside loop 5, MEASURED (0x822DBCE0..0x822DBEF0): part TYPE id (frame +0x3810,
// static 0x7230) -> ResourcePtr null check + GetType's two asserts + the type lookup ->
// part INDEX (frame +0x3912, static 0x7332) -> the part descriptor -> position (frame
// +0x27F0, static 0x6210) -> SetEntityPosition -> SetEntityRadius.
//
// ⚠️ FAITHFUL RE-EVALUATION: like the sibling, the console re-calls GetStaticLayout() at
// every use (its "Static buffer size is too small" assert is inlined at each site,
// 0x822DBDB0 / 0x822DBE20) and re-fetches the write-locked scene interface per call
// (`bl sub_822B9738` == OutputBuffer_PreScene::GetSceneInputInterface). Both are reproduced
// rather than cached, so no assert firing is lost. The loop bound is likewise re-read from
// the member every iteration (0x822DBEF4..0x822DBF00).
//
// ---------------------------------------------------------------------------
// CONSOLE-OFFSET AND STRIDE DISCIPLINE (gotcha 1) -- READ THIS BEFORE EDITING
// ---------------------------------------------------------------------------
// ⚠️ THE PART-DESCRIPTOR STRIDE IS A CONSOLE VALUE. At 0x822DBE30..0x822DBE3C the asm
// resolves the part descriptor as `maParts + 48 * partIndex`
// (`rotlwi r9,r11,1 ; add r11,r11,r9 ; slwi r11,r11,4` == index*3*16 == index*48).
// 48 is the CONSOLE sizeof(BrnPhysics::Props::PropPartTypeData); on this x64 host it is 64
// (pinned by that class's own _AssertLayout). The body below subscripts
// `lpPropTypeData->GetParts()[ lu16PartIndex ]` and NEVER computes an offset.
//
// Everything else, provenance only -- no console offset appears in the code:
//     this + 0xD3180  mPropEntitySerialiser    (r23, 0x822DB928/2C)
//     this + 0xD333C  muReplayPartsInScene     (r17, 0x822DB93C/48; re-loaded every loop
//                                               iteration -- 0x822DBB10, 0x822DBEFC)
//     this + 0xCDD88  mpPropPhysicsDataHeader
//     PropTypeData     + 0x40  maParts         -> GetParts()          (host +0x48)
//     PropPartTypeData + 0x28  mfSphereRadius  -> GetBoundingRadius() (host +0x30)
//
// ---------------------------------------------------------------------------
// gotchas 3 and 4, checked
// ---------------------------------------------------------------------------
// Neither applies: the function takes no float parameter, and it contains no
// floating-point compare -- every branch is an integer cmpwi/cmplwi/cmpw. Its two floats
// (AddEntity's 0.0f radius, SetEntityRadius's f1) are passed, never tested.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET ------------------------------------------

    // ========================================================================
    // PropEntityModule::ReplayUpdatePartsInScene @0x822DB900
    // The part half of the replay scene reconciliation.
    // ========================================================================
    void PropEntityModule::ReplayUpdatePartsInScene( PropEntityIO::OutputBuffer_PreScene* lpOutput )
    {
        // ---- 1. how many parts does the playback frame want in the scene? -----------
        s32 liNumPartsToAddToScene =
            static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPartPositions.muLength ) -
            static_cast<s32>( muReplayPartsInScene );
        s32 liNumPartsToRemoveFromScene = 0;

        if ( liNumPartsToAddToScene < 0 )
        {
            liNumPartsToRemoveFromScene = -liNumPartsToAddToScene;
            liNumPartsToAddToScene      = 0;
        }

        CGS_ASSERT( liNumPartsToAddToScene == 0 || liNumPartsToRemoveFromScene == 0,
                    "liNumPartsToAddToScene == 0 || liNumPartsToRemoveFromScene == 0" );

        // ---- 2. the two must reconcile exactly ---------------------------------------
        // ⚠️ LEADING SPACE IS DELIBERATE -- aNumAlreadyInSc_0 @0x822DBA18 is a DIFFERENT
        // literal from the props sibling's aNumAlreadyInSc. See banner note 3.
        CGS_ASSERT( static_cast<s32>( muReplayPartsInScene )
                        - liNumPartsToRemoveFromScene
                        + liNumPartsToAddToScene
                    == static_cast<s32>( mPropEntitySerialiser.GetStaticLayout()
                                             ->mLiveFrame.maPartPositions.muLength ),
                    " Num already in scene: " );

        // ---- 3. grow the scene population -------------------------------------------
        for ( s32 liEntity = 0; liEntity < liNumPartsToAddToScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPartsInScene + static_cast<u32>( liEntity ),
                           KU_REPLAY_SCENE_PART_FIELD_PART );

            // 0x822DBAFC `vspltisw128 v126, 0` -- ALL FOUR lanes zero.
            Vector3 lCentre;
            lCentre.SetZero();

            lpOutput->GetSceneInputInterface()->AddEntity(
                lEntityId,
                PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG,   // li r27, 0x490
                lCentre,
                // 0x822DBB04 `lfs f31, flt_82001CC0`. That constant's raw image bytes were
                // dumped == 0x00000000 == 0.0f by the ROUND-1 G3 VERIFIER in headless IDA
                // (scratchpad/waveQ/probe_PropEntityModule_3/), not re-dumped this round.
                0.0f );
        }

        // ---- 4. shrink it, peeling from the top of the range -------------------------
        for ( s32 liEntity = 0; liEntity < liNumPartsToRemoveFromScene; ++liEntity )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER,
                           muReplayPartsInScene - static_cast<u32>( liEntity ) - 1u,
                           KU_REPLAY_SCENE_PART_FIELD_PART );

            lpOutput->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );
        }

        // ---- 5. push every recorded part's position and radius ------------------------
        muReplayPartsInScene = mPropEntitySerialiser.GetStaticLayout()->mLiveFrame.maPartPositions.muLength;

        for ( u32 luPart = 0; luPart < muReplayPartsInScene; ++luPart )
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set( KU_REPLAY_PROP_SCENE_OWNER, luPart, KU_REPLAY_SCENE_PART_FIELD_PART );

            // BrnReplayArray<T,N>::operator[] takes a u8 (`clrlwi r27, r22, 24` @0x822DBCE4).
            const u16 lu16TypeId =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartTypes[ static_cast<u8>( luPart ) ];

            const BrnPhysics::Props::PropTypeData* lpPropTypeData =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lu16TypeId );

            const u16 lu16PartIndex =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartIds[ static_cast<u8>( luPart ) ];

            // ⚠️ CONSOLE STRIDE TRAP: the asm walks `maParts + 48*index`; 48 is the CONSOLE
            // PropPartTypeData size (host 64). Subscript, never base + 48*i.
            const BrnPhysics::Props::PropPartTypeData& lrPartTypeData =
                lpPropTypeData->GetParts()[ lu16PartIndex ];

            const Vector3& lrPosition =
                mPropEntitySerialiser.GetStaticLayout()
                    ->mLiveFrame.maPartPositions[ static_cast<u8>( luPart ) ];

            lpOutput->GetSceneInputInterface()->SetEntityPosition( lEntityId, lrPosition );
            // PropPartTypeData console +0x28 == mfSphereRadius (host +0x30) -- by accessor.
            lpOutput->GetSceneInputInterface()->SetEntityRadius( lEntityId,
                                                                 lrPartTypeData.GetBoundingRadius() );
        }
    }

}
