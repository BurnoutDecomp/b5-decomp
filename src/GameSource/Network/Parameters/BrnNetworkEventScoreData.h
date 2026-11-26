#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceStructureInterface.h"

// ===================================================================================
// BrnNetwork::EventScoreUploadData -- owning header
//   b5-decomp/src/GameSource/Network/Parameters/BrnNetworkEventScoreData.{h,cpp}
//
// The fixed-capacity batch of per-event scores the game uploads to the server in one
// "event scores" custom command. EventScoresManager::UpdateUploadEventScores builds one,
// fills it row-by-row (SetScoreData), and hands it to ServerInterfaceCustomCommands::
// UploadEventScoreData. The developer shortcut EventScoresManagerDebugComponent::
// SetCalvalryBurningRouteBest stack-constructs one and adds a single row.
//
// Names are ARTIST's: EventScoreUploadData::{Construct, SetScoreData, SerialiseToString,
// GetDataSize, `vector deleting destructor'} @ 0x82584AE0 / 0x82584AF0 / 0x82584CA0 /
// 0x82584D38 / 0x8254B468. No DWARF survives for this type.
//
// It IS a serialisable structure: the deleting destructor @ 0x8254B468 installs the
// ServerInterfaceStructureInterface vtable at this+0, and SerialiseToString dispatches
// through that interface's virtuals. Its constructor and destructor are implicit: the
// caller (UpdateUploadEventScores 0x8256C0E0) stores only the vtable, then calls Construct.
//
// The payload is one member, mEventScoreData (ARTIST's SetScoreData asserts read
// "mEventScoreData.miNumScores >= 0" @ 0x82584B50); its struct type is named after the file
// (BrnNetworkEventScoreData) -- the type name itself is not attested.
//
// LAYOUT (X360-AUTHORITATIVE):
//   +0x00 (4)   vptr (base ServerInterfaceStructureInterface subobject)
//   +0x04 (184) mEventScoreData
//       +0x00 (60)  maScoreboardIndex  [15]   (s32)  -- SetScoreData stores a2 at 4*(count+1)
//       +0x3C (60)  maScore            [15]   (s32)  -- SetScoreData stores a3 at 4*(count+16)
//       +0x78 (60)  maGameMode         [15]   (s32)  -- SetScoreData stores a4 at 4*(count+31)
//       +0xB4 (4)   miNumScores               (s32)  -- asserted >=0 and < KI_MAX_EVENT_SCORES_TO_UPLOAD
// Total 188 bytes; Construct XMemSet-zeros the 184-byte payload at this+4. GetDataSize
// returns 184 (0xB8) -- the payload size TagFieldSetStructure serialises.
//
// NOTE: the three parallel arrays' stride/offset is X360-authoritative; the field names
// are chosen to match the values the manager stores into them (scoreboard index / score /
// game mode). FLAG: rename if a richer source pins the true member spellings.
// ===================================================================================

namespace BrnNetwork
{
    // The online event leaderboards: one scoreboard per stunt-run event and one per burn-route
    // event. Scoreboard slot i of each mode belongs to the event whose id is entry i of that
    // mode's table (rodata tables read by the event-score uploader and the scoreboard browser).
    const s32 KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS  = 14;
    const s32 KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS = 35;

    const u64 KAU64_STUNT_RUN_SCOREBOARD_EVENT_IDS[KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS] =
    {
        481049u, 481074u, 481389u, 481037u, 481066u,
        480897u, 481387u, 480888u, 481005u, 481400u,
        481086u, 481026u, 481528u, 481384u,
    };

    const u64 KAU64_BURN_ROUTE_SCOREBOARD_EVENT_IDS[KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS] =
    {
        480974u, 481424u, 480973u, 480846u, 481040u,
        481012u, 480860u, 481027u, 481057u, 481050u,
        480998u, 480901u, 481030u, 480890u, 480899u,
        481045u, 481015u, 481069u, 481548u, 527090u,
        481034u, 480905u, 481061u, 481381u, 480854u,
        527091u, 480892u, 481083u, 481386u, 481402u,
        481032u, 481470u, 480847u, 481014u, 486462u,
    };

    // Maximum number of per-event scores carried in one upload batch. X360-authoritative:
    // SetScoreData asserts miNumScores < 15 (cmpwi r11, 0xF).
    const s32 KI_MAX_EVENT_SCORES_TO_UPLOAD = 15;

    // The serialised payload (184 bytes, pattern "l*").
    struct EventScoreData
    {
        s32 maScoreboardIndex[KI_MAX_EVENT_SCORES_TO_UPLOAD];  // +0x00
        s32 maScore[KI_MAX_EVENT_SCORES_TO_UPLOAD];            // +0x3C
        s32 maGameMode[KI_MAX_EVENT_SCORES_TO_UPLOAD];         // +0x78
        s32 miNumScores;                                       // +0xB4
    };

    class EventScoreUploadData : public CgsNetwork::ServerInterfaceStructureInterface
    {
    public:
        // Construct @ 0x82584AE0 -- zero the whole 184-byte payload after the vptr.
        void Construct();

        // Append one row {scoreboard-index, score, game-mode} at miNumScores and bump the count.
        // SetScoreData @ 0x82584AF0. Preconditions: liScoreboardIndex >= 0; miNumScores in
        // [0, KI_MAX_EVENT_SCORES_TO_UPLOAD); per-game-mode index bound (burn-route 5: <35;
        // stunt-run 7: <14; any other mode unsupported -> streamed assert). Returns true once
        // the batch is FULL.
        bool SetScoreData( s32 liScoreboardIndex, s32 liScore, s32 liGameMode );

        // Serialise the payload into the DirtySock record as one "EVSCORE" structure field.
        // SerialiseToString @ 0x82584CA0 (const: it dispatches the const GetData, slot +0x10).
        void SerialiseToString( char* lpcRecord, s32 liRecLen ) const;

        // ServerInterfaceStructureInterface overrides. GetDataSize returns 184. The class
        // vtable keeps the base GetPatternLength (its slot
        // holds the shared base body), so that virtual is NOT overridden here; GetPattern and
        // both GetData slots hold this class's own (identical-code-folded) bodies.
        virtual const char* GetPattern() const ;
        virtual u32         GetDataSize() const ;
        virtual void*       GetData() ;
        virtual const void* GetData() const ;

    private:
        EventScoreData mEventScoreData;   // +0x04
    };
} // namespace BrnNetwork
