// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 03: the alphabetical road index + the
// pool-construction wiring.
//   _RoadQSortFunction           @0x824867C8  (DWARF cpp:164, asserts cpp:173/174)
//   BuildAlphabeticalRoadIndexes @0x82486860  (DWARF cpp:183)
//   Construct                    @0x824866F0  (DWARF cpp:123)
//
//
// The committed leaf header BrnOnlineScoreboards.h is still the 29-line minimal version
// (the GetResourcesToLoad inline plus the two resource statics). The wave-I spec's
// full-shape class had not been applied when this partfile was written, and headers are
// frozen for implementers, so none of the three bodies can be declared as members, the
// nested RoadSortData record does not exist, and none of the ten members they touch
// exists. Measured with the compile gate, not assumed: cl /c reports
// "_RoadQSortFunction / BuildAlphabeticalRoadIndexes ist kein Member von
// BrnGui::OnlineScoreboards", "Construct: Memberfunktion wurde in
// BrnGui::OnlineScoreboards nicht deklariert", and undeclared RoadSortData,
// mpStateInterface, maiAlphabeticalRoadIndex, miCurrentCategory/Index/Variation,
// maacCategories/Indexes/Variations and mapcCategories/Indexes/Variations.
//
// The complete, drop-in-ready partfile (single `namespace BrnGui { ... }`, one anonymous
// namespace holding the 64-entry road-id table) lives at
// with a banner naming the exact declaration lines that unblock it. Copy it over this file
// once the spec's full-shape header lands; no edit is needed.
//
// EXACT DECLARATIONS THAT UNBLOCK IT (all inside
// `struct BrnGui::OnlineScoreboards : public CgsGui::State` -- every one of them is
// already part of the spec's full-shape header):
//
//     struct RoadSortData                              // DWARF h:122
//     {
//         static const s32 KI_MAX_ROAD_STRING = 20;    // DWARF h:124
//         s32  liIndex;                                // DWARF h:126
//         char lacRoadString[KI_MAX_ROAD_STRING];      // DWARF h:127
//     };
//
//     virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);  // @0x824866F0
//     static int _RoadQSortFunction(const void* lpRoadData1,
//                                   const void* lpRoadData2);          // @0x824867C8
//     void BuildAlphabeticalRoadIndexes();                             // @0x82486860
//
//     s32         maiAlphabeticalRoadIndex[64];   // X360 +84
//     s32         miCurrentCategory;              // X360 +344
//     char        maacCategories[15][31];         // X360 +352   (ARTIST count is 15)
//     const char* mapcCategories[15];             // X360 +820
//     s32         miCurrentIndex;                 // X360 +880
//     char        maacIndexes[10][31];            // X360 +888
//     const char* mapcIndexes[10];                // X360 +1200
//     s32         miCurrentVariation;             // X360 +1240
//     char        maacVariations[66][31];         // X360 +1248
//     const char* mapcVariations[66];             // X360 +3296
//
// mpStateInterface is already inherited from CgsGui::State. This group needs NO extra
// member-type includes, and it touches neither side of the
// BrnGuiEventTypeDefs.h / BrnGuiDemangledEventTypes.h collision.
//
// gate against a SHADOW copy of the header carrying only those declarations --
// scratchpad/waveI/probe03/ (run_probe.py prints PROBE_STATUS=pass). The faithfulness lint
// reports 0 new findings on it. No other declaration is needed.
//
// CgsStringUtils.h (CgsCore::SPrintf) + CgsGuiStateInterface.h
// (StateInterface::GetLanguageManager) + CgsLanguageManager.h (FormatText /
// E_FORMAT_ID_LOOKUP) + <cstdlib> (qsort).
//
// HOST-LAYOUT NOTE: the X360's baked 0x18 (24) sort-record stride, its 0x1F (31) text-row
// pitch, the +4 name offset inside the record and the 15/10/66 unrolled pointer-table
// `sizeof` or a named member instead; the console immediates survive only in comments.
//
// LINK NOTE for the conductor: nothing in this group is link-blocked. Every callee was
// checked for a real DEFINITION in the tree, not just a declaration (cl /c cannot see the
// difference): CgsCore::SPrintf (CgsStringUtils.cpp:31), CgsGui::State::Construct
// (CgsGuiState.cpp:52), CgsGui::StateInterface::GetLanguageManager
// (CgsGuiStateInterface.cpp:28), CgsLanguage::LanguageManager::FormatText(char*, u32,
// const char*, ParameterFormatType) (CgsLanguageManager.cpp:830 -- the X360 0x82864C48
// resolver) and the CGS_ASSERT trio (CgsAssert.cpp:18). qsort is the CRT.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatText / E_FORMAT_ID_LOOKUP
#include <cstdlib>   // qsort

namespace BrnGui
{

    namespace
    {
        // The screen offers one leaderboard per road, and the road filter lists them
        // alphabetically in the CURRENT language -- so the order cannot be baked into the
        // table and has to be rebuilt from the localised names every time the screen opens.
        //
        // FLAG consumer-named: this table is file-scope and unnamed on the X360
        // (qword_82029FA0 .. qword_8202A1A0, dumped in scratchpad/waveI/scoreboards_rodata.txt).
        // The 64 entries are localisation-database string ids -- BuildAlphabeticalRoadIndexes
        // renders each one decimally and resolves it through E_FORMAT_ID_LOOKUP.
        //
        // Element type is u64: the X360 walks the table with `ld` and a stride of 8. Every
        // entry's high word is zero, which is why the "%d" render below is well defined.
        const s32 KI_NUM_ALPHABETICAL_ROADS = 64;   // (0x8202A1A0 - 0x82029FA0) / 8

        const u64 KAU64_ALPHABETICAL_ROAD_IDS[KI_NUM_ALPHABETICAL_ROADS] =
        {
            383595, 385737, 385860, 386215, 386734, 387078, 387153, 387198,
            390723, 392323, 392532, 392589, 392684, 392688, 392997, 393062,
            393120, 393166, 393198, 393756, 393760, 393762, 393860, 393900,
            393911, 394273, 394306, 394474, 394487, 394853, 394965, 395197,
            395490, 395600, 395656, 395917, 395952, 396114, 396133, 396188,
            396228, 396456, 396460, 396706, 396718, 396742, 396988, 397134,
            397165, 397201, 397348, 397409, 397455, 397601, 397702, 505312,
            506394, 506519, 535195, 535196, 561121, 561413, 561415, 561416
        };

        // The decimal id is rendered into this many characters before it is handed to the
        // localisation lookup. Source constant (X360 `li r4, 0xA`), not a buffer size -- the
        // buffer itself is 16 bytes wide.
        const u32 KU_ROAD_ID_DIGITS = 10;
    }

    // ================================================================================
    //  _RoadQSortFunction  @ 0x824867C8  (cpp:164)
    //
    //  The qsort comparator BuildAlphabeticalRoadIndexes sorts the road records with:
    //  an ordinary lexicographic compare of the two localised road names. Written out
    //  by hand in the original (the X360 emits no strcmp call), so it is written out
    //  here too rather than forwarded to the CRT.
    // ================================================================================
    int OnlineScoreboards::_RoadQSortFunction(const void* lpRoadData1, const void* lpRoadData2)
    {
        // cpp:173 / cpp:174 -- both non-fatal on the X360: the compare runs regardless.
        CGS_ASSERT(lpRoadData1 != 0, "lpRoadData1");
        CGS_ASSERT(lpRoadData2 != 0, "lpRoadData2");

        // X360 `addi rN, rN, 4`: the compared field is the record's name, not the record.
        // Reached through the named member so the host's own offset is used.
        const char* lpcName1 = static_cast<const RoadSortData*>(lpRoadData1)->lacRoadString;
        const char* lpcName2 = static_cast<const RoadSortData*>(lpRoadData2)->lacRoadString;

        // The bytes are loaded with `lbz` (zero-extending), so the difference is taken
        // between UNSIGNED characters -- high-range localised bytes must sort above ASCII.
        s32 liResult;
        do
        {
            liResult = static_cast<s32>(static_cast<u8>(*lpcName1))
                     - static_cast<s32>(static_cast<u8>(*lpcName2));

            // Running off the end of the first name ends the compare whatever the
            // difference is (it is 0 only when both names ended together).
            if (*lpcName1 == '\0')
            {
                break;
            }

            ++lpcName1;
            ++lpcName2;
        }
        while (liResult == 0);

        return liResult;
    }

    // ================================================================================
    //  BuildAlphabeticalRoadIndexes  @ 0x82486860  (cpp:183)
    //
    //  Resolve all 64 road-name string ids in the current language, sort them by that
    //  localised name, and keep the permutation in maiAlphabeticalRoadIndex. The road
    //  filter then walks 0..63 and maps each slot through this index to get the table
    //  entry to request -- which is why the sort result is stored as indices and the
    //  names themselves are thrown away with the local array.
    //  Called once, from OnEnter.
    // ================================================================================
    void OnlineScoreboards::BuildAlphabeticalRoadIndexes()
    {
        RoadSortData laRecords[KI_NUM_ALPHABETICAL_ROADS];

        for (s32 liRoad = 0; liRoad < KI_NUM_ALPHABETICAL_ROADS; ++liRoad)
        {
            // Remember where this name started out, so the sort carries it along
            // (X360 `stw` -- the full index word, written before the name is filled in).
            laRecords[liRoad].liIndex = liRoad;

            // The localisation lookup takes the id as text.
            //
            // The X360 loads the whole doubleword into the vararg slot; "%d" consumes the
            // low word of that slot and every entry's high word is zero, so the rendered
            // digits are identical. Narrowing here makes the host's vararg slot unambiguous
            // rather than relying on that.
            char lacNumber[16];
            CgsCore::SPrintf(lacNumber, KU_ROAD_ID_DIGITS, "%d",
                             static_cast<s32>(KAU64_ALPHABETICAL_ROAD_IDS[liRoad]));

            // X360 r7 == 9 == E_FORMAT_ID_LOOKUP: resolve the digits as a database id.
            mpStateInterface->GetLanguageManager()->FormatText(
                laRecords[liRoad].lacRoadString,
                static_cast<u32>(sizeof(laRecords[liRoad].lacRoadString)),   // KI_MAX_ROAD_STRING
                lacNumber,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        }

        // The record stride is the HOST sizeof, never the X360's baked 0x18 (24) -- the
        // console record is 4 + 20 with no tail padding; the host record is not.
        qsort(laRecords,
              KI_NUM_ALPHABETICAL_ROADS,
              sizeof(RoadSortData),
              &OnlineScoreboards::_RoadQSortFunction);

        // Keep the permutation; the localised names were only ever the sort key.
        for (s32 liRoad = 0; liRoad < KI_NUM_ALPHABETICAL_ROADS; ++liRoad)
        {
            maiAlphabeticalRoadIndex[liRoad] = laRecords[liRoad].liIndex;
        }
    }

    // ================================================================================
    //  Construct  @ 0x824866F0  (cpp:123)
    //
    //  Pool-construction hook (BrnScreenFlow's NewPoolState<OnlineScoreboards> +
    //  Construct(CgsIDCompress("ON_SCOREB"), &lStateMachine)). Runs the base first, parks
    //  the three filter cursors on their first entry, and points each filter's pointer
    //  table at its own row of text storage -- the toggle group is handed the pointer
    //  table, so this wiring is what lets the list handlers just SPrintf into the rows.
    // ================================================================================
    void OnlineScoreboards::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CgsGui::State::Construct(liId, lpFsm);

        miCurrentCategory  = 0;
        miCurrentIndex     = 0;
        miCurrentVariation = 0;

        // Each of the three tables is `mapcX[i] = maacX[i]`. The X360 strides the text
        // storage by its baked row pitch (0x1F == 31) and unrolls the 10-entry index loop
        // outright; indexing the arrays by name reproduces both without either console
        // number appearing. Counts come from the declared arrays for the same reason
        // (X360: 15 categories, 10 indexes, 66 variations).
        const s32 liNumCategorySlots =
            static_cast<s32>(sizeof(mapcCategories) / sizeof(mapcCategories[0]));
        for (s32 liCategory = 0; liCategory < liNumCategorySlots; ++liCategory)
        {
            mapcCategories[liCategory] = maacCategories[liCategory];
        }

        const s32 liNumIndexSlots =
            static_cast<s32>(sizeof(mapcIndexes) / sizeof(mapcIndexes[0]));
        for (s32 liIndex = 0; liIndex < liNumIndexSlots; ++liIndex)
        {
            mapcIndexes[liIndex] = maacIndexes[liIndex];
        }

        const s32 liNumVariationSlots =
            static_cast<s32>(sizeof(mapcVariations) / sizeof(mapcVariations[0]));
        for (s32 liVariation = 0; liVariation < liNumVariationSlots; ++liVariation)
        {
            mapcVariations[liVariation] = maacVariations[liVariation];
        }
    }
}
