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
//   +0x00  union { f32 mafFloatBuffer[8]; u32 mauIntegerBuffer[8]; }  (32 bytes)
//   +0x20  u64 muSeed
//   +0x28  u32 muOldestBufferIndex
// sizeof == 0x30 (8-byte aligned).
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

// The 64-bit LCG multiplier the X360 refill block multiplies the seed by each draw
// (asm: 0x5851F42D low / 0x4C957F2D high). PCG-family constant.
const u64 KU_RANDOM_MULTIPLIER = 0x4C957F2D5851F42Dull;

// IEEE-754 1.0f bit pattern; each draw ORs the high seed bits (>>9) into the
// mantissa to make a float in [1, 2).
const u32 KU_IEEE_754_REPRESENTATION_FLOAT_ONE = 0x3F800000;

// DWARF CgsRandom.h:163. The 8-slot float ring buffer size.
const u32 KU_FLOAT_BUFFER_SIZE = 8;

class alignas(8) Random
{
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

    // ---- declared-only DWARF surface (bodies land with the full CgsRandom TU) ----
    void SetSeed(u64 lu64Seed);
    u32  RandomUInt();
    u32  RandomUInt(u32 luMin, u32 luMax);
    s32  RandomInt(s32 liMin, s32 liMax);
    bool RandomBool();
    f32  RandomFloat();
    f32  RandomFloat(f32 lfMin, f32 lfMax);
    f32  RandomSignedFloat();

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
}
