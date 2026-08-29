#ifndef BRN_ROAD_SIGN_H
#define BRN_ROAD_SIGN_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // CgsGraphics::Vector2 (the 2-float pair the DWARF spells Basic2dColouredVertex::Vector2)

// BrnGui::RoadSign -- a single road-sign nav record drawn by the crash-nav icon renderer
// (BrnGui::CrashNavIconRenderer::RenderRoadSign / RenderRoadSigns). Reconstructed from the
// X360 ARTIST build:
//   RoadSign::RoadSign(const RoadSign&)  @ 0x8244BAD8  (copy ctor)
//   RoadSign::operator=(const RoadSign&) @ 0x8244BBB8  (copy assignment)
//
// ⭐ NAMES CORRECTED 2026-08-29 (mainmenu wave). The previous shape was recovered from the
// two copy functions alone and therefore named its fields mafField00 / muField50 / ... The
// DWARF (references/DecFIGS/dwarfdump/.../BrnCrashNavIconRenderer.h:48-69) gives the real
// names, and BrnGui::RoadSignList::Construct @0x8244BC90 -- the 65-entry literal table --
// confirms every one of them position-for-position on the FIRST record:
//
//   +0x00 .. +0x1F  mavTextBoxBounds[2]           8 floats  (-2,-2,135.3,38) (-2,-2,122.1,32)
//   +0x20 .. +0x2F  mavIndividualTextOffsets[2]   (-133.9,-18)  (8.5,-17)
//   +0x30           mvCoords                      (514.0, 469.3)
//   +0x38           mvPoleOffset                  (15.3, -50.2)
//   +0x40           mvSignOffset                  (51.6, -72.0)
//   +0x48           mvTextOffset                  (71.3, -69.9)
//   +0x50           meSignSize                    1  == E_SIGNSIZE_MID_1
//   +0x54           mafFontSize[2]                30.0 / 20.0
//   +0x5C           mapcText[2]                   "READ" / "LN"
//   +0x64           mpcRoadId                     "393756"
// -> 104 (0x68) bytes, which is exactly the stride the table walks (record 1 starts at
// +104) and exactly (mTextObject 0x2FB4 - mRoadSignList 0x154C) / 65 on the owning renderer.
//
// ⭐ 2026-08-29 (wave G3): RoadSignList::Construct @0x8244BC90 -- the 65-record authored
// table that fills these fields -- is now reconstructed in BrnRoadSign.cpp. Its recovery
// re-confirmed every field name and offset above independently a second time (the first
// record's 26 stores land on exactly the 26 members listed here, in this order).
//
// HOST WIDTH: the three trailing char pointers widen 4 -> 8 on the x64 gate, so sizeof()
// here is NOT the console's 104. The console offsets above are documentation; access is by
// name only.
namespace BrnGui
{
    struct RoadSign
    {
        // DWARF BrnCrashNavIconRenderer.h:51 -- which of the four authored sign plates the
        // record draws (drives the atlas row RenderRoadSign samples).
        enum ESignSize
        {
            E_SIGNSIZE_SMALL = 0,
            E_SIGNSIZE_MID_1 = 1,
            E_SIGNSIZE_MID_2 = 2,
            E_SIGNSIZE_LARGE = 3,
            E_SIGNSIZE_COUNT = 4,
        };

        // DWARF :60 -- Vector4[2]. Held as two 4-float rows rather than the 16-byte VMX
        // Vector4 so the record stays copy-assignable POD storage and the console's 8-float
        // head is reproduced exactly (the copy functions move all 20 leading floats with
        // plain lfs/stfs, never lvx128 -- this block is NOT vector-aligned on the console).
        f32                  mavTextBoxBounds[2][4];      // +0x00 (DWARF :60)
        CgsGraphics::Vector2 mavIndividualTextOffsets[2]; // +0x20 (DWARF :61)
        CgsGraphics::Vector2 mvCoords;                    // +0x30 (DWARF :62) map position
        CgsGraphics::Vector2 mvPoleOffset;                // +0x38 (DWARF :63)
        CgsGraphics::Vector2 mvSignOffset;                // +0x40 (DWARF :64)
        CgsGraphics::Vector2 mvTextOffset;                // +0x48 (DWARF :65)
        ESignSize            meSignSize;                  // +0x50 (DWARF :66)
        f32                  mafFontSize[2];              // +0x54 (DWARF :67)
        const char*          mapcText[2];                 // +0x5C (DWARF :68) the two text lines
        const char*          mpcRoadId;                   // +0x64 (DWARF :69) the road's string id

        RoadSign() {}
        RoadSign(const RoadSign& lrOther);              // @ 0x8244BAD8
        RoadSign& operator=(const RoadSign& lrOther);   // @ 0x8244BBB8
    };
}

#endif
