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
//   GetWheelTransform                    @ 0x822A3220  ((luWheel+33)<<6)+this  -> &maWheelTransforms[luWheel]
//   GetWheelScaleMatrix                  @ 0x822A31B8  ((luWheel+39)<<6)+this  -> &maWheelScaleMatrices[luWheel]
//   SetWheelScale                        @ 0x822CD170  maWheelScaleMatrices[luWheel] = lrScale
//   IsPartVisible                        @ 0x822B8B60  (mau64PartVisibility[p>>6] >> (p&63)) & 1
//   GetCrackedGlassFractureAmountN       @ 0x822A1E30  4*(n+1286)+this  -> mafCrackedGlassFractureAmount[n]   (byte 5144 + 4n)
//   SetCrackedGlassFractureAmountN       @ 0x822A1D30  store n+1286
//   GetCrackedGlassEqualisationFactorN   @ 0x822A1EA0  4*(n+1294)+this  -> mafCrackedGlassEqualisationFactor[n] (byte 5176 + 4n)
//   SetCrackedGlassEqualisationFactorN   @ 0x822A1DB0  store n+1294
//   GetCrackedGlassScale                 @ 0x822B8410  8*(n+651)+this   -> mav2CrackedGlassScale[n]            (byte 5208 + 8n)
//   SetCrackedGlassScaleFactorsN         @ 0x822B83A0  store 2*n+1302 (two floats)
//   RequestBluesAndTwosStateSwitch       @ 0x822A1C90  strobe-timer accumulate/wrap/toggle on +5132/+5136/+5140/+5141
//   Reset                                @ 0x822E6818  init to just-spawned visual state
//   DEBUG_OverrideScratchAmount          @ 0x822A21B0  W-lane broadcast over the 128 scratch vectors
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
// They live inside this member function so offsetof has access to the private members
// (the data members carry a single private access control => RenderParams is still a
// standard-layout type, so offsetof is well-defined).
#define PIN_RP_OFFSETS()                                                                                      \
    do {                                                                                                      \
        static_assert(offsetof(RenderParams, mBodyTransform)                    == 0,    "mBodyTransform @0");            \
        static_assert(offsetof(RenderParams, mav4ScratchVertices)               == 64,   "scratch vectors @64");          \
        static_assert(offsetof(RenderParams, maWheelTransforms)                 == 2112, "maWheelTransforms @2112");      \
        static_assert(offsetof(RenderParams, maWheelScaleMatrices)             == 2496, "maWheelScaleMatrices @2496");   \
        static_assert(offsetof(RenderParams, mv4Field2944)                      == 2944, "mv4Field2944 @2944");           \
        static_assert(offsetof(RenderParams, mv4Field2960)                      == 2960, "mv4Field2960 @2960");           \
        static_assert(offsetof(RenderParams, mau8Field3456)                     == 3456, "mau8Field3456 @3456");          \
        static_assert(offsetof(RenderParams, mau64PartVisibility)              == 3488, "mau64PartVisibility @3488");    \
        static_assert(offsetof(RenderParams, mpDetachedPartRenderQueue)        == 3504, "queue buffer ptr @3504");       \
        /* queue max/count internal sub-offsets (3508/3512 on X360) are pointer-size      \
           dependent on the 64-bit PC gate, so they are intentionally NOT pinned. */       \
        static_assert(offsetof(RenderParams, maDetachedPartRenderQueueStorage) == 3520, "queue storage @3520");          \
        static_assert(offsetof(RenderParams, mu5120Version)                    == 5120, "version word @5120");           \
        static_assert(offsetof(RenderParams, mb5125)                           == 5125, "byte @5125");                   \
        static_assert(offsetof(RenderParams, mb5131)                           == 5131, "byte @5131");                   \
        static_assert(offsetof(RenderParams, mfBluesAndTwosTimerA)            == 5132, "blues/twos timer A @5132");      \
        static_assert(offsetof(RenderParams, mfBluesAndTwosTimerB)            == 5136, "blues/twos timer B @5136");      \
        static_assert(offsetof(RenderParams, mbBluesAndTwosState)             == 5140, "blues/twos state @5140");        \
        static_assert(offsetof(RenderParams, mbBluesAndTwosReturnState)       == 5141, "blues/twos return state @5141"); \
        static_assert(offsetof(RenderParams, mb5142)                           == 5142, "byte @5142");                   \
        static_assert(offsetof(RenderParams, mafCrackedGlassFractureAmount)     == 5144, "glass fracture[8] @5144");     \
        static_assert(offsetof(RenderParams, mafCrackedGlassEqualisationFactor) == 5176, "glass equalisation[8] @5176"); \
        static_assert(offsetof(RenderParams, mav2CrackedGlassScale)            == 5208, "glass scale[8] @5208");         \
    } while (0)

// ----------------------------------------------------------------------------
// Wheel transforms
// ----------------------------------------------------------------------------

// X360 0x822A3220: &maWheelTransforms[luWheel] via ((luWheel+33)<<6)+this.
Matrix44& ActiveRaceCar::RenderParams::GetWheelTransform(u32 luWheel)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");
    return maWheelTransforms[luWheel];
}

// X360 0x822A31B8: &maWheelScaleMatrices[luWheel] via ((luWheel+39)<<6)+this.
Matrix44& ActiveRaceCar::RenderParams::GetWheelScaleMatrix(u32 luWheel)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");
    return maWheelScaleMatrices[luWheel];
}

// X360 0x822CD170: the scale matrix arrived split across the int arg registers on
// console; the asm broadcasts each column and stores a full Matrix44 into slot
// (luWheel+39)<<6. In clean C++ this is a plain Matrix44 copy-assign.
void ActiveRaceCar::RenderParams::SetWheelScale(u32 luWheel, const Matrix44& lrScale)
{
    CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");
    maWheelScaleMatrices[luWheel] = lrScale;
}

// ----------------------------------------------------------------------------
// Body-part visibility (96 parts packed into two 64-bit words)
// ----------------------------------------------------------------------------

// X360 0x822B8B60: load the 64-bit word for this part group, shift the part's bit
// down to bit 0, mask. Word index = luPart>>6, bit index = luPart&63.
bool ActiveRaceCar::RenderParams::IsPartVisible(u8 luPart) const
{
    CGS_ASSERT(luPart < 96, "( 0 <= n ) && ( 96 > n )");
    return (mau64PartVisibility[luPart >> 6] >> (luPart & 63)) & 1;
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

// X360 0x822B8410: 8*(n+651)+this == byte 5208 + 8n == &mav2CrackedGlassScale[n]. The
// console returns the 2-float pair through a hidden out-pointer (copies *v7 then v7[1]);
// clean C++ returns a Vector2 by value built from the packed pair.
Vector2 ActiveRaceCar::RenderParams::GetCrackedGlassScale(u32 n) const
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    Vector2 lScale;
    lScale.x = mav2CrackedGlassScale[n].x;
    lScale.y = mav2CrackedGlassScale[n].y;
    lScale.z = 0.0f;
    lScale.w = 0.0f;
    return lScale;
}

// X360 0x822B83A0: store two floats at 2*n+1302 (the packed 8-byte pair).
void ActiveRaceCar::RenderParams::SetCrackedGlassScaleFactorsN(u32 n, const Vector2& lrScale)
{
    CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
    mav2CrackedGlassScale[n].x = lrScale.x;
    mav2CrackedGlassScale[n].y = lrScale.y;
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
    mfBluesAndTwosTimerB += lfDeltaTime;
    if (mfBluesAndTwosTimerB > 0.1f)
    {
        mfBluesAndTwosTimerB = 0.0f;
        mbBluesAndTwosState = 1;
    }

    mfBluesAndTwosTimerA += lfDeltaTime;
    if (mfBluesAndTwosTimerA > 1.0f)
    {
        mfBluesAndTwosTimerA -= 1.0f;
    }

    if (!lbForce || !mbBluesAndTwosState)
    {
        return mbBluesAndTwosReturnState != 0;
    }

    // Forced switch with the strobe armed: toggle and reset the timers. Note the
    // console writes the strobe state (+5140) and returns the toggled value but does
    // NOT write +5141 here -- faithful to the asm, mbBluesAndTwosReturnState is left
    // unchanged and the toggle is computed from its current value.
    const bool lbToggled = (mbBluesAndTwosReturnState == 0);
    mfBluesAndTwosTimerB = 0.0f;
    mfBluesAndTwosTimerA = 0.0f;
    mbBluesAndTwosState = lbToggled ? 1 : 0;
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

    // Identity wheel transforms + scale matrices (console unrolls this 6-iteration
    // loop; re-rolled here).
    for (u32 luWheel = 0; luWheel < 6; ++luWheel)
    {
        maWheelTransforms[luWheel].SetIdentity();
        maWheelScaleMatrices[luWheel].SetIdentity();
    }

    // Two all-ones vectors the console seeds at +2944/+2960 (presumed default scale).
    mv4Field2944.x = mv4Field2944.y = mv4Field2944.z = mv4Field2944.w = 1.0f;
    mv4Field2960.x = mv4Field2960.y = mv4Field2960.z = mv4Field2960.w = 1.0f;

    // Six bytes at +3456 cleared (the console clears one per wheel inside the matrix loop).
    for (u32 luByte = 0; luByte < 6; ++luByte)
    {
        mau8Field3456[luByte] = 0;
    }

    // All 96 body parts visible. The console seeds both words with 0xB80FFFFFFFF
    // (the top bits beyond the 96 valid parts carry a fixed pattern; preserved verbatim).
    mau64PartVisibility[0] = 0xB80FFFFFFFFull;
    mau64PartVisibility[1] = 0xB80FFFFFFFFull;

    // Detached-part render queue: empty, pointing at the embedded storage.
    mpDetachedPartRenderQueue      = maDetachedPartRenderQueueStorage;
    muDetachedPartRenderQueueMax   = 20;
    muDetachedPartRenderQueueCount = 0;

    // Misc scalar/flag fields the console zero-/seeds in Reset.
    mu5120Version = 4;
    mb5125        = 0;
    mb5131        = 0;
    mb5142        = 0;

    // Blues-and-twos strobe: timer A zeroed, strobe armed, reported state off.
    mfBluesAndTwosTimerA     = 0.0f;
    mbBluesAndTwosState      = 1;
    mbBluesAndTwosReturnState = 0;

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
        Vector4 lv4Value = mav4ScratchVertices[luVector];    // lvx128: whole register
        lv4Value.w = lfScratchAmount;                        // stfs f1 -> byte 12 (lane W)
        mav4ScratchVertices[luVector] = lv4Value;            // stvx128: all 16 bytes back
    }
}

} // namespace BrnWorld
