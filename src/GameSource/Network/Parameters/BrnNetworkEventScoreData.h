#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceStructureInterface.h"

// ===================================================================================
// BrnNetwork::EventScoreData -- owning header
//   b5-decomp/src/GameSource/Network/Parameters/BrnNetworkEventScoreData.{h,cpp}
//
// The fixed-capacity batch of per-event scores the game uploads to the server in one
// "event scores" custom command. EventScoresManager::UpdateUploadEventScores builds one,
// fills it row-by-row (AddEventScore), and hands it to ServerInterfaceCustomCommands::
// UploadEventScoreData. The developer shortcut EventScoresManagerDebugComponent::
// SetCalvalryBurningRouteBest stack-constructs one and adds a single row.
//
// The X360 build's mangled name for this type is EventScoreUploadData; this codebase's
// committed manager / custom-commands / debug code refers to it as EventScoreData, so the
// class name is kept as EventScoreData (member fns EventScoreUploadData::{Construct,
// GetDataSize,SerialiseToString,SetScoreData,~} @ 0x82584AE0 / 0x82584D38 / 0x82584CA0 /
// 0x82584AF0 / 0x8254B468).
//
// It IS a serialisable structure: the vector deleting destructor @ 0x8254B468 installs the
// ServerInterfaceStructureInterface vtable (off_8207C88C) at this+0, and SerialiseToString
// dispatches through that interface's virtuals (GetPattern @ slot+0x04, GetDataSize @
// slot+0x0C, GetData @ slot+0x10). So EventScoreData derives from
// CgsNetwork::ServerInterfaceStructureInterface and overrides its pure virtuals.
//
// LAYOUT (X360-AUTHORITATIVE):
//   +0x00 (4)   vptr (base ServerInterfaceStructureInterface subobject)
//   +0x04 (60)  maScoreboardIndex  [15]   (s32)  -- SetScoreData stores a2 at 4*(count+1)
//   +0x40 (60)  maScore            [15]   (s32)  -- SetScoreData stores a3 at 4*(count+16)
//   +0x7C (60)  maGameMode         [15]   (s32)  -- SetScoreData stores a4 at 4*(count+31)
//   +0xB8 (4)   miNumScores               (s32)  -- asserted >=0 and < KI_MAX_EVENT_SCORES_TO_UPLOAD
// Total 188 bytes; Construct XMemSet-zeros the 184 payload bytes at this+4. GetDataSize
// returns 184 (0xB8) -- the payload size TagFieldSetStructure serialises.
//
// NOTE: the three parallel arrays' stride/offset is X360-authoritative; the field names
// are chosen to match the values the manager stores into them (scoreboard index / score /
// game mode). No DWARF survives for this type -- FLAG: rename if a richer source pins the
// true member spellings; the s32[15] size/offset must not change.
// ===================================================================================

namespace BrnNetwork
{
    class EventScoreData : public CgsNetwork::ServerInterfaceStructureInterface
    {
    public:
        // Maximum number of per-event scores carried in one upload batch. X360-authoritative:
        // SetScoreData asserts miNumScores < 15 (cmpwi r11, 0xF).
        static const s32 KI_MAX_EVENT_SCORES_TO_UPLOAD = 15;

        EventScoreData();
        virtual ~EventScoreData();

        // Construct @ 0x82584AE0 -- zero the whole 184-byte payload after the vptr. The default
        // ctor calls this so the manager's stack-constructed instance starts empty.
        void Construct();

        // Append one row {scoreboard-index, score, game-mode} at miNumScores and bump the count.
        // X360 SetScoreData @ 0x82584AF0. Preconditions: liScoreboardIndex >= 0; miNumScores in
        // [0, KI_MAX_EVENT_SCORES_TO_UPLOAD); per-game-mode index bound (burn-route 5: <35;
        // stunt-run 7: <14; any other mode unsupported -> dynamic assert). Returns true once the
        // batch is FULL. The committed manager / debug call sites name it AddEventScore.
        bool AddEventScore( s32 liScoreboardIndex, s32 liScore, s32 liGameMode );

        // Serialise the payload into the DirtySock record as one "EVSCORE" structure field.
        // X360 SerialiseToString @ 0x82584CA0.
        void SerialiseToString( char* lpcRecord, s32 liRecLen );

        // ServerInterfaceStructureInterface overrides. GetDataSize is X360 GetDataSize @
        // 0x82584D38 (returns 184); GetPattern / GetPatternLength / GetData bodies live in
        // other (out-of-batch) TUs -- declared here so SerialiseToString / the interface
        // contract resolve. FLAG: those override bodies are not yet reconstructed in-tree; the
        // vtable is anchored to this TU (destructor), so they must be provided before this
        // class is instantiated at link time.
        virtual const char* GetPattern() const override;
        virtual s32         GetPatternLength() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

    private:
        s32 maScoreboardIndex[KI_MAX_EVENT_SCORES_TO_UPLOAD];  // +0x04
        s32 maScore[KI_MAX_EVENT_SCORES_TO_UPLOAD];            // +0x40
        s32 maGameMode[KI_MAX_EVENT_SCORES_TO_UPLOAD];         // +0x7C
        s32 miNumScores;                                       // +0xB8
    };
} // namespace BrnNetwork
