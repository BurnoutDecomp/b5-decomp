// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_06.cpp
//
// WAVE Q (breakable props) -- group 6, "the animated-prop leg". ONE chain, three
// bodies, all reconstructed from the BURNOUT_X360_ARTIST.XEX per-address exports:
//
//   BrnWorld::PropEntityModule::UpdateProps      @ 0x822FB2A0  (148 insns)  [ledger]
//   BrnWorld::PropZoneManager::UpdateAnimation   @ 0x822DF3A0  (202 insns)  [non-ledger;
//                                                   declared BrnPropZoneManager.h:351,
//                                                   the link between the other two]
//   BrnWorld::PropEntityInstance::Update         @ 0x822C5608  (398 insns)  [ledger, filed
//                                                   under the altivec.h catch-all]
//
// Authority: the RAW assembly of those three addresses (Hex-Rays mislabels all three --
// it renders the PPC float-argument slot as a phantom leading `int` parameter on every
// one of them). Member names / offsets / enum values come from the committed headers,
// which were themselves DWARF-derived; the assert texts are the verbatim rodata strings
// read out of the image.
//
// ⚠️ NOTE ON FILE PLACEMENT: the wave brief's group guidance asked for
// PropEntityInstance::Update in its own PropEntityInstance_wQ_06.cpp, while the
// ownership rule for this lane says "exactly one new file". The ownership rule wins, so
// all three bodies land here; the two ledger TUs are unaffected (attribution is by
// function, not by file) and splitting the last function out later is a pure file move.
//
// ⚠️ CONSOLE-CONSTANT DISCIPLINE (the recurring killer). Three console strides appear in
// this group's asm and NONE of them is written here:
//   * `idx*5 << 4` == 80  -- the console sizeof(PropEntityInstance); subscripted instead.
//   * `6 * (idx + 0x22314)` -- 6 is the console sizeof(PropEntityRotationParams) and
//     0x22314 (140052) is `offsetof(PropZoneManager, maRotationParams) / 6` on the
//     console; subscripted instead.
//   * `this + 0xC46` in UpdateProps == mZoneManager(0x280) + maProps(0x980) +
//     mu16ZoneIndex(70) -- reached here through the accessor, not through arithmetic.
// ============================================================================

// ---- the wave Q keystone include set (spec §5, verbatim) -------------------------
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"

#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Graphics/BrnCoronaManager.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ---- this group's extra leaves ---------------------------------------------------
#include "rw/math/vpu/matrix44affine_operation.h"  // rw::math::vpu::OrthoNormalize3x3 @0x82203B28
#include "rw/math/vpu/vector3_operation.h"         // Vector3 operator* / + / - (the matrix rows)

#include <cmath>                                   // std::floor (the vrfin round-to-nearest)

namespace BrnWorld
{
    namespace vpu = rw::math::vpu;

    // ========================================================================
    // PropEntityModule::UpdateProps @ 0x822FB2A0
    // ------------------------------------------------------------------------
    // Drain this frame's physics-result queue into the zone manager, then advance every
    // animated (swinging) prop. Called only by PostPhysicsUpdate @0x823031D8, inside its
    // miUpdatePropsPM perfmon bracket.
    //
    // Shape, instruction for instruction:
    //   * `lwz r11, 8(r23)` == BaseEventQueue::miLength, re-read at the head AND the foot
    //     of the loop (0x822FB2C0 and 0x822FB4C4) -- so the bound is the live GetLength(),
    //     not a hoisted copy.
    //   * `bl BrnPhysics__Props__Upd` (an IDA-TRUNCATED symbol, gotcha 6) is
    //     BaseEventQueue<UpdatePropEvent>::GetEvent(i) -- it takes (queue, index) and
    //     returns the element address, which the body then reads at +0x60 (mEntityId),
    //     +0x68 (mbFrozen), +0x40 / +0x50 (the two velocity vectors) and +0x00 (the
    //     transform). That is UpdatePropEvent's layout exactly.
    //   * the owner tripwire at 0x822FB358..0x822FB37C is PropEntityID::AssertIsProp()
    //     folded in ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278);
    //     it is what PropZoneManager::GetZone() runs before the zone read, which is why
    //     the tripwire below is spelled as GetZone() rather than as a raw slot read.
    //   * the unloaded-prop tripwire compares mu16ZoneIndex against 0xFFFF
    //     (KU_UNLOADED_ZONE). GetZone() hands the same halfword back SIGNED, so 0xFFFF
    //     arrives as -1 -- the sentinel survives the narrowing, which is the whole point
    //     of that accessor's signature.
    //
    // ⚠️ NOT A MISSING STEP: at 0x822FB458..0x822FB490 the console builds a LOCAL
    // ResourcePtr on the stack (three self-pointers + BaseResourcePtr::CreateFromHandle
    // @0x822FB490, fed from `mpPropPhysicsDataHeader + 20`, the handle field) and passes
    // `&local` in r7, because the DWARF's UpdateInstance takes a PropPhysicsResourcePtr
    // BY VALUE. The committed UpdateInstance takes the raw `const PropPhysicsDataHeader*`
    // instead; GetPropPhysicsDataHeader() (== mpPropPhysicsDataHeader.GetMemoryResource())
    // is the identical pointer, so the two forms are semantically equivalent and the
    // CreateFromHandle round trip has no observable effect here. (Spec §10 defect 6.)
    //
    // ⚠️ ARGUMENT ORDER IS ABI-PINNED, NOT GUESSED (gotcha 3): the UpdateInstance call at
    // 0x822FB4C0 loads r3=&mZoneManager, r4=mEntityId, r5=&event (the by-value
    // Matrix44Affine's hidden pointer; mTransform is at event+0), r6=mbFrozen,
    // r7=the resource ptr, **r8 is never written**, r9=lpOutput, r10=&mPropEntitySerialiser,
    // f1=mrTimestep, v1=mLinearVelocity(+0x40), v2=mAngularVelocity(+0x50). The skipped r8
    // is the float's consumed-but-unused GPR slot, which is what makes the committed
    // nine-parameter signature the right one.
    void PropEntityModule::UpdateProps( PropEntityIO::OutputBuffer_PostPhysics* lpOutput,
                                        const UpdatePropEventQueue* lpUpdatePropEventQueue )
    {
        for ( s32 liEvent = 0; liEvent < lpUpdatePropEventQueue->GetLength(); ++liEvent )
        {
            const BrnPhysics::Props::UpdatePropEvent& lrEvent =
                lpUpdatePropEventQueue->GetEvent( liEvent );

            // "Trying to update unloaded prop: " -- BrnPropEntityModule.cpp:2251. The console
            // streams the packed id after the prefix (StrStream AppendFormat "0x%X"); the
            // project's CGS_ASSERT takes a plain string, so only the literal prefix survives.
            CGS_ASSERT( mZoneManager.GetZone( lrEvent.mEntityId ) != -1,
                        "Trying to update unloaded prop: " );

            mZoneManager.UpdateInstance( lrEvent.mEntityId,
                                         lrEvent.mTransform,
                                         lrEvent.mLinearVelocity,
                                         lrEvent.mAngularVelocity,
                                         lrEvent.mbFrozen,
                                         GetPropPhysicsDataHeader(),
                                         mrTimestep,
                                         lpOutput,
                                         &mPropEntitySerialiser );
        }

        // 0x822FB4D4..0x822FB4E4: `lfsx f1, r29, 0xD3378` == mrTimestep, r3 == this + 0x280
        // == &mZoneManager. (Again the float rides f1 and skips r4.)
        mZoneManager.UpdateAnimation( mrTimestep );
    }

    // ========================================================================
    // PropZoneManager::UpdateAnimation @ 0x822DF3A0 (202 insns)
    // ------------------------------------------------------------------------
    // Step every animated prop by one timestep. NOT a ledger function of either wave-Q TU
    // (DWARF BrnPropZoneManager.h:172, declared in the committed header this wave), but
    // UpdateProps' only unresolved callee, so it is bodied with its group.
    //
    // The whole body is a set-bit walk over mUsedRotationParams (a BitArray<100>, two u64
    // fields). The console INLINES both halves of that walk, which is why the asm looks
    // bigger than the source:
    //   * 0x822DF3CC..0x822DF414 -- scan the two fields for a non-zero one, then isolate its
    //     lowest set bit with the `(x & -x)` / `cntlzd` / `+63` idiom. That is exactly
    //     CgsContainers::BitArray::GetFirstNonZeroBit() (its GetZeroBitInInt helper carries
    //     that same idiom in its comment).
    //   * 0x822DF4EC..0x822DF6B8 -- walk to the end of the current 64-bit field one bit at a
    //     time through IsBitSet (whose bounds assert, "invalid index : <n> < 100"
    //     @CgsBitArray.h:203, is emitted at THIS call site because the container header is
    //     deliberately assert-free), then jump to the next non-zero field. That is
    //     GetNextNonZeroBit(). The committed helper cannot run past tuNumBits, so the
    //     inlined assert is unreachable in the port and is not reproduced.
    //   * `cmpwi r29, 0x64` at 0x822DF418 pins the bound at 100 == KU_MAX_ROTATION_PARAMS,
    //     and `cmpwi r29, -1` is the KI_INVALID_BITINDEX exit.
    //
    // Per entry: read the params record's miPropIndex (`lhz`/`extsh` at 0x822DF498 -- a
    // SIGNED halfword), resolve that prop slot, tripwire, step it.
    //
    // ⚠️ ODR NOTE (2026-08-18). This is a PropZoneManager member bodied in a PropEntityModule
    // partfile, not at its mirrored home BrnPropZoneManager.cpp. It is the ONLY definition in
    // the tree -- but `cl /c` cannot see a cross-TU duplicate, so whoever fills out
    // BrnPropZoneManager.cpp must NOT mint a second one. The class's own header now records
    // that (BrnPropZoneManager.h:377-383, corrected in round 2 -- it previously claimed the
    // method was "currently unbodied in the tree"). Left here rather than moved: this fixer's
    // write set is the PropEntityModule_wQ_0*.cpp partfiles only, and moving a body across
    // that boundary is a conductor action.
    void PropZoneManager::UpdateAnimation( f32 lfTimeStep )
    {
        for ( s32 liAnimationIndex = mUsedRotationParams.GetFirstNonZeroBit();
              liAnimationIndex != CgsContainers::BitArray<KU_MAX_ROTATION_PARAMS>::KI_INVALID_BITINDEX
                  && static_cast<u32>( liAnimationIndex ) < KU_MAX_ROTATION_PARAMS;
              liAnimationIndex = mUsedRotationParams.GetNextNonZeroBit( liAnimationIndex ) )
        {
            const PropEntityRotationParams& lrRotationParams =
                maRotationParams[liAnimationIndex];

            PropEntityInstance* lpPropToUpdate = &maProps[lrRotationParams.miPropIndex];

            // BrnPropZoneManager.cpp:693, verbatim rodata text. The console reads the byte at
            // prop+0x4B (mi8RotationParamsIndex) and sign-extends it before the compare.
            CGS_ASSERT( lpPropToUpdate->mi8RotationParamsIndex == liAnimationIndex,
                        "lpPropToUpdate->GetRotationParamsIndex() == liAnimationIndex" );

            lpPropToUpdate->Update( lfTimeStep, &lrRotationParams );
        }
    }

    // ========================================================================
    // PropEntityInstance::Update @ 0x822C5608 (398 insns) -- ONE animated prop's swing step
    // ------------------------------------------------------------------------
    // The console inlines an entire VMX sin/cos kernel into this function, which is where
    // most of those 398 instructions go. The kernel is the SAME one four other reconstructed
    // subsystems already identified (range-reduce by 2*pi via vrfin, then a 12-term Taylor
    // pair); its coefficient rows were re-dumped for this body out of the shipped image and
    // agree value-for-value with the bank already committed in
    // GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp.
    // It is de-SIMD'd here TU-locally for the same reason it is there: the committed
    // rw::math::vpu tree flags SinCos as not-committed.
    // ========================================================================

    // Re-dumped 2026-08-18 from BURNOUT_X360_ARTIST.XEX (headless IDA on a DB copy):
    //   0x82000BD0 = { 1, -1/3!, 1/5!, -1/7! }        0x82000C00 = { 1, -1/2!, 1/4!, -1/6! }
    //   0x82000BE0 = { 1/9! .. -1/15! }               0x82000C10 = { 1/8! .. -1/14! }
    //   0x82000BF0 = { 1/17! .. -1/23! }              0x82000C20 = { 1/16! .. -1/22! }
    //   0x82000C60 = { pi, 2pi, 1/pi, 1/(2pi) }  (lanes 1 and 3 are the ones this uses)
    // Which lane feeds which accumulator is MEASURED, not assumed: the odd-power chain seeded
    // by `x + BD0[1]*x^3` (0x822C5784) is SIN and the chain seeded by `1 + C00[1]*x^2`
    // (0x822C57F4, whose seed 1.0f comes from the vspltisw/vupkd3d128 pair) is COS.
    static const f32 KAF_PEI_SIN_COEFFS[12] =
    {
        1.0f,            -1.66666672e-1f,  8.33333377e-3f,  -1.98412701e-4f,
        2.75573188e-6f,  -2.50521080e-8f,  1.60590438e-10f, -7.64716373e-13f,
        // ⚠️ 2026-08-18: this first entry was 2.81145725e-15f, which encodes 0x274A963B --
        // ONE ULP BELOW the shipped image. The image word at 0x82000BF0 is 0x274A963C, and
        // that is also the correct round-to-nearest float32 of 1/17!
        // (1/17! == 2.8114572543455206e-15 -> 0x274A963C, checked by round-trip, and the
        // image byte was dumped independently with headless IDA). Corrected. The same wrong
        // word is still the committed value in the sibling bank at
        // GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp:571,
        // which is NOT this lane's file -- reported to the conductor.
        2.8114573e-15f,  -8.22063525e-18f, 1.95729411e-20f, -3.86817017e-23f
    };
    static const f32 KAF_PEI_COS_COEFFS[12] =
    {
        1.0f,            -5.0e-1f,         4.16666679e-2f,  -1.38888892e-3f,
        2.48015876e-5f,  -2.75573188e-7f,  2.08767570e-9f,  -1.14707456e-11f,
        4.77947733e-14f, -1.56192070e-16f, 4.11031762e-19f, -8.89679139e-22f
    };
    static const f32 KF_PEI_TWO_PI     = 6.28318548f;   // 0x40C90FDB @0x82000C64 (lane 1)
    static const f32 KF_PEI_INV_TWO_PI = 0.159154937f;  // 0x3E22F983 @0x82000C6C (lane 3)

    // r = angle - 2pi*round(angle/2pi) (the console's vrfin round-to-nearest; floor(x+0.5)
    // here differs only at an exact .5 tie on an already-inexact angle), then the two 12-term
    // series over r^2.
    //
    // ⚠️ ROUNDING, STATED HONESTLY (round-1 wording was too strong): the console does NOT
    // evaluate a Horner form. It materialises the explicit odd/even powers r^3, r^5 ... r^23
    // through the vmulfp128 lattice at 0x822C5774..0x822C57CC and accumulates c_k*r^(2k+1)
    // with a chain of FUSED vmaddfp -- one rounding per term. The Horner recurrence below is
    // the SAME TERMS but a DIFFERENT rounding sequence (unfused multiply-add over r^2), so it
    // is a deliberate de-optimisation that differs by a few ULP in the tail terms, not a
    // "float-rounding-equivalent order". The same unfused-vs-fused delta applies to the
    // range reduction (the console uses `vnmsubfp v8, v13, v12, v0` at 0x822C5934) and to the
    // three per-arm row combines (e.g. `vmaddfp` at 0x822C58B4), alongside the fmsubs note
    // already recorded elsewhere in this file.
    static void PeiSinCos( f32 lfAngle, f32& lrfSin, f32& lrfCos )
    {
        const f32 lfR  = lfAngle - KF_PEI_TWO_PI * std::floor( lfAngle * KF_PEI_INV_TWO_PI + 0.5f );
        const f32 lfR2 = lfR * lfR;

        f32 lfSinPoly = KAF_PEI_SIN_COEFFS[11];
        f32 lfCosPoly = KAF_PEI_COS_COEFFS[11];
        for ( s32 li = 10; li >= 0; --li )
        {
            lfSinPoly = lfSinPoly * lfR2 + KAF_PEI_SIN_COEFFS[li];
            lfCosPoly = lfCosPoly * lfR2 + KAF_PEI_COS_COEFFS[li];
        }
        lrfSin = lfR * lfSinPoly;
        lrfCos = lfCosPoly;
    }

    // ------------------------------------------------------------------------
    // The per-frame swing step for ONE animated prop (a gate, a barrier arm, a swinging
    // sign). Its only caller is PropZoneManager::UpdateAnimation @0x822DF4E8, which sets
    // r3 / f1 / r5 only -- r4 is dead, being the float's consumed GPR slot (gotcha 3).
    //
    // ⭐ THE ANGULAR-SPEED CONSTANTS -- and a CORRECTION to the wave spec.
    // 0x822C5658..0x822C56A0 computes, exactly:
    //     speed6 = (u8)mnRotSpeed & ~0xC0        (`lbz` + `rlwinm ...,0,26,23`, mask
    //                                             0xFFFFFF3F; the `lbz` proves the field is
    //                                             read UNSIGNED before the mask -- a signed
    //                                             read would not fold to `& 0x3F`)
    //     rate   = fmsubs( (f32)speed6 * flt_82014AB0, flt_82CDB6AC, flt_82FAD61C )
    //     angle  = rate * lfTimeStep
    //   flt_82014AB0 = 0.015625f == 1/64   (rodata, dumped)
    //   flt_82CDB6AC = 12.568f             (dumped; ~4*pi, a hand-typed constant)
    //   flt_82FAD61C -- the spec records this as 0.0f. That is the value in the STATIC
    //     image, and it is WRONG for the running game: the address has a dynamic
    //     initialiser at 0x82C4C298 (`lfs flt_82CDB6AC ; lfs flt_820147FC ; fmuls ; stfs
    //     flt_82FAD61C`), i.e. it is filled at run time with 12.568f * 0.5f == 6.284f.
    //     flt_820147FC == 0.5f was read out of rodata alongside. This is the same
    //     dynamic-initialiser class of constant the spec itself recovered for
    //     KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET (80.0f) and KVF_PROP_FLOOR (-1000.0f); the
    //     bias was simply read statically and missed. WITH the bias the 6-bit field is a
    //     SIGNED speed spanning about +-1 revolution/second and 32 means "stopped"; without
    //     it every animated prop in the game would spin one way only, at up to 2 rev/s.
    //
    // ⚠️ `fmsubs` is a FUSED multiply-subtract (one rounding). The host expression below is
    // two roundings unless the compiler contracts it -- a sub-ULP difference on a value that
    // is immediately scaled by a timestep, recorded rather than papered over.
    void PropEntityInstance::Update( f32 lfTimeStep, const PropEntityRotationParams* lpRotationParams )
    {
        // 0x822C562C: `lbz r11, 0x4B(r31) ; cmplwi r11, 0xFF` -- the byte form of
        // IsAnimated(), i.e. mi8RotationParamsIndex != -1. BrnPropEntityInstance.cpp:254.
        CGS_ASSERT( mi8RotationParamsIndex != -1, "IsAnimated()" );

        const f32 KF_PEI_ROT_SPEED_STEP  = 0.015625f;         // flt_82014AB0 == 1/64
        const f32 KF_PEI_ROT_SPEED_SCALE = 12.568f;           // flt_82CDB6AC
        const f32 KF_PEI_ROT_SPEED_BIAS  = 12.568f * 0.5f;    // flt_82FAD61C, dynamic init

        // mnRotSpeed packs { axis selector : 2 } into the top bits and { rate : 6 } below.
        const u8  lu8RotSpeed  = static_cast<u8>( lpRotationParams->mnRotSpeed );
        const u32 luRotAxis    = lu8RotSpeed & 0xC0u;
        const u32 luRotSpeed6  = lu8RotSpeed & 0x3Fu;

        const f32 lfAngularSpeed =
            static_cast<f32>( luRotSpeed6 ) * KF_PEI_ROT_SPEED_STEP * KF_PEI_ROT_SPEED_SCALE
            - KF_PEI_ROT_SPEED_BIAS;

        f32 lfRotationAngle = lfAngularSpeed * lfTimeStep;

        // A prop whose two limit bytes are equal has no end stops: it free-runs, and the
        // constraint / ease-in pair is skipped entirely (`beq` at 0x822C56A8).
        if ( lpRotationParams->muMinAngle != lpRotationParams->muMaxAngle )
        {
            // 0x822C56B4: `fcmpu ; bge` past the `fneg`. bge after fcmpu is `bc 4,LT`, which
            // is TAKEN when the compare is unordered -- so this is the NEGATED ORDERED
            // predicate and `x < 0.0f` is the NaN-correct spelling (gotcha 4). Written as the
            // compare rather than as std::fabs on purpose: fabs would clear a NaN's sign bit
            // where the console leaves it alone.
            f32 lfConstraintInput = lfRotationAngle;
            if ( lfConstraintInput < 0.0f )
            {
                lfConstraintInput = -lfConstraintInput;
            }

            // The three out-params come back through r5/r6/r7 (the console reuses the
            // rotation angle's own stack slot for lfMaxAngle, which is why the asm looks like
            // it aliases them). The two calls are strictly sequenced: UpdateConstraints fills
            // the three floats, THEN CalculateEaseInSpeedModulation reads them, and only then
            // are the two results multiplied (`fmuls f0, f1, f11` at 0x822C56F0).
            f32 lfTrueAngle = 0.0f;
            f32 lfMinAngle  = 0.0f;
            f32 lfMaxAngle  = 0.0f;

            const f32 lfConstrainedSpeed = UpdateConstraints( lfConstraintInput,
                                                              &lfTrueAngle,
                                                              &lfMinAngle,
                                                              &lfMaxAngle,
                                                              lpRotationParams );
            const f32 lfEaseIn = CalculateEaseInSpeedModulation( lfTrueAngle,
                                                                 lfMinAngle,
                                                                 lfMaxAngle );
            lfRotationAngle = lfConstrainedSpeed * lfEaseIn;
        }

        // 0x822C570C: the translation row is captured BEFORE anything else touches the
        // matrix, and restored as the very last store of the function (0x822C5C28), after
        // the orthonormalise has written all four rows back.
        const Vector3 lSavedTranslation = mWorldTransform.wAxis;

        f32 lfSin;
        f32 lfCos;
        PeiSinCos( lfRotationAngle, lfSin, lfCos );

        // Rotate the 3x3 about the local axis mnRotSpeed selects. Each arm loads BOTH source
        // rows before storing either (the console holds them in vector registers), and the
        // untouched row is the rotation axis itself. The sign convention is measured, not
        // assumed -- in every arm the FIRST row written is `u*cos + v*sin` and the second is
        // `v*cos - u*sin`, with (u,v) the two rows following the axis cyclically:
        //   axis 0x00 (X): 0x822C58B4 `v0 = row2*sin + row1*cos` -> +0x10,
        //                  0x822C58B8 `v13 = row2*cos - row1*sin` -> +0x20
        //   axis 0x40 (Y): 0x822C5A70 -> +0x20,  0x822C5A74 -> +0x00
        //   axis 0x80/0xC0 (Z): 0x822C5BE0 -> +0x00, 0x822C5BE4 -> +0x10
        const Vector3 lXAxis = mWorldTransform.xAxis;
        const Vector3 lYAxis = mWorldTransform.yAxis;
        const Vector3 lZAxis = mWorldTransform.zAxis;

        if ( luRotAxis == 0x00u )
        {
            mWorldTransform.yAxis = lYAxis * lfCos + lZAxis * lfSin;
            mWorldTransform.zAxis = lZAxis * lfCos - lYAxis * lfSin;
        }
        else if ( luRotAxis == 0x40u )
        {
            mWorldTransform.zAxis = lZAxis * lfCos + lXAxis * lfSin;
            mWorldTransform.xAxis = lXAxis * lfCos - lZAxis * lfSin;
        }
        else
        {
            mWorldTransform.xAxis = lXAxis * lfCos + lYAxis * lfSin;
            mWorldTransform.yAxis = lYAxis * lfCos - lXAxis * lfSin;
        }

        // 0x822C5BF8: `bl rw__math__vpu__OrthoNormalize3x3(&temp, this)`, then four
        // lvx128/stvx128 pairs copying temp's rows 0/0x10/0x20/0x30 back over this. The
        // committed OrthoNormalize3x3 already passes wAxis through untouched, so the explicit
        // restore below is a no-op in the port -- it is kept because the console performs it
        // and because it pins the intent (the swing must never translate the prop).
        mWorldTransform = vpu::OrthoNormalize3x3( mWorldTransform );
        mWorldTransform.wAxis = lSavedTranslation;
    }
}
