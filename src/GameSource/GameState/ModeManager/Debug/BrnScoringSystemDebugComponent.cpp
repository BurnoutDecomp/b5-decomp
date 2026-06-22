#include "GameSource/GameState/ModeManager/Debug/BrnScoringSystemDebugComponent.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"                          // BrnGameState::ScoringSystem / CarData / CarScoreData
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"            // BrnNetworkModuleIO::InGamePlayerStatusInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameShared/GameClasses/Containers/CgsArray.h"                                          // Array<ChainableMultiplierInfo,8>
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"          // CgsDev::DebugInterface (+ DebugManager)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"                // CgsDev::DebugRender / RGBA / Vector2
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                          // CgsCore::StrCpy / SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"                                               // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The "Scoring System" chainable-stunt-multiplier debug
// table. OnActivate registers the single "Show chainable stunts" bool tweakable; GetName returns the
// menu label; DebugRenderChainableStunts draws a labelled (rows x columns) grid through the buffered
// debug 2D renderer; GetChainableTableEntry fills one cell's text + colour.
//
// The X360 inlines all the per-cell helpers (string-copy-with-overflow-assert, the per-car record
// lookups, the array element accessors). They are expressed here through the recovered named APIs:
//   * the cell-string copy = CgsCore::StrCpy(dest, 200, src)  (the inlined 200-byte CgsStringUtils copy)
//   * the per-car record    = mpScoringSystem->GetCarData(idx)->GetScoreData()  (X360 sub_8231DCD0 + +0)
//   * the chainable gather   = CarScoreData::GetChainableStuntMultipliers(...) into Array<...,8>
//   * the player record      = InGamePlayerStatusInterface::GetPlayerStatusData(idx)  (X360 ...::In)

namespace BrnGameState
{
    typedef GameStateModuleIO::CarScoreData                         CarScoreData;
    typedef CarScoreData::ChainableMultiplierInfo                   ChainableMultiplierInfo;
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveCarOutput;
    typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface PlayerStatusInterface;

    namespace
    {
        // Table layout constants (X360 inline immediates): the backing box origin (50,300), width 600,
        // a 24px row pitch, and the per-cell text scale 20. The X360 packs these as float immediates.
        const f32 KF_TABLE_ORIGIN_X   = 50.0f;
        const f32 KF_TABLE_ORIGIN_Y   = 300.0f;
        const f32 KF_TABLE_WIDTH      = 600.0f;
        const f32 KF_ROW_PITCH        = 24.0f;
        const f32 KF_TEXT_SCALE       = 20.0f;
        const f32 KF_CELL_GAP_X       = 8.0f;   // gap added after the widest cell of a column (X360 +8.0)
        const f32 KF_CELL_CHAR_WIDTH  = 4.0f;   // per-character advance estimate (X360 vrlimi128 width step)

        // The fixed row-header labels (X360 off_82CDB6FC[]). Row 0 is the player-name header column,
        // rows 1..6 are the chainable-stunt-multiplier bands. The leading "PLAYERS" is the column-0
        // header for the name row.
        const char* const KAPC_ROW_HEADERS[7] =
        {
            "PLAYERS", "x2", "x3", "x4", "x5", "x6", "x7",
        };

        // Per-row stunt-multiplier bit index: the bit tested in the gathered ChainableMultiplierInfo's
        // chainable-score mask to decide whether a band is "armed". One entry per band (rows 1..6 ->
        // indices 0..5). The X360 reads this from the dword_82020F54[a3-1] table, but that rodata's
        // CONTENTS are NOT in the exports. UNRECOVERED PLACEHOLDER: a best-effort identity mapping
        // (band N -> bit N); the real per-row bit indices are unknown and may differ.
        const s32 KAI_ROW_BIT_INDEX[6] = { 0, 1, 2, 3, 4, 5 };

        // File-scope cell colours. The X360 loads these as single constant words from dword_82CDB878
        // (box), dword_82CDB87C (default text) and dword_82CDB880 (active text), but those rodata words
        // are NOT in the exports. UNRECOVERED PLACEHOLDERS: best-effort RGBA8 values chosen to read
        // plausibly; the real X360 colour words are unknown.
        const CgsDev::RGBA KU_BOX_COLOUR          = 0x000000C0u; // placeholder: translucent black backing box
        const CgsDev::RGBA KU_CELL_DEFAULT_COLOUR = 0xFFFFFFFFu; // placeholder: white (inactive / header cell)
        const CgsDev::RGBA KU_CELL_ACTIVE_COLOUR  = 0xFF8000FFu; // placeholder: orange (armed/recent multiplier)

        inline Vector2 MakeVector2(f32 lfX, f32 lfY)
        {
            Vector2 lv2Result;
            lv2Result.x = lfX;
            lv2Result.y = lfY;
            lv2Result.z = 0.0f;
            lv2Result.w = 0.0f;
            return lv2Result;
        }

        inline u32 StringLength(const char* lpcString)
        {
            u32 luLen = 0;
            while (lpcString[luLen] != '\0')
                ++luLen;
            return luLen;
        }
    }

    // @ 0x82312470
    const char* ScoringSystemDebugComponent::GetName() const
    {
        return "Scoring System";
    }

    // @ 0x82312490 - register the local "Show chainable stunts" toggle with the debug menu. The X360
    // tail-calls DebugUI::VariableManager::RegisterVariable(&mbShowChainableStunts, "Show chainable
    // stunts"); the base RegisterVariable(bool*, const char*) is that path.
    void ScoringSystemDebugComponent::OnActivate()
    {
        RegisterVariable(&mbShowChainableStunts, "Show chainable stunts");
    }

    // @ 0x82329D60 - fill one table cell (text + colour).
    void ScoringSystemDebugComponent::GetChainableTableEntry(s32 liColumn, s32 liRow,
                                                             const ActiveCarOutput* lpActiveCarOutput,
                                                             const PlayerStatusInterface* lpPlayerStatusInterface,
                                                             s32 liMaxMultiplier,
                                                             bool lbThirtyFps,
                                                             char* lpcOutBuffer,
                                                             u32* lpuOutColour)
    {
        CGS_ASSERT(mpScoringSystem != nullptr, "mpScoringSystem");
        CGS_ASSERT(lpPlayerStatusInterface != nullptr, "lpPlayerStatusInterface");

        // Default: an empty cell drawn in the default (white) colour.
        *lpuOutColour = KU_CELL_DEFAULT_COLOUR;
        CgsCore::StrCpy(lpcOutBuffer, 200, "");

        if (liColumn == 0)
        {
            // Column 0 = the fixed row-header label.
            CgsCore::StrCpy(lpcOutBuffer, 200, KAPC_ROW_HEADERS[liRow]);
            return;
        }

        // Player columns: liColumn-1 indexes the network player list.
        const s32 liPlayerIndex = liColumn - 1;

        if (liRow == 0)
        {
            // Row 0 = the player-name header for this column.
            if (liPlayerIndex < lpPlayerStatusInterface->GetNumPlayers())
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData =
                    lpPlayerStatusInterface->GetPlayerStatusData(liPlayerIndex);
                if (lpPlayerData != nullptr)
                    CgsCore::StrCpy(lpcOutBuffer, 200, lpPlayerData->mPlayerName.macName);
            }
            return;
        }

        // Rows 1..6 = a chainable-stunt-multiplier band for this player.
        if (liPlayerIndex >= lpPlayerStatusInterface->GetNumPlayers())
            return;

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerData =
            lpPlayerStatusInterface->GetPlayerStatusData(liPlayerIndex);
        if (lpPlayerData == nullptr)
            return;

        // meActiveRaceCarIndex is the BrnNetwork degenerate enum (s32-wide); the ScoringSystem car
        // lookup keys off the game-wide ::EActiveRaceCarIndex. The X360 treats both as the same s32.
        const EActiveRaceCarIndex leRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(static_cast<s32>(lpPlayerData->meActiveRaceCarIndex));
        if (leRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return;

        CarData* lpCarData = mpScoringSystem->GetCarData(leRaceCarIndex);
        if (lpCarData == nullptr)
            return;

        // Gather this car's chainable-multiplier table for the current band, then test the band's bit.
        const CarScoreData* lpScoreData = lpCarData->GetScoreData();
        Array<ChainableMultiplierInfo, 8> laMultiplierInfo;
        lpScoreData->GetChainableStuntMultipliers(liMaxMultiplier, -1, &laMultiplierInfo);

        if (laMultiplierInfo.GetCount() == 0)
            return;

        const ChainableMultiplierInfo& lEntry = laMultiplierInfo.Ge(0);
        const s32 liBit = 1 << KAI_ROW_BIT_INDEX[liRow - 1];
        if ((lEntry.miChainableScore & liBit) != liBit)
            return;

        CGS_ASSERT(lpActiveCarOutput->IsPlayerCarActive(), "lpLocalPlayersCarState");

        // The X360 then runs a screen-space proximity cull on the local player's car position/forward
        // (RaceCarState +0x220 position / +0x330 forward) before emitting the timer text, and only
        // draws the cell when the other car is within ~320m and ahead. FLAG: the RaceCarState byte
        // layout (position/forward at the +0x220/+0x330 offsets the cull reads) is NOT homed in-tree
        // (BrnPhysics::Vehicle::RaceCarState is only forward-declared everywhere); reproducing the cull
        // would require raw-offset reads, which the HARD RULES forbid (members BY NAME, no offset
        // casts). The cull is a render-time visibility gate only -- it does not change the cell's text/
        // colour -- so this models the post-cull result: emit the band timer text in the active colour.
        // When RaceCarState gains a homed layout (GetPosition/GetForward accessors) the cull should be
        // restored here using the named accessors off lpActiveCarOutput->GetPlayerRaceCarState() and the
        // other car's GetRaceCarState(leRaceCarIndex).

        // Timer text: the band's remaining-window in seconds. The X360 derives the frame count from the
        // gathered entry's supplied/field deltas (+300 frames) and scales by the frame period (1/50 at
        // 30fps display, 1/60 otherwise), formatting "%.3fs". An armed band under 1s flips to the active
        // colour.
        const s32 liFrames = (lEntry.miSuppliedValue - liMaxMultiplier) + 300;
        const f32 lfFramePeriod = lbThirtyFps ? 0.02f : 0.016666668f;
        const f32 lfSeconds = static_cast<f32>(liFrames) * lfFramePeriod;
        CgsCore::SPrintf(lpcOutBuffer, 200, "%.3fs", static_cast<f64>(lfSeconds));
        if (lfSeconds <= 1.0f)
            *lpuOutColour = KU_CELL_ACTIVE_COLOUR;
    }

    // @ 0x82337C38 - draw the live chainable-stunt multiplier table.
    void ScoringSystemDebugComponent::DebugRenderChainableStunts(const ActiveCarOutput* lpActiveCarOutput,
                                                                 const PlayerStatusInterface* lpPlayerStatusInterface,
                                                                 s32 liMaxMultiplier,
                                                                 bool lbThirtyFps)
    {
        CGS_ASSERT(mpScoringSystem != nullptr, "mpScoringSystem");

        // Only render when the toggle is on AND the mode has at least one car.
        if (!mbShowChainableStunts)
            return;
        const u32 luNumCars = mpScoringSystem->GetNumberOfActiveCars();
        if (luNumCars == 0)
            return;

        // The number of table columns = the header column + one column per car (X360 muCarsInCurrentMode
        // + 1). The X360 forwards its own received maxMultiplier (a6) and 30fps display flag (a7) to
        // every GetChainableTableEntry call unchanged (saved r22/r21 at 0x82337C68-6C).
        const s32 liNumColumns = static_cast<s32>(luNumCars) + 1;

        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugRender& lRender = lDebugInterface.Get2dRender();

        // Backing box: from the table origin, 600 wide x (columns * row pitch) tall.
        const Vector2 lv2BoxMin = MakeVector2(KF_TABLE_ORIGIN_X, KF_TABLE_ORIGIN_Y);
        const Vector2 lv2BoxMax = MakeVector2(KF_TABLE_ORIGIN_X + KF_TABLE_WIDTH,
                                              KF_TABLE_ORIGIN_Y + static_cast<f32>(liNumColumns) * KF_ROW_PITCH);
        lRender.Draw2DBox(lv2BoxMin, lv2BoxMax, KU_BOX_COLOUR);

        char lacCell[280];

        // Walk the grid column-major (the X360 outer loop is the 7 rows, inner is the columns), tracking
        // an x pen that advances by the widest cell drawn in each column so columns stay aligned.
        f32 lfPenX = KF_TABLE_ORIGIN_X;
        for (s32 liRow = 0; liRow < 7; ++liRow)
        {
            f32 lfMaxCellWidth = 0.0f;
            f32 lfPenY = KF_TABLE_ORIGIN_Y;
            for (s32 liColumn = 0; liColumn < liNumColumns; ++liColumn)
            {
                u32 luCellColour = KU_CELL_DEFAULT_COLOUR;
                GetChainableTableEntry(liColumn, liRow, lpActiveCarOutput,
                                       lpPlayerStatusInterface, liMaxMultiplier, lbThirtyFps,
                                       lacCell, &luCellColour);

                const Vector2 lv2CellPos = MakeVector2(lfPenX, lfPenY);
                lRender.Draw2DText(lacCell, lv2CellPos, KF_TEXT_SCALE, luCellColour);

                const f32 lfCellWidth = static_cast<f32>(StringLength(lacCell)) * KF_CELL_CHAR_WIDTH;
                if (lfCellWidth > lfMaxCellWidth)
                    lfMaxCellWidth = lfCellWidth;

                lfPenY += KF_ROW_PITCH;
            }
            lfPenX += lfMaxCellWidth + KF_CELL_GAP_X;
        }

        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }
}
