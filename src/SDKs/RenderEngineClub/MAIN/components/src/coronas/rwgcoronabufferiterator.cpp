#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaBuffer::Iterator::Write @ 0x823F3350
//
// The CoronaBuffer holds a 16-byte header followed by 64-byte corona records (the
// GetResourceDescriptor stride is (count << 6) + 16). The Iterator is a {data, index, count}
// cursor; Write lays one 64-byte record down at mpData[muIndex] and advances the index.
//
// Per-record layout (renderengine::Corona, GameSource/Graphics/BrnCoronaManager.h):
//   +0x00 mvPosition   three 128-bit VMX stores from v1/v2/v3 (position / direction / size)
//   +0x10 mvDirection
//   +0x20 mvSize
//   +0x30 mfDistance   stfs f1
//   +0x34 muColour     stw  r9   (the byte-permuted colour -- see below)
//   +0x38 miTextureID  stw  r6
//
// ---- SIGNATURE (2026-08-17, coronas step 1) -----------------------------------------------------
// RETYPED to the DWARF signature (rwgcoronabuffer.h:28,
// `void Write(Vector3, Vector3, Vector2, float, RGBA, int)`), which is also what the ASM shows:
// three VECTOR registers (v1/v2/v3), one FPR (f1) and two GPRs. The previous spelling --
// `Write(const void* lpVectorPayload, f64 lfFloat, u32 luColour, int liValue)` -- is Hex-Rays'
// rendering of exactly that call, because Hex-Rays cannot see by-value vector parameters on PPC and
// renders the three v-regs as "a 48-byte blob the caller staged". Taking the real types deletes the
// caller-side staging struct AND fixes the colour bug below, which the blob form made invisible.
//
// ---- ⚠ THE COLOUR BYTE ORDER IS AN ENDIANNESS CONVERSION, NOT A PERMUTATION ---------------------
// The console's `srwi/clrlwi/insrwi/slwi/or` chain at 0x823F33xx is a plain 32-bit BYTE SWAP -- and
// on the X360 that swap is what puts the four channels in MEMORY in the order the GPU's vertex
// fetch reads them. Walk it:
//     rw::RGBA::m_rgba == (a<<24) | (b<<16) | (g<<8) | r        (RwRGBA.cpp:5-9)
//     X360 is BIG-endian  -> that word occupies memory as bytes  [A][B][G][R]
//     the swap makes it                                          [R][G][B][A]
//     ...which is what the corona vertex element (Xenos format word 0x14C86, a UBYTE4N) delivers to
//     the shader as (r, g, b, a).
// THIS HOST IS LITTLE-ENDIAN, so m_rgba ALREADY occupies memory as [R][G][B][A]. Re-applying the
// console's swap would store [A][B][G][R] and the shader would read (a, b, g, r) -- i.e. with the
// usual opaque-white lamp colour the ALPHA lane would come out 255 (no distance fade at all, the
// exact defect kfCoronaFadeDistance exists to produce) and the red channel would carry the fade.
// AGENTS.md rule 1: a console byte order is not a host byte order.
//
// The record is therefore written BYTE BY BYTE below, so the stored order is R,G,B,A on any host and
// the question cannot come back. (Storing `lColour.m_rgba` directly would be equivalent on x64 and
// wrong on a big-endian host; the explicit form says what the console says: "lay the channels down
// in R,G,B,A order".)

namespace renderengine
{
    void CoronaBuffer::Iterator::Write(Vector3 lvPosition, Vector3 lvDirection, Vector2 lvSize,
                                        f32 lfDistance, rw::RGBA lColour, int liTextureID)
    {
        Corona& lrRecord = mpData[muIndex];        // mpData + (muIndex << 6)

        lrRecord.mvPosition  = lvPosition;         // stvx128 v1, +0x00
        lrRecord.mvDirection = lvDirection;        // stvx128 v2, +0x10
        lrRecord.mvSize      = lvSize;             // stvx128 v3, +0x20
        lrRecord.mfDistance  = lfDistance;         // stfs    f1, +0x30

        // stw r9, +0x34 -- the channels in memory order R,G,B,A (see the banner).
        u8* const lpu8Colour = reinterpret_cast<u8*>(&lrRecord.muColour);
        lpu8Colour[0] = static_cast<u8>( lColour.m_rgba        & 0xFFu);   // R
        lpu8Colour[1] = static_cast<u8>((lColour.m_rgba >>  8) & 0xFFu);   // G
        lpu8Colour[2] = static_cast<u8>((lColour.m_rgba >> 16) & 0xFFu);   // B
        lpu8Colour[3] = static_cast<u8>((lColour.m_rgba >> 24) & 0xFFu);   // A

        lrRecord.miTextureID = liTextureID;        // stw r6, +0x38

        ++muIndex;                                 // addi r11,r11,1 ; stw r11,4(r3)
    }
}
