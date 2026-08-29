// ===================================================================================
// BrnGui::RoadSign  -- implementation
//   class:BrnGui::RoadSign
//
// Two 104-byte (0x68) field-by-field copies, recovered store-for-store from the X360
// ARTIST build. Member-by-name; both reproduce a complete copy of the record.
//
// 2026-08-29 (mainmenu wave): the field names are now the DWARF's (see BrnRoadSign.h for
// the RoadSignList::Construct @0x8244BC90 cross-check that pins every one of them). The
// COPIED SET is unchanged -- the console moves the 20 leading floats with lfs/stfs and the
// six trailing words with lwz/stw, which is exactly the whole record.
// ===================================================================================
#include "GameSource/Gui/CustomRenderer/Renderers/BrnRoadSign.h"
#include "GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.h" // BrnGui::RoadSignList (the DWARF homes it on the renderer header)

namespace BrnGui
{
    // @ 0x8244BAD8 -- copy ctor. Copies +0x00..+0x4F as 20 floats (lfs/stfs), then
    // +0x50/+0x54/+0x58/+0x5C/+0x60/+0x64 as words (lwz/stw). (The trailing .long is
    // alignment padding, not a return.)
    RoadSign::RoadSign(const RoadSign& lrOther)
    {
        for (int li = 0; li < 2; ++li)
        {
            for (int lj = 0; lj < 4; ++lj)
                mavTextBoxBounds[li][lj] = lrOther.mavTextBoxBounds[li][lj];
            mavIndividualTextOffsets[li] = lrOther.mavIndividualTextOffsets[li];
        }
        mvCoords     = lrOther.mvCoords;
        mvPoleOffset = lrOther.mvPoleOffset;
        mvSignOffset = lrOther.mvSignOffset;
        mvTextOffset = lrOther.mvTextOffset;

        meSignSize     = lrOther.meSignSize;
        mafFontSize[0] = lrOther.mafFontSize[0];
        mafFontSize[1] = lrOther.mafFontSize[1];
        mapcText[0]    = lrOther.mapcText[0];
        mapcText[1]    = lrOther.mapcText[1];
        mpcRoadId      = lrOther.mpcRoadId;
    }

    // @ 0x8244BBB8 -- copy assignment. Same fields; +0x54/+0x58 move via float loads
    // (lfs/stfs), the rest as words. (Hex-Rays' v2 temp at +0x5C is the compiler
    // scheduling a word load ahead of the +0x54 float store -- no semantic effect.)
    RoadSign& RoadSign::operator=(const RoadSign& lrOther)
    {
        for (int li = 0; li < 2; ++li)
        {
            for (int lj = 0; lj < 4; ++lj)
                mavTextBoxBounds[li][lj] = lrOther.mavTextBoxBounds[li][lj];
            mavIndividualTextOffsets[li] = lrOther.mavIndividualTextOffsets[li];
        }
        mvCoords     = lrOther.mvCoords;
        mvPoleOffset = lrOther.mvPoleOffset;
        mvSignOffset = lrOther.mvSignOffset;
        mvTextOffset = lrOther.mvTextOffset;

        meSignSize     = lrOther.meSignSize;
        mafFontSize[0] = lrOther.mafFontSize[0];
        mafFontSize[1] = lrOther.mafFontSize[1];
        mapcText[0]    = lrOther.mapcText[0];
        mapcText[1]    = lrOther.mapcText[1];
        mpcRoadId      = lrOther.mpcRoadId;
        return *this;
    }
}

// ===================================================================================
// BrnGui::RoadSignList::Construct  @0x8244BC90
//
// The 65-record AUTHORED road-sign table. On the console this is one enormous inline
// literal initialiser: 1,690 stores (65 x 26 fields x 4 bytes == 6,760 == 65 x 104) laid
// straight onto the list at CrashNavIconRenderer+0x154C, with no loop and no data section
// to point at. Every value below was RECOVERED, not authored:
//   * the 1,690 store offsets and their register sources were replayed out of the
//     function's own asm (lis/addi @ha/@l symbol forms, lfs/lwz, the r1 spill slots), and
//     each float was then READ from the raw image (file offset = VA - 0x82000000,
//     big-endian) at the flt_* address the load names -- never typed from a printed decimal;
//   * the 195 string pointers were read as C strings at their target VAs, which is how the
//     records IDA prints only as `&unk_82056D90` resolve ("DR", "WAY", "AV", "ST", ...);
//   * eight fields whose base register the replay could not colour (five floats reached
//     through a reused @ha page register, three string pointers) were taken from the
//     export's own pseudocode, whose float bit patterns it prints as raw ints.
// The two paths were then cross-checked field-for-field: 1,690 of 1,690 agree.
//
// TWO INDEPENDENT STRUCTURAL CONFIRMATIONS that the recovery is aligned, not merely
// self-consistent:
//   * mvCoords.y is MONOTONICALLY NON-DECREASING across all 65 records (469.3 .. 1517.0).
//     A misaligned stride or a swapped field pair would break that immediately.
//   * every mpcRoadId but one appears in the icon table this renderer matches against
//     (KAPC_ROAD_IDS, BrnCrashNavIconRenderer_wK_01.cpp, itself read straight out of
//     off_82F27C98..off_82F27D98). See the "392373" note below.
//
// ⚠️ RECORD 26 ("392373" / "6th ST") HAS NO ICON SLOT, ON THE CONSOLE TOO. Its road id is
// the one of the 65 that is absent from the 64 live ids in KAPC_ROAD_IDS, so
// CrashNavIconRenderer::RenderRoadSign's id scan falls through to the "invisible" sentinel,
// finds no match and returns before drawing -- exactly the console's `if (v36 ==
// off_82F27D98) goto end`. This is authored data, not a recovery error; do not "fix" it by
// substituting a nearby id.
//
// SHAPE: RoadSign is not an aggregate (it has a user-declared default ctor and copy
// functions), so the table cannot brace-initialise RoadSign directly. It is held as the
// POD twin below and copied field-by-field by name -- which is also the only way to write
// it that survives the host's 4->8 pointer widening.
// ===================================================================================
namespace
{
    // The console record, field-for-field, as a plain aggregate. Offsets are the X360
    // record offsets (see BrnRoadSign.h); they are documentation only -- the copy below
    // is by name.
    struct RoadSignInit
    {
        f32         afTextBoxBounds[8];         // +0x00  two 4-float text-box rows
        f32         afIndividualTextOffsets[4]; // +0x20  two (x,y) per-line offsets
        f32         fCoordsX,     fCoordsY;     // +0x30  map position
        f32         fPoleOffsetX, fPoleOffsetY; // +0x38
        f32         fSignOffsetX, fSignOffsetY; // +0x40
        f32         fTextOffsetX, fTextOffsetY; // +0x48
        BrnGui::RoadSign::ESignSize eSignSize;  // +0x50
        f32         afFontSize[2];              // +0x54
        const char* apcText[2];                 // +0x5C
        const char* pcRoadId;                   // +0x64
    };

    using BrnGui::RoadSign;

    const RoadSignInit KA_ROAD_SIGNS[BrnGui::RoadSignList::KI_NUM_ROAD_SIGNS] =
    {
    // [ 0] road id 393756 -- "READ LN"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.9f, -18.0f, 8.5f, -17.0f },
      514.0f, 469.3f,  15.3f, -50.2f,  51.6f, -72.0f,  71.3f, -69.9f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "READ", "LN" }, "393756" },
    // [ 1] road id 393762 -- "N. ROUSE DR"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -147.2f, -25.5f, 19.6f, -24.5f },
      1026.7f, 480.0f,  15.3f, -50.2f,  81.6f, -72.0f,  107.8f, -62.4f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "N. ROUSE", "DR" }, "393762" },
    // [ 2] road id 393760 -- "LEWIS PASS"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -125.6f, -25.5f, 14.0f, -24.5f },
      875.4f, 505.3f,  15.3f, -50.2f,  61.6f, -72.0f,  70.5f, -62.2f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "LEWIS", "PASS" }, "393760" },
    // [ 3] road id 393860 -- "NELSON WAY"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -135.0f, -18.0f, 6.4f, -17.0f },
      642.7f, 534.6f,  15.3f, -50.2f,  61.6f, -72.0f,  88.6f, -69.9f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "NELSON", "WAY" }, "393860" },
    // [ 4] road id 394306 -- "N. MOUNTAIN DR"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -157.5f, -18.0f, 24.9f, -17.0f },
      261.5f, 584.6f,  15.3f, -50.2f,  81.6f, -72.0f,  126.6f, -69.9f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "N. MOUNTAIN", "DR" }, "394306" },
    // [ 5] road id 396228 -- "W.CRAWFORD DR"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -154.0f, -26.6f, 13.0f, -24.4f },
      1173.4f, 600.6f,  15.3f, -50.2f,  81.6f, -72.0f,  137.6f, -61.3f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "W.CRAWFORD", "DR" }, "396228" },
    // [ 6] road id 395600 -- "NEWTON DR"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -146.2f, -25.5f, 19.1f, -24.5f },
      1367.6f, 621.0f,  15.3f, -50.2f,  61.6f, -72.0f,  83.3f, -62.5f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "NEWTON", "DR" }, "395600" },
    // [ 7] road id 396456 -- "MOORE AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -131.4f, -18.0f, 8.8f, -17.0f },
      1637.2f, 673.1f,  15.3f, -50.2f,  51.6f, -72.0f,  75.5f, -69.9f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "MOORE", "AV" }, "396456" },
    // [ 8] road id 535195 -- "CANNON PASS"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -129.6f, -18.0f, 10.0f, -17.0f },
      509.9f, 682.0f,  15.3f, -50.2f,  61.6f, -72.0f,  82.5f, -69.8f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "CANNON", "PASS" }, "535195" },
    // [ 9] road id 396460 -- "HUDSON AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -134.1f, -18.0f, 6.5f, -17.0f },
      1757.4f, 696.2f,  15.3f, -50.2f,  61.6f, -72.0f,  95.7f, -69.8f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "HUDSON", "AV" }, "396460" },
    // [10] road id 397601 -- "E. CRAWFORD DR"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -158.5f, -18.0f, 24.9f, -17.0f },
      1366.9f, 698.3f,  15.3f, -50.2f,  81.6f, -72.0f,  129.4f, -69.9f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "E. CRAWFORD", "DR" }, "397601" },
    // [11] road id 396114 -- "NAKAMURA AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -145.7f, -25.5f, 21.1f, -24.5f },
      1513.9f, 717.4f,  15.3f, -50.2f,  81.6f, -72.0f,  117.8f, -62.5f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "NAKAMURA", "AV" }, "396114" },
    // [12] road id 396188 -- "HAWLEY AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -132.9f, -25.5f, 7.8f, -24.5f },
      1694.1f, 731.0f,  15.3f, -50.2f,  61.6f, -72.0f,  96.5f, -62.2f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "HAWLEY", "AV" }, "396188" },
    // [13] road id 393900 -- "UPHILL DR"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -134.3f, -25.5f, 2.7f, -24.5f },
      330.1f, 757.3f,  15.3f, -50.2f,  51.6f, -72.0f,  81.5f, -62.4f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "UPHILL", "DR" }, "393900" },
    // [14] road id 397702 -- "I-88 SECTION 2"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.0f, -18.0f, 7.9f, -17.0f },
      1824.6f, 759.3f,  15.3f, -50.2f,  81.6f, -72.0f,  75.9f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "I-88", "SECTION 2" }, "397702" },
    // [15] road id 396133 -- "SULLIVAN AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -146.7f, -25.5f, 20.1f, -24.5f },
      1573.3f, 760.0f,  15.3f, -50.2f,  81.6f, -72.0f,  110.8f, -62.2f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "SULLIVAN", "AV" }, "396133" },
    // [16] road id 395952 -- "9th ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.6f, -18.0f, 8.5f, -16.8f },
      1433.4f, 760.5f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.9f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "9th", "ST" }, "395952" },
    // [17] road id 394273 -- "ROSS DR"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -128.3f, -18.0f, 13.2f, -17.0f },
      970.0f, 799.3f,  15.3f, -50.2f,  51.6f, -72.0f,  67.5f, -70.1f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "ROSS", "DR" }, "394273" },
    // [18] road id 387078 -- "PATTERSON AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -155.9f, -25.5f, 11.8f, -24.5f },
      1791.1f, 806.0f,  15.3f, -50.2f,  81.6f, -72.0f,  133.0f, -62.4f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "PATTERSON", "AV" }, "387078" },
    // [19] road id 535196 -- "LAWRENCE RD"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -141.7f, -18.0f, 40.6f, -15.0f },
      1147.7f, 826.7f,  15.3f, -50.2f,  81.6f, -72.0f,  101.8f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "LAWRENCE", "RD" }, "535196" },
    // [20] road id 395917 -- "7th ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.6f, -18.0f, 8.5f, -16.9f },
      1494.7f, 835.1f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "7th", "ST" }, "395917" },
    // [21] road id 385737 -- "WATT ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -132.1f, -18.0f, 7.3f, -16.9f },
      1942.9f, 845.1f,  15.3f, -50.2f,  31.6f, -72.0f,  46.6f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "WATT", "ST" }, "385737" },
    // [22] road id 392589 -- "PARADISE AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -149.1f, -25.5f, 14.8f, -24.5f },
      1653.1f, 891.9f,  15.3f, -50.2f,  61.6f, -72.0f,  95.0f, -62.3f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "PARADISE", "AV" }, "392589" },
    // [23] road id 393198 -- "HAMILTON AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -147.4f, -25.5f, 19.6f, -24.5f },
      1469.0f, 898.2f,  15.3f, -50.2f,  81.6f, -72.0f,  118.8f, -62.4f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "HAMILTON", "AV" }, "393198" },
    // [24] road id 561416 -- "E. LAKE DR"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -127.9f, -25.5f, 11.8f, -24.5f },
      880.4f, 900.1f,  15.3f, -50.2f,  61.6f, -72.0f,  87.5f, -62.2f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "E. LAKE", "DR" }, "561416" },
    // [25] road id 396988 -- "WEBSTER AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -149.7f, -25.5f, 18.6f, -24.5f },
      1860.3f, 906.0f,  15.3f, -50.2f,  81.6f, -72.0f,  109.3f, -62.5f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "WEBSTER", "AV" }, "396988" },
    // [26] road id 392373 -- "6th ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -131.6f, -18.0f, 10.5f, -16.9f },
      1591.8f, 931.6f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "6th", "ST" }, "392373" },
    // [27] road id 387198 -- "RIVERSIDE AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -148.4f, -25.5f, 18.8f, -24.5f },
      1751.2f, 948.3f,  15.3f, -50.2f,  81.6f, -72.0f,  120.8f, -62.2f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "RIVERSIDE", "AV" }, "387198" },
    // [28] road id 394474 -- "RACK WAY"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.9f, -18.0f, 7.3f, -17.0f },
      1048.7f, 951.3f,  15.3f, -50.2f,  51.6f, -72.0f,  64.6f, -69.9f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "RACK", "WAY" }, "394474" },
    // [29] road id 383595 -- "ANGUS WHARF"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.1f, -18.0f, 8.3f, -16.9f },
      1933.9f, 968.7f,  15.3f, -50.2f,  59.9f, -72.3f,  67.9f, -70.1f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "ANGUS", "WHARF" }, "383595" },
    // [30] road id 393911 -- "CHUBB LN"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -131.6f, -25.5f, 8.0f, -24.5f },
      616.0f, 973.9f,  15.3f, -50.2f,  61.6f, -72.0f,  85.0f, -62.2f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "CHUBB", "LN" }, "393911" },
    // [31] road id 385860 -- "4th ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -130.4f, -18.0f, 10.5f, -16.9f },
      1837.8f, 985.4f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "4th", "ST" }, "385860" },
    // [32] road id 393062 -- "YOUNG AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -135.0f, -25.5f, 5.4f, -24.5f },
      1497.7f, 989.0f,  15.3f, -50.2f,  51.6f, -72.0f,  76.2f, -62.6f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "YOUNG", "AV" }, "393062" },
    // [33] road id 392323 -- "5th ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -130.6f, -18.0f, 11.5f, -16.9f },
      1623.9f, 989.6f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "5th", "ST" }, "392323" },
    // [34] road id 386215 -- "3rd ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -132.1f, -18.0f, 9.5f, -16.9f },
      1382.9f, 1005.9f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "3rd", "ST" }, "386215" },
    // [35] road id 397165 -- "I-88 SECTION 3"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.0f, -18.0f, 7.9f, -17.0f },
      1183.8f, 1016.5f,  15.3f, -50.2f,  81.6f, -72.0f,  75.9f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "I-88", "SECTION 3" }, "397165" },
    // [36] road id 394487 -- "SCHEMBRI PASS"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -147.9f, -25.5f, 19.8f, -24.5f },
      336.7f, 1018.5f,  15.3f, -50.2f,  81.6f, -72.0f,  110.8f, -62.2f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "SCHEMBRI", "PASS" }, "394487" },
    // [37] road id 505312 -- "ANDERSEN ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -132.1f, -18.0f, 9.8f, -17.0f },
      1911.0f, 1044.7f,  15.3f, -50.2f,  81.6f, -72.0f,  130.7f, -69.9f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "ANDERSEN", "ST" }, "505312" },
    // [38] road id 393166 -- "FRANKE AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -136.6f, -25.5f, 4.0f, -24.5f },
      1629.3f, 1049.0f,  15.3f, -50.2f,  61.6f, -72.0f,  96.5f, -62.5f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "FRANKE", "AV" }, "393166" },
    // [39] road id 506519 -- "ROOT AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -139.6f, -18.0f, 2.5f, -17.0f },
      1727.8f, 1063.4f,  15.3f, -50.2f,  51.6f, -72.0f,  72.2f, -70.0f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "ROOT", "AV" }, "506519" },
    // [40] road id 506394 -- "EVANS BLVD"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -135.4f, -18.0f, 9.1f, -17.0f },
      1535.8f, 1097.8f,  15.3f, -50.2f,  61.6f, -72.0f,  80.4f, -70.0f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "EVANS", "BLVD" }, "506394" },
    // [41] road id 392532 -- "FRY AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.6f, -18.0f, 6.5f, -17.0f },
      1625.0f, 1127.6f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "FRY", "AV" }, "392532" },
    // [42] road id 386734 -- "2nd ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -134.9f, -18.0f, 6.5f, -16.9f },
      1447.4f, 1129.6f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "2nd", "ST" }, "386734" },
    // [43] road id 387153 -- "GLANCEY AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -149.8f, -25.5f, 19.2f, -24.5f },
      1822.2f, 1149.1f,  15.3f, -50.2f,  81.6f, -72.0f,  113.2f, -62.5f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "GLANCEY", "AV" }, "387153" },
    // [44] road id 561415 -- "W. LAKE DR"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -133.9f, -25.5f, 6.3f, -24.5f },
      653.9f, 1173.7f,  15.3f, -50.2f,  61.6f, -72.0f,  91.0f, -62.5f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "W. LAKE", "DR" }, "561415" },
    // [45] road id 396742 -- "HUBBARD AV"
    { { -2.0f, -2.0f, 149.9f, 45.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -160.6f, -25.5f, -5.5f, -24.5f },
      1310.8f, 1202.5f,  15.3f, -50.2f,  81.6f, -72.0f,  141.0f, -62.5f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "HUBBARD", "AV" }, "396742" },
    // [46] road id 393120 -- "KING AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.5f, -18.0f, 4.9f, -17.0f },
      1567.5f, 1207.5f,  15.3f, -50.2f,  31.6f, -72.0f,  47.7f, -70.0f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "KING", "AV" }, "393120" },
    // [47] road id 395490 -- "S. ROUSE DR"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -147.4f, -25.5f, 19.3f, -24.5f },
      1135.2f, 1213.7f,  15.3f, -50.2f,  81.6f, -72.0f,  108.3f, -62.4f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "S. ROUSE", "DR" }, "395490" },
    // [48] road id 390723 -- "1st ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -135.9f, -18.0f, 6.5f, -17.0f },
      1749.9f, 1221.4f,  15.3f, -50.2f,  31.6f, -72.0f,  40.2f, -69.7f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "1st", "ST" }, "390723" },
    // [49] road id 397455 -- "I-88 SECTION 1"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.0f, -18.0f, 7.9f, -17.0f },
      1877.6f, 1227.7f,  15.3f, -50.2f,  81.6f, -72.0f,  75.9f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "I-88", "SECTION 1" }, "397455" },
    // [50] road id 395197 -- "LUCAS WAY"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.9f, -18.0f, 4.5f, -17.0f },
      435.3f, 1257.1f,  15.3f, -50.2f,  51.6f, -72.0f,  74.2f, -69.9f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "LUCAS", "WAY" }, "395197" },
    // [51] road id 397348 -- "HARBER ST"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -132.9f, -25.5f, 6.8f, -24.5f },
      1636.1f, 1261.8f,  15.3f, -50.2f,  61.6f, -72.0f,  96.0f, -62.4f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "HARBER", "ST" }, "397348" },
    // [52] road id 395656 -- "HALL AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.3f, -18.0f, 7.2f, -17.0f },
      1153.2f, 1307.7f,  15.3f, -50.2f,  31.6f, -72.0f,  46.4f, -69.8f,
      RoadSign::E_SIGNSIZE_SMALL, { 30.0f, 20.0f }, { "HALL", "AV" }, "395656" },
    // [53] road id 397409 -- "MANNERS AV"
    { { -2.0f, -2.0f, 149.9f, 45.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -163.4f, -25.5f, -6.3f, -24.5f },
      1524.3f, 1311.1f,  15.3f, -50.2f,  81.6f, -72.0f,  141.0f, -62.5f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "MANNERS", "AV" }, "397409" },
    // [54] road id 396706 -- "LAMBERT PKWY"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -154.2f, -18.0f, 29.1f, -17.0f },
      1229.2f, 1344.4f,  15.3f, -50.2f,  81.6f, -72.0f,  95.5f, -69.7f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "LAMBERT", "PKWY" }, "396706" },
    // [55] road id 394965 -- "HANS WAY"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -132.1f, -18.0f, 7.0f, -17.0f },
      495.8f, 1347.7f,  15.3f, -50.2f,  51.6f, -72.0f,  66.7f, -69.8f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "HANS", "WAY" }, "394965" },
    // [56] road id 561413 -- "GELDARD DR"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -177.0f, -18.0f, 6.4f, -17.0f },
      826.0f, 1348.4f,  15.3f, -50.2f,  61.6f, -72.0f,  100.8f, -69.8f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "GELDARD", "DR" }, "561413" },
    // [57] road id 392997 -- "WARREN AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -132.4f, -25.5f, 7.3f, -24.5f },
      1438.3f, 1367.8f,  15.3f, -50.2f,  61.6f, -72.0f,  95.0f, -62.4f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "WARREN", "AV" }, "392997" },
    // [58] road id 396718 -- "PARR AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.3f, -18.0f, 8.0f, -17.0f },
      1273.1f, 1403.1f,  15.3f, -50.2f,  51.6f, -72.0f,  72.7f, -69.8f,
      RoadSign::E_SIGNSIZE_MID_1, { 30.0f, 20.0f }, { "PARR", "AV" }, "396718" },
    // [59] road id 397201 -- "I-88 SECTION 4"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.0f, -18.0f, 7.9f, -17.0f },
      1030.5f, 1415.7f,  15.3f, -50.2f,  81.6f, -72.0f,  75.9f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "I-88", "SECTION 4" }, "397201" },
    // [60] road id 561121 -- "CASEY PASS"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -133.9f, -19.0f, 8.3f, -16.0f },
      782.5f, 1416.8f,  15.3f, -50.2f,  61.6f, -72.0f,  80.0f, -69.7f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "CASEY", "PASS" }, "561121" },
    // [61] road id 392688 -- "SHEPHERD AV"
    { { -2.0f, -2.0f, 161.0f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -143.2f, -25.5f, 23.6f, -24.5f },
      1401.8f, 1429.7f,  15.3f, -50.2f,  81.6f, -72.0f,  113.3f, -62.2f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "SHEPHERD", "AV" }, "392688" },
    // [62] road id 392684 -- "GABRIEL AV"
    { { -2.0f, -2.0f, 135.3f, 38.0f, -2.0f, -2.0f, 122.1f, 51.9f },
      { -133.1f, -25.5f, 4.3f, -24.5f },
      1355.1f, 1484.3f,  15.3f, -50.2f,  61.6f, -72.0f,  100.0f, -62.2f,
      RoadSign::E_SIGNSIZE_MID_2, { 30.0f, 20.0f }, { "GABRIEL", "AV" }, "392684" },
    // [63] road id 394853 -- "S. MOUNTAIN DR"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -156.9f, -18.0f, 25.4f, -17.0f },
      480.5f, 1487.0f,  15.3f, -50.2f,  81.6f, -72.0f,  126.6f, -69.9f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "S. MOUNTAIN", "DR" }, "394853" },
    // [64] road id 397134 -- "SOUTH BAY EXPWY"
    { { -2.0f, -2.0f, 177.7f, 38.0f, -2.0f, -2.0f, 122.1f, 32.0f },
      { -150.2f, -18.0f, 32.1f, -17.0f },
      1216.5f, 1517.0f,  15.3f, -50.2f,  81.6f, -72.0f,  92.8f, -69.8f,
      RoadSign::E_SIGNSIZE_LARGE, { 30.0f, 20.0f }, { "SOUTH BAY", "EXPWY" }, "397134" },
    };
}

namespace BrnGui
{
    // @0x8244BC90 -- fill all 65 records. The console stores the fields in scheduler order
    // (it interleaves four records' worth of loads); order of stores to distinct fields has
    // no observable effect, so the records are filled in index order here.
    void RoadSignList::Construct()
    {
        for (s32 liIndex = 0; liIndex < KI_NUM_ROAD_SIGNS; ++liIndex)
        {
            const RoadSignInit& lrInit = KA_ROAD_SIGNS[liIndex];
            RoadSign&           lrSign = maRoadSigns[liIndex];

            for (s32 liRow = 0; liRow < 2; ++liRow)
            {
                for (s32 liLane = 0; liLane < 4; ++liLane)
                    lrSign.mavTextBoxBounds[liRow][liLane] =
                        lrInit.afTextBoxBounds[liRow * 4 + liLane];

                lrSign.mavIndividualTextOffsets[liRow].x = lrInit.afIndividualTextOffsets[liRow * 2 + 0];
                lrSign.mavIndividualTextOffsets[liRow].y = lrInit.afIndividualTextOffsets[liRow * 2 + 1];
            }

            lrSign.mvCoords.x     = lrInit.fCoordsX;
            lrSign.mvCoords.y     = lrInit.fCoordsY;
            lrSign.mvPoleOffset.x = lrInit.fPoleOffsetX;
            lrSign.mvPoleOffset.y = lrInit.fPoleOffsetY;
            lrSign.mvSignOffset.x = lrInit.fSignOffsetX;
            lrSign.mvSignOffset.y = lrInit.fSignOffsetY;
            lrSign.mvTextOffset.x = lrInit.fTextOffsetX;
            lrSign.mvTextOffset.y = lrInit.fTextOffsetY;

            lrSign.meSignSize     = lrInit.eSignSize;
            lrSign.mafFontSize[0] = lrInit.afFontSize[0];
            lrSign.mafFontSize[1] = lrInit.afFontSize[1];
            lrSign.mapcText[0]    = lrInit.apcText[0];
            lrSign.mapcText[1]    = lrInit.apcText[1];
            lrSign.mpcRoadId      = lrInit.pcRoadId;
        }
    }
}
