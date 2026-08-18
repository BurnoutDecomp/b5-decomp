// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ2_01.cpp
//
// WAVE Q ROUND 2 (2026-08-18) -- the replay half of the BREAKABLE-PROPS keystone that
// round 1 reconstructed completely but could not compile.  Round 1 parked three bodies on
// BrnReplays::PropSerialiserFrame; the round-2 replay-frame owner has now landed the
// frame's nine real sub-arrays and its Get{Prop,Part}Transform accessors
// (scratchpad/waveQ2/replays.owner.md), so the prop-render body lands here.
//
// LEDGER TUs (one class, two ids):
//   GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   class:BrnWorld::PropEntityModule
//
// BODIED HERE (1 of 3):
//   BrnWorld::PropEntityModule::RenderReplayProp          @0x822EF968  (77 insns)
//
// STILL PARKED (2 of 3) -- ONE remaining blocker, and it is NOT the frame:
//   BrnWorld::PropEntityModule::ReplayUpdatePropsInScene  @0x822DB370  (355 insns)
//   BrnWorld::PropEntityModule::ReplayUpdatePartsInScene  @0x822DB900  (393 insns)
//     -> scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePropsInScene.cpp
//     -> scratchpad/waveQ2/parked/PropEntityModule_01_ReplayUpdatePartsInScene.cpp
//   Their frame reads are now fully expressible (maPropPositions / maTypes /
//   maPartPositions / maPartTypes / maPartIds all exist).  What they still cannot say is
//   the scene write: both push a BARE POSITION through
//   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::SetEntityPosition, and this
//   tree declares only `SetEntityPosition(u32, const Matrix44Affine&)`.  The DecFIGS DWARF
//   declares exactly one form and it is the other one --
//   references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
//   CgsSceneManagerIO_SceneUpdate.h:446 (source line :346)
//   `void SetEntityPosition(EntityId, Vector3);`.  See the parked banners.
//
// ---- SOURCES ---------------------------------------------------------------------
//  * RAW X360 ASSEMBLY, .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EF968.json, read as
//    `assembly` and decoded instruction by instruction.  (The Hex-Rays pseudocode types
//    this function `_DWORD *(int, unsigned int, int, int, double)` and mislabels the
//    variadic tail -- it is not used for anything below.)  77 instructions COUNTED:
//    0x822EF968..0x822EFA98 inclusive, /4, +1.
//  * scratchpad/waveQ2/replays.owner.md sections 6 and 7 for the landed member spelling.
//  * scratchpad/waveQ/round1/PropEntityModule_G2.md for the round-1 verifier's
//    instruction-by-instruction confirmation of this body (no MUST_FIX was filed against
//    RenderReplayProp itself; the two MUST_FIXes in that report are GetDesiredState's,
//    already committed in PropEntityModule_wQ_02.cpp).
//
// ---- CALLEES (the export's own `xrefs_from`, MEASURED -- every one has a body) -----
//   0x822A4090  BrnReplays::PropEntitySerialiser::GetStaticLayout
//                 -> BrnReplayPropEntitySerialiser.cpp:43
//   0x822AA638  BrnReplayArray<u16,254>::operator[](u8)   [IDA prints this symbol
//                 TRUNCATED as "BrnReplays::DefaultAreDiffere" -- gotcha 6; the real
//                 identity is fixed by its stride-2 / count-at-+0x1FC guard]
//                 -> generic inline in BrnReplayArray.h + the committed explicit
//                    instantiation GameSource/Replays/Array_u16_254_operator_index.cpp
//   0x822BB920  BrnReplays::PropSerialiserFrame::GetPropTransform
//                 -> BrnReplayPropSerialiserFrame_wQ2_owner.cpp:225 (landed this round)
//   0x822DC010  BrnWorld::PropEntityModule::RenderPropAndCoronas
//                 -> BrnPropEntityModule_Render.cpp:429
//   plus CgsDev::Assert::Begin/Fire/EndAssert (CGS_ASSERT) and __savegprlr_24 (helper).
// `xrefs_to` has EXACTLY ONE entry: PropEntityModule::GenerateDispatchLists @0x822FB4F0.
// Note what is NOT in xrefs_from: PropGraphicsManager::GetPropGraphics -- confirming from
// the export itself that the console inlined it (see note 3 below).
//
// ---- WHAT CHANGED SINCE THE PARKED COPY (re-derived, per the round-2 brief) --------
//  1. `maPropTypes` -> `maTypes`.  The round-1 park banner guessed the name; the landed
//     one is ATTESTED VERBATIM by the streamed assert literal " maTypes.GetLength(): "
//     that PropSerialiserFrame::KeyFrameRead/Read/Write build
//     (BrnReplayPropEntitySerialiser.h:420/:421).  The park banner's pad-arithmetic recipe
//     ("shrink maPad27ED by one byte") is moot -- the whole pad ladder is gone.
//  2. The park banner's `Matrix44Affine GetPropTransform(u8) const` request landed exactly
//     as written, at BrnReplayPropSerialiserFrame.h:301.
//  3. THE EXPLICIT `CGS_ASSERT( luType < KU_MAX_PROP_TYPES )` IS DELETED -- it was a
//     double-fire.  MEASURED: the assert the console fires at 0x822EF9F0..0x822EFA0C cites
//     BrnPropZoneManager.cpp:1034 with the literal "luType < BrnPhysics::Props::KU_MAX_PROP"
//     -- i.e. it is PropGraphicsManager::GetPropGraphics' OWN assert, inlined here together
//     with its table read.  That read is FIVE instructions, transcribed verbatim (an
//     earlier banner quoted a three-instruction `lwzx r4,r11,0xD21C4` form that does not
//     exist -- `lwzx` takes two register operands):
//         0x822EFA10  slwi  r11, r31, 3
//         0x822EFA14  lis   r10, 0xD
//         0x822EFA18  add   r11, r11, r29
//         0x822EFA1C  ori   r10, r10, 0x21C4      # 0xD21C4
//         0x822EFA20  lwzx  r4, r11, r10
//     -- in effect `r4 = *(this + 0xD21C4 + 8*type)`; the module-relative 0xD21C4 is
//     mPropGraphicsManager and the ×8 stride is that manager's PropGraphicsReference.
//     The committed de-inlined accessor already carries that identical CGS_ASSERT
//     (BrnPropZoneManager.cpp:203), so restating it here would fire it TWICE where the
//     console fires it once.  Calling GetPropGraphics at this point in the sequence
//     reproduces the console firing exactly (AGENTS.md "undo compiler optimizations").
//
// ---- CONSOLE-OFFSET DISCIPLINE (gotcha 1) -----------------------------------------
// Not one console offset, stride or size appears in the code below.  The decode used to
// READ the asm is recorded here as PROVENANCE ONLY:
//     this + 0x0D3180   mPropEntitySerialiser  (0x822EF990 `addis r31,r29,0xD ; addi 0x3180`)
//     this + 0x0D21C4   mPropGraphicsManager   (0x822EFA1C, indexed `slwi r11,type,3`)
//     staticLayout + 0x3A20  == mLiveFrame     (0x822EF9B4 / 0x822EF9DC)
//     liveFrame    + 0x25F0  == maTypes        (0x822EF9BC)
// Every one of them is reached BY NAME below.
//
// ---- gotchas 3 and 4, checked -----------------------------------------------------
//  * gotcha 3 (a float rides an FPR and SKIPS its GPR slot): this is precisely why the
//    parameter list below has no parameter in the r7 slot -- `f1` (lfLodDistanceZoomScale)
//    consumes it.  The same rule one slot later is why RenderPropAndCoronas' `r9` is empty.
//    Both halves of the map are proved by the forwarding stores at 0x822EFA2C..0x822EFA84,
//    which copy each incoming stack slot to the outgoing one 0x10 bytes higher.
//  * gotcha 4 (PPC NaN polarity): NOT APPLICABLE -- this function performs no floating
//    point compare.  Its only float, f1, is moved (`fmr f1,f31`) and never tested.  The
//    two integer branches are `blt cr6` on an unsigned compare and `beq cr6` on a null
//    test; neither has a NaN reading.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET (spec section 5) --------------------------
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"   // PropGraphicsManager::GetPropGraphics

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                          // PropEntityID::GetEntityIndex

#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"         // GetStaticLayout()
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"          // maTypes / GetPropTransform
#include "GameSource/Replays/BrnReplayArray.h"                                    // BrnReplayArray<T,N>::operator[](u8)

#include "GameSource/Graphics/BrnCoronaManager.h"                                 // BrnSubmissionInterface
// ---------------------------------------------------------------------------------

namespace BrnWorld
{
    // ---- SIGNATURE PIN (gotcha 11) -------------------------------------------------
    // RenderReplayProp is NOT virtual: it is a plain public member of PropEntityModule
    // (BrnPropEntityModule.h -- the `public:` at :264 is the last access specifier before
    // :378), and the export's `xrefs_to` list has EXACTLY ONE entry,
    // PropEntityModule::GenerateDispatchLists @0x822FB4F0 -- a direct `bl`, no vtable slot.
    // So there is no override to bind.  What IS worth pinning is that the definition binds
    // to the DECLARATION
    // at BrnPropEntityModule.h:378 and not to some future overload of the same name: taking
    // a member pointer at the declared type makes a parameter-type drift a compile error
    // here rather than an unbodied declaration at link time.
    namespace
    {
        typedef void ( PropEntityModule::*RenderReplayPropFn )(
            PropEntityID, DispatchFrame*, Matrix44::InParam, Vector3::InParam, f32,
            s32, s32, s32, bool, const ShaderLodInfo*, bool, const void*, bool,
            BrnCoronaManager::BrnSubmissionInterface* );
        const RenderReplayPropFn KP_RENDER_REPLAY_PROP = &PropEntityModule::RenderReplayProp;
    }

    // ========================================================================
    // PropEntityModule::RenderReplayProp  @ 0x822EF968  (77 insns, counted)
    //
    // The playback-time draw of ONE recorded prop: resolve its recorded type and world
    // transform out of the replay frame, look the graphics record up in
    // mPropGraphicsManager, and forward the entire render tail to RenderPropAndCoronas.
    // There is no logic of its own beyond the two lookups and the null early-out.
    //
    // PARAMETER MAP -- this is a MEASURED PER-PARAMETER MAP, not a rule to re-derive from.
    // Every entry below is read straight off the forwarding stores at 0x822EFA2C..0x822EFA84.
    // Stack slots are named by their SLOT BASE; on this big-endian target a 4-byte value
    // lands at slot+4 and a bool at slot+7, which is the address the cited instruction uses.
    //
    // ⚠️ THE ROUND-1/2 "SAVE-AREA RULE" THAT USED TO SIT HERE WAS WRONG AND IS DELETED.
    // It said "the save area starts at incoming-SP + 0x14, 8 bytes per parameter".  0x14 is
    // not 8-byte aligned, and every slot measured below IS (0x60/0x68/0x70/0x78/0x80/0x88
    // incoming), so no 8-byte ladder can start there; 0x14 + 8*9 == 0x5C, one slot short of
    // the measured 0x60.  The rule that DOES reproduce all six incoming and all eight
    // outgoing offsets, checked arithmetically below, is: save area at incoming-SP + 0x08
    // (X360 linkage area = back chain @0x00 + saved LR @0x04); 8 bytes per scalar in
    // DECLARATION order; a __vector4 rides v1, consumes NO GPR, but DOES take a 16-byte,
    // 16-byte-ALIGNED save-area slot; a float rides f1, BURNS its GPR (gotcha 3) and takes
    // its own 8-byte slot.  Worked through for this function:
    //     this@0x08  id@0x10  dispatch@0x18  viewproj@0x20  [align 0x28->0x30]
    //     vector@0x30..0x40  f1@0x40  modelOnly@0x48  opaque@0x50  transparent@0x58
    //     envMap@0x60(byte 0x67)  shaderLod@0x68(word 0x6C)  coronas@0x70(byte 0x77)
    //     vfx@0x78(word 0x7C)  zOnly@0x80(byte 0x87)  iface@0x88(word 0x8C)   -- 6/6 exact.
    // Note the GPR ladder and the save-area ladder are SEPARATE counters here: the vector
    // advances only the save-area ladder, which is why the last three GPR arguments are
    // r8/r9/r10 while their slots are 0x48/0x50/0x58.
    // THE SAME WRONG SENTENCE STILL SITS IN BrnPropEntityModule.h:365-366 (and that header's
    // table at :372-375 uses the same slot+4 labelling as the old table below) -- that file
    // is not this partfile's to edit; the header's owner should apply the identical
    // correction so the wrong rule stops propagating.
    //
    //     incoming                    ->  outgoing (RenderPropAndCoronas)
    //     r4  lPropEntityId           ->  (consumed: GetEntityIndex)
    //     r5  dispatch frame          ->  r7          (0x822EFA44  mr r7, r28)
    //     r6  view-projection         ->  r8          (0x822EFA3C  mr r8, r27)
    //     v1  camera position         ->  v1          (0x822EFA30  vmr128 v1, v127)
    //     f1  lod zoom scale          ->  f1          (0x822EFA48  fmr f1, f31)
    //     r8  model-only list         ->  r10         (0x822EFA34  mr r10, r26)
    //     r9  opaque list             ->  slot 0x60   (0x822EFA40  stw r25, var_DC == +0x64)
    //     r10 transparent list        ->  slot 0x68   (0x822EFA38  stw r24, var_D4 == +0x6C)
    //     slot 0x60 envMap  (bool)    ->  slot 0x70   (0x822EFA7C lbz arg_67 / 0x822EFA80
    //                                                  stb var_C9 == +0x77)
    //     slot 0x68 shaderLodInfo     ->  slot 0x78   (0x822EFA74 lwz arg_6C / 0x822EFA78
    //                                                  stw var_C4 == +0x7C)
    //     slot 0x70 renderCoronas     ->  slot 0x80   (0x822EFA6C lbz arg_77 / 0x822EFA70
    //                                                  stb var_B9 == +0x87)
    //     slot 0x78 VFX prop table    ->  slot 0x88   (0x822EFA64 lwz arg_7C / 0x822EFA68
    //                                                  stw var_B4 == +0x8C)
    //     slot 0x80 zOnly   (bool)    ->  slot 0x90   (0x822EFA58 lbz arg_87 / 0x822EFA60
    //                                                  stb var_A9 == +0x97)
    //     slot 0x88 corona iface      ->  slot 0x98   (0x822EFA2C lwz arg_8C / 0x822EFA50
    //                                                  stw var_A4 == +0x9C)
    //   (var_X displacements are 0x140 - X, the frame being `stwu r1,-0x140`; incoming
    //    arg_X displacements are 0x140 + X off the post-prologue r1, i.e. X off entry SP.)
    //   and the three RenderPropAndCoronas parameters this function SUPPLIES itself:
    //     r4 lpPropGraphics (the manager lookup)
    //     r5 lpTransform    (0x822EFA54 `addi r5, r1, var_A0` -- the 64-byte sret slot
    //                        GetPropTransform filled at 0x822EF9D8/0x822EF9E4)
    //     r6 luPropTypeId   (0x822EFA4C `mr r6, r31`)
    //
    // Return type is void: the epilogue sets no result and the `beq loc_822EFA88`
    // early-out leaves r3 holding a leftover.
    // ========================================================================
    void
    PropEntityModule::RenderReplayProp(
        PropEntityID lPropEntityId,
        DispatchFrame* lpDispatchFrame,
        Matrix44::InParam lCameraViewProjection,
        Vector3::InParam lCameraPosition,
        f32 lfLodDistanceZoomScale,
        s32 liModelOnlyDisplayList,
        s32 liOpaqueList, s32 liTransparentList,
        bool lbRenderingEnvironmentMap,
        const ShaderLodInfo* lpShaderLodInfo,
        bool lbRenderCoronas,
        const void* lpVFXPropTable,
        bool lbUseZOnlyRendering,
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface )
    {
        // 0x822EF9AC `extrwi r30, r4, 14, 8` -- PropEntityID::GetEntityIndex() inlined.
        //
        // ASSERT PARITY, MEASURED so note 3's "one firing, not two" claim is not overstated:
        // the 77-instruction body contains EXACTLY ONE assert triple (BeginAssert /
        // FireAssert / EndAssert occur once each, at 0x822EF9F0 / 0x822EFA08 / 0x822EFA0C),
        // and that one is the luType/KU_MAX_PROP_TYPES assert de-inlined below.  The console's
        // index extraction here is a bare branch-free `extrwi` with no compare and no call,
        // whereas the tree's PropEntityID::GetEntityIndex (BrnPropEntityID.cpp:35-39) opens
        // with AssertIsProp().  That tripwire is therefore an ACCEPTED TREE-CONVENTION
        // ADDITION, not a console firing -- the same convention as the committed sites
        // BrnPropZoneManager.cpp:426/:630 and BrnPropCellManager.cpp:823, and it cannot fire
        // spuriously on this path because the sole caller, GenerateDispatchLists @0x822FB4F0,
        // iterates prop entities (owner byte always 3).  If exact firing parity is ever
        // wanted, read the field without the tripwire instead of changing the console shape.
        const u32 luPropIndex = lPropEntityId.GetEntityIndex();

        // 0x822EF9B0..0x822EF9CC.  The recorded type, out of the LIVE frame's type table.
        // The index is truncated to a byte on the way in (0x822EF9B8 `clrlwi r4,r30,24`) --
        // that is BrnReplayArray<T,N>::operator[](u8)'s own parameter type, and its bounds
        // assert ("Array index out of bounds: ", BrnReplayArray.h) is the one the console
        // fires.  The element load is `lhz` (0x822EF9CC), a zero-extended u16 that the rest
        // of the function carries in a 32-bit register and hands on as the u32
        // luPropTypeId, so the local is a u32 here too.
        const u32 luType =
            mPropEntitySerialiser.GetStaticLayout()
                ->mLiveFrame.maTypes[ static_cast<u8>( luPropIndex ) ];

        // 0x822EF9D0..0x822EF9E4.  GetStaticLayout() is called a SECOND time here in the
        // shipped code (it is a checked accessor, so both calls carry its "Static buffer
        // size is too small" assert); reproduced rather than folded, so no assert firing is
        // lost.  The transform comes back BY VALUE into the 64-byte stack slot the call
        // below then points at (r3 is the hidden sret pointer, r4 is &mLiveFrame, r5 the
        // index -- GetPropTransform does its own `clrlwi r29,r5,24` truncation).
        const Matrix44Affine lPropTransform =
            mPropEntitySerialiser.GetStaticLayout()
                ->mLiveFrame.GetPropTransform( static_cast<u8>( luPropIndex ) );

        // 0x822EF9E8..0x822EFA28.  The console has PropGraphicsManager::GetPropGraphics
        // INLINED here -- its bounds assert (0x822EF9E8 `cmplwi cr6,r31,0x1F4`, fired at
        // 0x822EF9F0 against BrnPropZoneManager.cpp:1034) and then its one-line table read.
        // De-inlined back to the accessor: see the banner note 3 -- restating the assert
        // here would fire it twice.  It really does sit AFTER the transform fetch, and it
        // is NON-GATING: the console asserts and carries straight on into the lookup.
        //
        // No assert on the RESULT -- an unregistered type is simply not drawn.
        const BrnPhysics::Props::PropGraphics* lpPropGraphics =
            mPropGraphicsManager.GetPropGraphics( luType );
        if ( lpPropGraphics == 0 )
        {
            return;                                        // 0x822EFA28 beq loc_822EFA88
        }

        // 0x822EFA2C..0x822EFA84 -- the straight forward.
        RenderPropAndCoronas( lpPropGraphics,
                              &lPropTransform,
                              luType,
                              lpDispatchFrame,
                              lCameraViewProjection,
                              lCameraPosition,
                              lfLodDistanceZoomScale,
                              liModelOnlyDisplayList,
                              liOpaqueList,
                              liTransparentList,
                              lbRenderingEnvironmentMap,
                              lpShaderLodInfo,
                              lbRenderCoronas,
                              lpVFXPropTable,
                              lbUseZOnlyRendering,
                              lpCoronaSubmissionInterface );
    }
}
