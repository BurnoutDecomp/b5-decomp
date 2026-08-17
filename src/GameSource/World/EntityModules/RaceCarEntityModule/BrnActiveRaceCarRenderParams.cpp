#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// ============================================================================
// BrnWorld::ActiveRaceCar::RenderParams member functions, reconstructed from
// BURNOUT_X360.XEX. RenderParams is the per-car visual snapshot the renderer
// reads each frame: per-wheel transforms, packed body-part visibility, the
// cracked-glass shader params, the blues-and-twos (police strobe) state machine,
// and the detached-part render queue. The IO/physics side fills it
// (RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics); the render side
// consumes it (RaceCarEntityModule::RenderRaceCar / SubmitCoronasForRaceCar).
//
// All 13 X360 functions are bodied here:
//   GetWheelTransform                    @ 0x822A3220  ((luWheel+33)<<6)+this  -> &mWheelTransforms[luWheel]
//   GetWheelScaleMatrix                  @ 0x822A31B8  ((luWheel+39)<<6)+this  -> &mWheelScaleTransforms[luWheel]
//   SetWheelScale                        @ 0x822CD170  maWheelScaleMatrices[luWheel] = lrScale
//   IsPartVisible                        @ 0x822B8B60  the inlined BitArray<96>::IsBitSet
//   GetCrackedGlassFractureAmountN       @ 0x822A1E30  4*(n+1286)+this  -> mafCrackedGlassFractureAmount[n]   (byte 5144 + 4n)
//   SetCrackedGlassFractureAmountN       @ 0x822A1D30  store n+1286
//   GetCrackedGlassEqualisationFactorN   @ 0x822A1EA0  4*(n+1294)+this  -> mafCrackedGlassEqualisationFactor[n] (byte 5176 + 4n)
//   SetCrackedGlassEqualisationFactorN   @ 0x822A1DB0  store n+1294
//   GetCrackedGlassScale                 @ 0x822B8410  8*(n+651)+this   -> mavCrackedGlassScaleFactors[n]            (byte 5208 + 8n)
//   SetCrackedGlassScaleFactorsN         @ 0x822B83A0  store 2*n+1302 (two floats)
//   RequestBluesAndTwosStateSwitch       @ 0x822A1C90  strobe-timer accumulate/wrap/toggle on +5132/+5136/+5140/+5141
//   Reset                                @ 0x822E6818  init to just-spawned visual state
//   DEBUG_OverrideScratchAmount          @ 0x822A21B0  W-lane broadcast over the 128 verlet offsets
//                                                      (compiler-unrolled VMX; re-rolled -- wave-2 pass)
//
// The X360-baked d:\p4 ...BrnActiveRaceCar.h file/line cites are discarded per project
// policy; CGS_ASSERT carries the stringized condition + __FILE__/__LINE__. The console
// asserts spell the glass bound "( 0 <= n ) && ( KI_MAX_ACTIVE_RACE_CARS > n )" but the
// actual compiled bound is `n < 8` (8 glass panes); the part bound is `luPart < 96`.
// ============================================================================

namespace BrnWorld
{

// PIN every X360-asm-proven byte offset in the RenderParams layout map. These are the
// ONLY place offsets appear numerically; all member access in the bodies is by name.
// They live inside a member function so offsetof has access to the private members
// (the data members carry a single private access control => RenderParams is still a
// standard-layout type, so offsetof is well-defined).
//
// RenderParams is byte-identical on the x64 gate: its only pointer lives inside
// maDetachedParts' CgsModule::EventQueue base, whose 16-byte alignment absorbs the
// widening (console ptr/max/count + 4 pad vs. x64 ptr + max/count), so maEvents and
// everything after it land on the console offsets.
#define PIN_RP_OFFSETS()                    \
    do {                                                                          \
        static_assert(offsetof(RenderParams, mBodyTransform) == 0, "mBodyTransform @0");                      \
        static_assert(offsetof(RenderParams, maVerletOffsets) == 64, "verlet offsets @64");                   \
        static_assert(offsetof(RenderParams, mWheelTransforms) == 2112, "wheel xforms @2112");                \
        static_assert(offsetof(RenderParams, mWheelScaleTransforms) == 2496, "wheel scales @2496");           \
        static_assert(offsetof(RenderParams, maAxlePositions) == 2880, "axle positions @2880");               \
        static_assert(offsetof(RenderParams, mPaintColour) == 2944, "paint colour @2944");                    \
        static_assert(offsetof(RenderParams, mPearlescentColour) == 2960, "pearlescent @2960");               \
        static_assert(offsetof(RenderParams, maLightLocatorPos) == 2976, "light pos[24] @2976");              \
        static_assert(offsetof(RenderParams, maLightLocatorType) == 3360, "light type[24] @3360");            \
        static_assert(offsetof(RenderParams, mabWheelExists) == 3456, "wheel exists @3456");                  \
        static_assert(offsetof(RenderParams, mfDeformationSquared) == 3484, "deform^2 @3484");                \
        static_assert(offsetof(RenderParams, mBodyPartVisibility) == 3488, "part visibility @3488");          \
        static_assert(offsetof(RenderParams, maDetachedParts) == 3504, "detached queue @3504");               \
        static_assert(offsetof(RenderParams, mLOD) == 5120, "mLOD @5120");                                    \
        static_assert(offsetof(RenderParams, mbCrashing) == 5125, "mbCrashing @5125");                        \
        static_assert(offsetof(RenderParams, mbIsEngineOff) == 5126, "engine off @5126");                     \
        static_assert(offsetof(RenderParams, mbIsBraking) == 5127, "braking @5127");                          \
        static_assert(offsetof(RenderParams, mbIsHidden) == 5131, "hidden @5131");                            \
        static_assert(offsetof(RenderParams, mfLightOpacityFlipFlop) == 5132, "light flipflop @5132");        \
        static_assert(offsetof(RenderParams, mfLightSwitchTimeOut) == 5136, "light timeout @5136");           \
        static_assert(offsetof(RenderParams, mbBluesAndTwosCanSwitchState) == 5140, "b and t can switch @5140");\
        static_assert(offsetof(RenderParams, mbBluesAndTwosActive) == 5141, "b and t active @5141");          \
        static_assert(offsetof(RenderParams, mu8RenderDamageFlags) == 5142, "damage flags @5142");            \
        static_assert(offsetof(RenderParams, mafCrackedGlassFractureAmount) == 5144, "glass fracture @5144"); \
        static_assert(offsetof(RenderParams, mafCrackedGlassEqualisationFactor) == 5176, "glass equal @5176");\
        static_assert(offsetof(RenderParams, mavCrackedGlassScaleFactors) == 5208, "glass scale @5208");      \
        static_assert(sizeof(RenderParams) == 5280, "sizeof == 0x14A0");                                      \
    } while (0)

// ----------------------------------------------------------------------------
// Wheel transforms
// ----------------------------------------------------------------------------

// X360 0x822A3220: &maWheelTransforms[luWheel] via ((luWheel+33)<<6)+this.
Matrix44Affine& ActiveRaceCar::RenderParams::GetWheelTransform(u32 luWheel)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");
    return mWheelTransforms[luWheel];
}

// X360 0x822A31B8: &maWheelScaleMatrices[luWheel] via ((luWheel+39)<<6)+this.
Matrix44Affine& ActiveRaceCar::RenderParams::GetWheelScaleMatrix(u32 luWheel)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");
    return mWheelScaleTransforms[luWheel];
}

// X360 0x822CD170.
//
// ⛔ SIGNATURE CORRECTED (wheel-render wave 2026-08-03). This used to be declared as
// `SetWheelScale(u32, const Matrix44Affine&)` with the note "the scale matrix arrived split
// across the int arg registers on console". It did not: the asm's ONLY value argument is a
// single VECTOR register, `stvx128 v1, r0, r11` at 0x822CD194 spilling v1 and nothing else,
// and the caller (ActiveRaceCar::OnResourcesLoaded @0x822EB2FC) loads it with ONE
// `lvx128 v1, r30, r29` off the deformation spec -- WheelSpec::mScale, a Vector3. The
// Hex-Rays prototype's fifteen int arguments are the usual noise.
//
// The body then BUILDS the matrix: it fills a 4x4 on the stack with flt_82001C98 (1.0f) on
// the diagonal and flt_82001CC0 (0.0f) everywhere else, splats the argument's x/y/z
// (`vspltw v13/v12/v11, v1, 0/1/2`) and vperms each one into its own diagonal slot. That is
// diag(scale.x, scale.y, scale.z, 1) -- so the console stores a SCALE matrix built from a
// scale VECTOR, and the old declaration would have made every caller hand it 64 bytes of
// something else.
void ActiveRaceCar::RenderParams::SetWheelScale(u32 luWheel, const Vector3& lrScale)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");

    Matrix44Affine& lrDest = mWheelScaleTransforms[luWheel];
    lrDest.xAxis.x = lrScale.x; lrDest.xAxis.y = 0.0f;      lrDest.xAxis.z = 0.0f;      lrDest.xAxis.w = 0.0f;
    lrDest.yAxis.x = 0.0f;      lrDest.yAxis.y = lrScale.y; lrDest.yAxis.z = 0.0f;      lrDest.yAxis.w = 0.0f;
    lrDest.zAxis.x = 0.0f;      lrDest.zAxis.y = 0.0f;      lrDest.zAxis.z = lrScale.z; lrDest.zAxis.w = 0.0f;
    lrDest.wAxis.x = 0.0f;      lrDest.wAxis.y = 0.0f;      lrDest.wAxis.z = 0.0f;      lrDest.wAxis.w = 1.0f;
}

// ----------------------------------------------------------------------------
// Body-part visibility (96 parts packed into two 64-bit words)
// ----------------------------------------------------------------------------

// X360 0x822B8B60: load the 64-bit word for this part group, shift the part's bit
// down to bit 0, mask. Word index = luPart>>6, bit index = luPart&63.
bool ActiveRaceCar::RenderParams::IsPartVisible(u8 lu8Part) const
{
    CGS_ASSERT(lu8Part < KU_MAX_BODY_PARTS_PER_RACE_CAR, "( 0 <= n ) && ( 96 > n )");
    return mBodyPartVisibility.IsBitSet(lu8Part);
}

// ChangePartVisibility (DWARF BrnActiveRaceCar.h:170) -- the inlined BitArray Set/UnSetBit.
void ActiveRaceCar::RenderParams::ChangePartVisibility(u8 lu8Part, bool lbVisible)
{
    CGS_ASSERT(lu8Part < KU_MAX_BODY_PARTS_PER_RACE_CAR, "( 0 <= n ) && ( 96 > n )");
    if (lbVisible)
    {
        mBodyPartVisibility.SetBit(lu8Part);
    }
    else
    {
        mBodyPartVisibility.UnSetBit(lu8Part);
    }
}

// MakeAllPartsVisible (DWARF BrnActiveRaceCar.h:178).
void ActiveRaceCar::RenderParams::MakeAllPartsVisible()
{
    for (u32 luPart = 0; luPart < KU_MAX_BODY_PARTS_PER_RACE_CAR; ++luPart)
    {
        mBodyPartVisibility.SetBit(luPart);
    }
}

// ----------------------------------------------------------------------------
// Cracked-glass shader params (8 panes)
// ----------------------------------------------------------------------------

// X360 0x822A1E30: 4*(n+1286)+this == byte 5144 + 4n == &mafCrackedGlassFractureAmount[n].
f32 ActiveRaceCar::RenderParams::GetCrackedGlassFractureAmountN(u32 n) const
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    return mafCrackedGlassFractureAmount[n];
}

// X360 0x822A1D30: store at 4*(n+1286)+this.
void ActiveRaceCar::RenderParams::SetCrackedGlassFractureAmountN(u32 n, f32 lfValue)
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    mafCrackedGlassFractureAmount[n] = lfValue;
}

// X360 0x822A1EA0: 4*(n+1294)+this == byte 5176 + 4n == &mafCrackedGlassEqualisationFactor[n].
f32 ActiveRaceCar::RenderParams::GetCrackedGlassEqualisationFactorN(u32 n) const
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    return mafCrackedGlassEqualisationFactor[n];
}

// X360 0x822A1DB0: store at 4*(n+1294)+this.
void ActiveRaceCar::RenderParams::SetCrackedGlassEqualisationFactorN(u32 n, f32 lfValue)
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    mafCrackedGlassEqualisationFactor[n] = lfValue;
}

// X360 0x822B8410: 8*(n+651)+this == byte 5208 + 8n == &mavCrackedGlassScaleFactors[n]. The
// console returns the 2-float pair through a hidden out-pointer (copies *v7 then v7[1]);
// clean C++ returns a Vector2 by value built from the packed pair.
Vector2 ActiveRaceCar::RenderParams::GetCrackedGlassScale(u32 n) const
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    Vector2 lScale;
    lScale.x = mavCrackedGlassScaleFactors[n].x;
    lScale.y = mavCrackedGlassScaleFactors[n].y;
    lScale.z = 0.0f;
    lScale.w = 0.0f;
    return lScale;
}

// X360 0x822B83A0: store two floats at 2*n+1302 (the packed 8-byte pair).
void ActiveRaceCar::RenderParams::SetCrackedGlassScaleFactorsN(u32 n, const Vector2& lrScale)
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    mavCrackedGlassScaleFactors[n].x = lrScale.x;
    mavCrackedGlassScaleFactors[n].y = lrScale.y;
}

// ----------------------------------------------------------------------------
// Blues-and-twos (police strobe) state machine
// ----------------------------------------------------------------------------

// X360 0x822A1C90: advance both strobe timers by lfDeltaTime each frame. The fast
// timer (B) elapses at 0.1s and arms the strobe; the slow timer (A) wraps at 1.0s.
// When a forced switch is requested and the strobe is armed, toggle the reported
// state and re-zero both timers. The console encodes the toggle with _cntlzw: for a
// 32-bit input, both (cntlzw(v)>>5)&1 and (cntlzw(v)&0x20)!=0 equal (v == 0), i.e. a
// logical NOT of the previous reported state.
bool ActiveRaceCar::RenderParams::RequestBluesAndTwosStateSwitch(f32 lfDeltaTime, bool lbForce)
{
    mfLightSwitchTimeOut += lfDeltaTime;
    if (mfLightSwitchTimeOut > 0.1f)
    {
        mfLightSwitchTimeOut = 0.0f;
        mbBluesAndTwosCanSwitchState = 1;
    }

    mfLightOpacityFlipFlop += lfDeltaTime;
    if (mfLightOpacityFlipFlop > 1.0f)
    {
        mfLightOpacityFlipFlop -= 1.0f;
    }

    if (!lbForce || !mbBluesAndTwosCanSwitchState)
    {
        return mbBluesAndTwosActive != 0;
    }

    // Forced switch with the strobe armed: toggle and reset the timers. Note the
    // console writes the strobe state (+5140) and returns the toggled value but does
    // NOT write +5141 here -- faithful to the asm, mbBluesAndTwosActive is left
    // unchanged and the toggle is computed from its current value.
    const bool lbToggled = (mbBluesAndTwosActive == 0);
    mfLightSwitchTimeOut = 0.0f;
    mfLightOpacityFlipFlop = 0.0f;
    mbBluesAndTwosCanSwitchState = lbToggled ? 1 : 0;
    return lbToggled;
}

// ----------------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------------

// X360 0x822E6818: reset to the just-spawned visual state. Called by
// ActiveRaceCar::Construct and ActiveRaceCar::Prepare.
void ActiveRaceCar::RenderParams::Reset()
{
    PIN_RP_OFFSETS();   // compile-time layout pin (no runtime cost)

    // Body render transform -> identity (the console writes rows [1,0,0,0][0,1,0,0]
    // [0,0,1,0][0,0,0,0]; SetIdentity is the semantic-parity reset used throughout).
    mBodyTransform.SetIdentity();

    // ⭐ [FLAG PC bring-up, 2026-08-12 exploding-panels wave] ZERO THE VERLET SCRATCH.
    //
    // NOT a console store: the X360 Reset does not touch maVerletOffsets, because on the
    // console this array always has a writer -- the deformation system owns it, and an
    // undeformed car simply holds zeros in it. On PC BrnDeformationManager is not mounted,
    // so NOTHING has ever written these 128 vectors: they were whatever the ActiveRaceCar
    // allocation happened to contain.
    //
    // ⚠ BE PRECISE ABOUT WHAT THIS DOES AND DOES NOT FIX. Zeroing the CPU array is only HALF
    // the repair, and on its own it is INERT. The garbage the vertex shader actually read was
    // never this array -- it was the stale D3D9 REGISTER FILE, because slot 22 had no writer
    // at all and an unset external constant is SKIPPED rather than zeroed, leaving c0..c127
    // holding the preceding world draw's ShadowMap_WorldToLight / world matrices. The other
    // half is the upload in RenderRaceCar, which now publishes this array to constant 22;
    // that is what makes the zeros here reach the GPU. Neither change alone is sufficient.
    //
    // Zero is the honest stand-in AND the console's own value for a car that has taken no
    // damage: zero displacement, zero scratch in the w lane. Once BrnDeformationManager lands
    // it becomes the correct seed for a freshly spawned car rather than a stand-in.
    // DELETE-WHEN the deformation system writes this array.
    for (u32 luVerlet = 0; luVerlet < KU_MAX_RACE_CAR_VERLET_POINTS; ++luVerlet)
    {
        maVerletOffsets[luVerlet].SetZero();
    }

    // [FLAG PC bring-up, car+lights step 1] NOT a console store -- on the console the
    // deformation system owns mfDeformationSquared (ActiveRaceCar::UpdateDeformationState
    // @0x822D4A58, parked on this build: BrnGame.log "[physics-readback] PARKED deformation
    // legs"). RenderRaceCar's constant-24 lights-out gate (`mfDeformationSquared >= 0.25` =>
    // lamps off) is the FIRST reader of the member on PC and it had no writer and no
    // initialiser -- it was 0.0f only by accident of storage class (verify F3, lights).
    // Zero = an undamaged car = lamps allowed. DELETE-WHEN the deformation leg lands.
    mfDeformationSquared = 0.0f;

    // Identity wheel transforms + scale matrices (console unrolls this 6-iteration
    // loop; re-rolled here).
    for (u32 luWheel = 0; luWheel < 6; ++luWheel)
    {
        mWheelTransforms[luWheel].SetIdentity();
        mWheelScaleTransforms[luWheel].SetIdentity();
    }

    // The two colour vectors seed to white (the paint / pearlescent shader tints).
    mPaintColour.x = mPaintColour.y = mPaintColour.z = mPaintColour.w = 1.0f;
    mPearlescentColour.x = mPearlescentColour.y = mPearlescentColour.z = mPearlescentColour.w = 1.0f;

    // Six wheel-exists bytes cleared (the console clears one per wheel inside the
    // matrix loop above).
    for (u32 luByte = 0; luByte < 6; ++luByte)
    {
        mabWheelExists[luByte] = false;
    }

    // Body-part visibility. The console seeds BOTH 64-bit fields with the literal
    // 0xB80FFFFFFFF; that is not "all parts visible", it is the authored default part
    // mask, so it is preserved verbatim rather than "corrected" to all-ones.
    mBodyPartVisibility.SetBitField(0, 0xB80FFFFFFFFull);
    mBodyPartVisibility.SetBitField(1, 0xB80FFFFFFFFull);

    // Detached-part render queue: empty, pointing at its embedded storage.
    maDetachedParts.Construct();

    // The just-spawned visual state.
    mLOD                 = static_cast<CgsGraphics::Model::State>(4);
    mbCrashing           = false;
    mbIsHidden           = false;
    mu8RenderDamageFlags = 0;

    // Blues-and-twos strobe: the light flip-flop zeroed, switching armed, strobe off.
    mfLightOpacityFlipFlop       = 0.0f;
    mbBluesAndTwosCanSwitchState = true;
    mbBluesAndTwosActive         = false;

    // Clear all 8 cracked-glass fracture amounts (console unrolls 8x; re-rolled).
    for (u32 luPane = 0; luPane < 8; ++luPane)
    {
        mafCrackedGlassFractureAmount[luPane] = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// DEBUG scratch-amount override
// ----------------------------------------------------------------------------

// X360 0x822A21B0 (wave-2 VMX pass): broadcast lfScratchAmount into the W lane of all
// 128 scratch vectors at +0x40..+0x83F. The console body is a fully compiler-unrolled
// 128-iteration loop (0x1000 bytes, straight-line, no branches, no CR use); each element
// is the classic VectorIntrinsicUnion round-trip (cf. Vector4::SetComponent /
// vector4_type_inline.h):
//     addi    r11, r3, OFF          OFF = 0x40, 0x50, ..., 0x830
//     lvx128  v0, r0, r11           load the whole member vector
//     stvx128 v0, r0, r10           spill it to the 16-byte stack union (sp+0..15)
//     stfs    f1, 0xC(r1)           overwrite byte 12 == big-endian float lane 3 == W
//     lvx128  v0, r0, r10           reload the patched register
//     stvx128 v0, r0, r11           store ALL 16 bytes back to the member
// (The Hex-Rays pseudocode silently dropped every stfs, making the function look like a
// no-op self-copy; the asm is authoritative.) r3 (this) is never modified and blr returns
// it untouched -- no semantic return value. Faithfully re-rolled; iteration order
// (ascending offsets) and the whole-vector store per element are preserved.
void ActiveRaceCar::RenderParams::DEBUG_OverrideScratchAmount(f32 lfScratchAmount)
{
    for (u32 luVector = 0; luVector < 128; ++luVector)
    {
        Vector3Plus lvValue = maVerletOffsets[luVector];  // lvx128: whole register
        lvValue.w = lfScratchAmount;                     // stfs f1 -> byte 12 (lane W)
        maVerletOffsets[luVector] = lvValue;             // stvx128: all 16 bytes back
    }
}

} // namespace BrnWorld
