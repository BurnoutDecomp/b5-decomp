// b5-decomp/src/SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronaatlasuvs.inl
// =============================================================================
// rwgcoronaatlasuvs.inl -- THE CORONA ATLAS UV TABLE, s_atlasUVs[12][4].
//
// Reconstructed 2026-08-17 (coronas step 1, group `coronadata`) from
// BURNOUT_X360_ARTIST.XEX.  This file is an INITIALISER LIST ONLY (no declaration, no
// semicolon, no include guard) -- it is used exactly once, like this:
//
//     // GameSource/Graphics/BrnCoronaTypeParams.cpp   (SEAM S2)
//     extern const Vector2 KA_CORONA_ATLAS_UVS[12][4] =
//     {
//     #include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronaatlasuvs.inl"
//     };
//
// BrnCoronaManager::Construct copies KA_CORONA_ATLAS_UVS into the DWARF's NON-const
// `BrnCoronaManager::s_atlasUVs[12][4]` (BrnCoronaManager.h:229 / DWARF :211) -- the console's
// static-initialiser effect, reproduced at the earliest point every reader is guaranteed to follow.
//
// ---------------------------------------------------------------------------------------------
// GROUND TRUTH
//
//   * ADDRESS / SHAPE.  X360 `unk_82FAFC10`, 768 bytes.  `BrnCoronaManager::Construct`
//     @0x823FCD90 and `BrnCoronaManager::SetTextureAtlas` @0x823FD11C-2C both store
//     `&unk_82FAFC10` into `dword_82FAB6B8`.  Its consumer,
//     `renderengine::CoronaRenderer::Dispatch<shadow::Device>` @0x82404F30, reads per corona
//         v24 = record[+0x38] << 6                       ; textureID * 64  -> the ROW
//         uv0 = *(v24 + dword_82FAB6B8 +  0), *(... +  4)
//         uv1 = *(v24 + dword_82FAB6B8 + 16), *(... + 20)
//         uv2 = *(v24 + dword_82FAB6B8 + 32), *(... + 36)
//         uv3 = *(v24 + dword_82FAB6B8 + 48), *(... + 52)
//     -- a 64-byte row per texture id, four 16-byte-strided (u,v) pairs per row.  12 rows x
//     4 corners x a 16-byte VMX Vector2 = 768 bytes, matching the declaration exactly.
//     Only .x/.y of each lane are ever read; .z/.w are emitted as 0.0f here.
//
//   * VALUES.  The image words at 0x82FAFC10 are ALL ZERO -- the table is built at run time by
//     the C++ dynamic initialiser `sub_82C4EDD8` (0x82C4EDD8-0x82C4F414), which stages 16-byte
//     lanes on the stack from five rodata floats and stores them with 48 `stvx128 v0, r11, <0x00,
//     0x10, ... , 0x2F0>` -- exactly the 48 lanes of the 12x4 table, no gaps, no duplicates.
//     The five source floats (values from the conductor's dump):
//         flt_82001CC0 = 0.0        (u32 00000000)
//         flt_8203DAA4 = 1/3        (u32 3EAAAAAB)
//         flt_8200AECC = 2/3        (u32 3F2AAAAB)
//         flt_8203DCEC = 5/6        (u32 3F555555)
//         flt_82001C98 = 1.0        (u32 3F800000)   <- also the conductor's CONTROL value
//     The decimal literals below are the shortest decimals that round-trip to those exact f32
//     bit patterns (verified, work/verify_literals.txt).
//
//   * WHAT THE TABLE IS.  Decoded blind, the 12 rows tile the atlas as a 3x3 grid of 1/3-sized
//     cells whose LAST cell (u,v in [2/3,1]) is subdivided into a 2x2 of 1/6-sized cells:
//
//         +---------+---------+---------+        pages 0..7 : the eight 1/3 cells, in
//         |    0    |    1    |    2    |                     reading order
//         +---------+---------+---------+        pages 8..11: the (2/3..1, 2/3..1) cell
//         |    3    |    4    |    5    |                     quartered, in reading order
//         +---------+---------+----+----+
//         |    6    |    7    | 8  | 9  |
//         |         |         +----+----+
//         |         |         | 10 | 11 |
//         +---------+---------+----+----+
//
//     Every corner is exactly one of {0, 1/3, 2/3, 5/6, 1} and every row is a closed rectangle
//     in corner order (minU,minV) (maxU,minV) (maxU,maxV) (minU,maxV) -- i.e. TL, TR, BR, BL.
//     That self-consistency, plus the fact that every miTextureID in the archetype table is in
//     [0,11], is the sanity check the brief asked for.  NOTE: the atlas's PIXEL dimensions were
//     NOT read (see the report, BLOCKED item A2) -- they are not needed, because these UVs are
//     normalised.  A 1024x1024 atlas would give 341x341-pixel large pages and 170x170 small ones.
//
//   * PER-LANE CITATIONS: work/DECODE_LOG.txt lists the `stvx128` instruction address for each of
//     the 48 lanes; the row comments below carry the address of the row's first lane store.
// =============================================================================

    // page  0 -- grid cell (0,0), 1/3 x 1/3            first store @0x82C4EE98
    { Vector2{ 0.0f,        0.0f,        0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.0f,        0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.0f,        0.33333334f, 0.0f, 0.0f } },

    // page  1 -- grid cell (1,0), 1/3 x 1/3            first store @0x82C4EF6C
    { Vector2{ 0.33333334f, 0.0f,        0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.0f,        0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.33333334f, 0.0f, 0.0f } },

    // page  2 -- grid cell (2,0), 1/3 x 1/3            first store @0x82C4EFD8
    { Vector2{ 0.66666669f, 0.0f,        0.0f, 0.0f },
      Vector2{ 1.0f,        0.0f,        0.0f, 0.0f },
      Vector2{ 1.0f,        0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.33333334f, 0.0f, 0.0f } },

    // page  3 -- grid cell (0,1), 1/3 x 1/3            first store @0x82C4F05C
    { Vector2{ 0.0f,        0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.0f,        0.66666669f, 0.0f, 0.0f } },

    // page  4 -- grid cell (1,1), 1/3 x 1/3            first store @0x82C4F0E4
    { Vector2{ 0.33333334f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.66666669f, 0.0f, 0.0f } },

    // page  5 -- grid cell (2,1), 1/3 x 1/3            first store @0x82C4F128
    { Vector2{ 0.66666669f, 0.33333334f, 0.0f, 0.0f },
      Vector2{ 1.0f,        0.33333334f, 0.0f, 0.0f },
      Vector2{ 1.0f,        0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.66666669f, 0.0f, 0.0f } },

    // page  6 -- grid cell (0,2), 1/3 x 1/3            first store @0x82C4F1CC
    { Vector2{ 0.0f,        0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.33333334f, 1.0f,        0.0f, 0.0f },
      Vector2{ 0.0f,        1.0f,        0.0f, 0.0f } },

    // page  7 -- grid cell (1,2), 1/3 x 1/3            first store @0x82C4F210
    { Vector2{ 0.33333334f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 1.0f,        0.0f, 0.0f },
      Vector2{ 0.33333334f, 1.0f,        0.0f, 0.0f } },

    // page  8 -- quarter of cell (2,2), 1/6 x 1/6      first store @0x82C4F250
    { Vector2{ 0.66666669f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.83333331f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 0.83333331f, 0.83333331f, 0.0f, 0.0f },
      Vector2{ 0.66666669f, 0.83333331f, 0.0f, 0.0f } },

    // page  9 -- quarter of cell (2,2), 1/6 x 1/6      first store @0x82C4F324
    { Vector2{ 0.83333331f, 0.66666669f, 0.0f, 0.0f },
      Vector2{ 1.0f,        0.66666669f, 0.0f, 0.0f },
      Vector2{ 1.0f,        0.83333331f, 0.0f, 0.0f },
      Vector2{ 0.83333331f, 0.83333331f, 0.0f, 0.0f } },

    // page 10 -- quarter of cell (2,2), 1/6 x 1/6      first store @0x82C4F390
    { Vector2{ 0.66666669f, 0.83333331f, 0.0f, 0.0f },
      Vector2{ 0.83333331f, 0.83333331f, 0.0f, 0.0f },
      Vector2{ 0.83333331f, 1.0f,        0.0f, 0.0f },
      Vector2{ 0.66666669f, 1.0f,        0.0f, 0.0f } },

    // page 11 -- quarter of cell (2,2), 1/6 x 1/6      first store @0x82C4F3D4
    { Vector2{ 0.83333331f, 0.83333331f, 0.0f, 0.0f },
      Vector2{ 1.0f,        0.83333331f, 0.0f, 0.0f },
      Vector2{ 1.0f,        1.0f,        0.0f, 0.0f },
      Vector2{ 0.83333331f, 1.0f,        0.0f, 0.0f } },
