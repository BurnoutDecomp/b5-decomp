#pragma once

// =============================================================================
// CgsRandom.h  (OWNING HEADER for CgsNumeric::Random)
//
// Home for CgsNumeric::Random -- the engine's buffered pseudo-random generator.
// Reconstructed from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Numeric/CgsRandom.h,
// struct @ line 49) and the X360 inlined Construct/refill block (attested by the
// online-mode Start blocks and previously reconstructed inline in
// BrnGameStateFlybyManager.cpp).
//
// LAYOUT (DWARF-authoritative, asm-confirmed by the +0x260 mRandom embed in
// FlybyManager @ 0x823774C8):
//   +0x00  union { f32 mafFloatBuffer[8]; u32 mauIntegerBuffer[8];
//                  VectorIntrinsic mvVector[2]; }                     (32 bytes)
//   +0x20  u64 muSeed
//   +0x28  u32 muOldestBufferIndex
// sizeof == 0x30, **16-byte aligned**.
//
// ⚠️ CORRECTED 2026-08-03: this class was declared `alignas(8)`. The DWARF union
// (CgsRandom.h:179-183) has a THIRD member the committed banner omitted --
// `VectorIntrinsicUnion::VectorIntrinsic mvVector[2]` -- which makes the whole object
// **16-byte aligned**. sizeof is 0x30 either way (44 rounded to 48 at align 8, or to 48 at
// align 16), so the existing sizeof gate could never have caught it; what catches it is an
// EMBED at a 16-but-not-8 seat. VehicleManager has one: `mRandom` sits at +16 with
// meReleaseStage ending at +8, i.e. eight bytes of pure alignment padding that only exist
// because the type is 16-aligned (X360 VehicleManager::Construct @0x8263BCEC takes
// `addi r11, r31, 0x10` as &mRandom). At alignas(8) the compiler is free to seat it at +8 and
// every member behind it moves. Every other embed in the tree is already at a 16-aligned seat
// (CgsSoundUtils::SelectionHistory +0x10, CameraShake +0x7A0, FlybyManager +0x260), so this
// widening moves nothing that exists today -- it only makes the next embed correct by
// construction.
// The vector member itself is NOT declared below: VectorIntrinsic is an SDK register type this
// minimal home deliberately does not pull in, and no reconstructed body reads the ring through
// it. alignas(16) carries the only observable consequence it has.
//
// Only the buffer-priming spine the embedders inline (Construct + its two private
// helpers) is bodied here. The remaining DWARF methods
// (SetSeed / RandomUInt / RandomInt / RandomBool / RandomFloat / the RandomVectorN
// family) are other-TU surface and are declared-only so the shape stays coherent
// without forking the VecFloat / Vector2/3/4 return types into this minimal home.
// =============================================================================

#include "types.hpp"

namespace CgsNumeric
{
// DWARF CgsRandom.h:34. The default LCG seed Construct installs.
const u64 KU_RANDOM_DEFAULT_SEED = 0xC87CD8C91AD0891Bull;

// The 64-bit LCG multiplier the X360 refill block multiplies the seed by each draw.
//
// ⚠️⚠️ CORRECTED 2026-08-02 -- THE TWO HALVES WERE THE WRONG WAY ROUND, and the old comment
// ("0x5851F42D low / 0x4C957F2D high", value 0x4C957F2D5851F42D) described the mistake rather
// than the asm. Every X360 site builds it the same way, and the insert is what settles it:
//     lis  r11, 0x4C95 ; ori r11, r11, 0x7F2D     -> r11 = 0x4C957F2D
//     lis  r9,  0x5851 ; ori r9,  r9,  0xF42D     -> r9  = 0x5851F42D
//     insrdi r11, r9, 32, 0
// `insrdi RA,RS,n,b` inserts the n RIGHTMOST bits of RS at big-endian bit position b, so
// b == 0 puts r9 in the HIGH half:  r11 = 0x5851F42D_4C957F2D.
// Two independent witnesses, both then `mulld` by that register:
//   BehaviourGameplayExternal::UpdateJumping        @0x8220EB74..0x8220EB9C
//   BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ @0x82277ED0..0x82277F08
// And the corrected value is the canonical Knuth MMIX / PCG64 multiplier
// 6364136223846793005; the old one is not a known constant at all.
// ⇒ every draw in the tree (the ring refill below, the three effects randomisers,
// PropCollisions, BrnEffectsUtils) was stepping a DIFFERENT LCG from the console's.
const u64 KU_RANDOM_MULTIPLIER = 0x5851F42D4C957F2Dull;

// IEEE-754 1.0f bit pattern; each draw ORs the high seed bits (>>9) into the
// mantissa to make a float in [1, 2).
const u32 KU_IEEE_754_REPRESENTATION_FLOAT_ONE = 0x3F800000;

// DWARF CgsRandom.h:163. The 8-slot float ring buffer size.
const u32 KU_FLOAT_BUFFER_SIZE = 8;

class Random;
}

// Forward-declared for the bounded RandomVector draws below; the complete types live in
// rw/math/vpu/types.h and are NOT pulled in here (see the note on those declarations).
namespace rw { namespace math { namespace vpu {
struct Vector2;
struct Vector3;
struct Vector4;
} } }

// The effects-system per-call vector randomisers draw straight from a Random's
// internal LCG ring (asm @ 0x82277EC8 / 0x82277FB8 read muSeed @+0x20, the index
// @+0x28, and the f32/u32 ring union at +0). They are granted friend access below
// so they can reconstruct that spine by NAME rather than by raw offset.
namespace BrnEffects { namespace Utils {
struct Vector3Randomiser;
struct Vector4Randomiser;
struct DebrisColourRandomiser;
} }

namespace CgsNumeric
{
class alignas(16) Random
{
    // Additive grant only -- does not change Random's own layout or method
    // semantics. The randomisers use their own Vector-slot indexing scheme
    // ((index + 3) & 4) over the same 8-slot ring; that scheme lives in the
    // randomiser bodies, not here.
    friend struct BrnEffects::Utils::Vector3Randomiser;
    friend struct BrnEffects::Utils::Vector4Randomiser;
    friend struct BrnEffects::Utils::DebrisColourRandomiser;

public:
    // X360 (inlined at every Construct site, e.g. FlybyManager::Construct and the
    // online-mode Start blocks). Installs the default seed, primes the 8-slot float
    // ring buffer, and advances the oldest-slot index by one. The first slot is
    // seeded with exactly 1.0f, the remaining seven via AddRandomFloatToBuffer.
    void Construct()
    {
        muSeed              = KU_RANDOM_DEFAULT_SEED;
        muOldestBufferIndex = 0;
        mauIntegerBuffer[0] = KU_IEEE_754_REPRESENTATION_FLOAT_ONE;

        for (u32 luIndex = 1; luIndex < KU_FLOAT_BUFFER_SIZE; ++luIndex)
            AddRandomFloatToBuffer();

        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);
    }

    // ADDITIVE 2026-08-02 -- BODIED (it was in the declared-only list below).
    // The console INLINES it, which is why it is inline here rather than in the TU:
    // BehaviourGameplayExternal::UpdateJumping @0x8220EB94..0x8220EBBC is
    //     lwz r11, 0x5D4(r4)        ; the shared info's mpRandom
    //     ld  r10, 0x20(r11)        ; muSeed  (the OLD value)
    //     mulld r9, r10, r9         ; * KU_RANDOM_MULTIPLIER
    //     addi  r9, r9, 1
    //     srdi  r8, r10, 32         ; the OLD seed's high word ...
    //     std   r9, 0x20(r11)       ; ... store the new seed
    //     clrlwi r11, r8, 31        ; ... and test its bit 0  == old muSeed bit 32
    // ⚠️ IT DOES NOT TOUCH THE RING BUFFER. Unlike AddRandomFloatToBuffer below, a bool draw
    // only steps the LCG; muOldestBufferIndex and mauIntegerBuffer are untouched (the asm
    // makes no store other than the seed). And the returned bit comes from the seed BEFORE
    // the step, not after. The DecFIGS PS3 build attributes the same block to
    // CgsRandom.h:273/:284, i.e. it is a header inline there too.
    bool RandomBool()
    {
        const u64 luOldSeed = muSeed;
        muSeed = luOldSeed * KU_RANDOM_MULTIPLIER + 1;
        return ((luOldSeed >> 32) & 1u) != 0u;
    }

    // ---- declared-only DWARF surface (bodies land with the full CgsRandom TU) ----
    void SetSeed(u64 lu64Seed);
    u32  RandomUInt();
    u32  RandomUInt(u32 luMin, u32 luMax);
    s32  RandomInt(s32 liMin, s32 liMax);
    f32  RandomFloat();
    f32  RandomFloat(f32 lfMin, f32 lfMax);
    f32  RandomSignedFloat();

    // ADDITIVE 2026-08-01 (DWARF CgsRandom.h:143/:148/:153) -- the bounded VECTOR draws of the
    // RandomVectorN family this header's banner lists but never declared. Declared with a
    // FORWARD-declared rw::math::vpu type so the header keeps standing alone (a declaration
    // may use an incomplete parameter/return type; only definitions and calls need it
    // complete), which is what the banner's "without forking the Vector2/3/4 return types into
    // this minimal home" asked for.
    // FIRST CONSUMER: BrnDirector::Camera::Utils::CameraShake::Update @0x82221310 inlines the
    // Vector3 overload for its wobble impulse. Its draw is attested there store-for-store and
    // is byte-identical to the one BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ
    // @0x82277EC8 already reconstructs (two LCG steps, the two high words packed across three
    // ring slots at the Vector slot ((muOldestBufferIndex + 3) & 4), returning the quad primed
    // by the PREVIOUS call). Bodies land with the full CgsRandom TU.
    rw::math::vpu::Vector2 RandomVector(rw::math::vpu::Vector2 lMin, rw::math::vpu::Vector2 lMax);
    rw::math::vpu::Vector3 RandomVector(rw::math::vpu::Vector3 lMin, rw::math::vpu::Vector3 lMax);
    rw::math::vpu::Vector4 RandomVector(rw::math::vpu::Vector4 lMin, rw::math::vpu::Vector4 lMax);

private:
    union
    {
        f32 mafFloatBuffer[KU_FLOAT_BUFFER_SIZE];
        u32 mauIntegerBuffer[KU_FLOAT_BUFFER_SIZE];
    };
    u64 muSeed;
    u32 muOldestBufferIndex;

    // DWARF CgsRandom.h:158. Maps a fixed-point u32 into the mantissa of a [1, 2) float.
    static u32 ConvertUnsignedFixed32ToFloatRepresentation(u32 lu32Random)
    {
        return KU_IEEE_754_REPRESENTATION_FLOAT_ONE | (lu32Random >> 9);
    }

    // DWARF CgsRandom.h:161. Advances the LCG one step and writes the new float into
    // the ring buffer's next slot.
    void AddRandomFloatToBuffer()
    {
        const u32 luRandomFloatBits = ConvertUnsignedFixed32ToFloatRepresentation(static_cast<u32>(muSeed >> 32));
        muSeed              = muSeed * KU_RANDOM_MULTIPLIER + 1;
        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);
        mauIntegerBuffer[muOldestBufferIndex] = luRandomFloatBits;
    }
};

static_assert(sizeof(Random) == 0x30, "CgsNumeric::Random layout drift");
static_assert(alignof(Random) == 16,
              "CgsNumeric::Random is 16-aligned (the DWARF union's VectorIntrinsic[2] member); "
              "at align 8 an embedder can seat it at +8 and shift everything behind it");
}
