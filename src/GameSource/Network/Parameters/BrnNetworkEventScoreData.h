#pragma once

#include "types.hpp"

// ===================================================================================
// BrnNetwork::EventScoreData -- owning header
//   b5-decomp/src/GameSource/Network/Parameters/BrnNetworkEventScoreData.{h,cpp}
//
// The fixed-capacity batch of per-event scores the game uploads to the server in one
// "event scores" custom command. EventScoresManagerDebugComponent::SetCalvalryBurningRouteBest
// builds one of these on the stack (developer shortcut), and
// ServerInterfaceCustomCommands::UploadEventScoreData serialises it.
//
// LAYOUT (X360-AUTHORITATIVE, read off SetCalvalryBurningRouteBest @ 0x82591D00 -- the debug
// component constructs the object at sp+var_E0 and the three element stores plus the count are
// at the offsets below; XMemSet zeros the 184 data bytes after the vptr; UploadEventScoreData
// receives the object by pointer):
//   +0x00 (4)   vptr                          (off_8207F2F8 is its vtable; >=1 virtual)
//   +0x04 (60)  maEventScoreType   [15]       (s32; SetCalvalryBurningRouteBest stores 6)
//   +0x40 (60)  maScore            [15]       (s32; SetCalvalryBurningRouteBest stores 50000)
//   +0x7C (60)  maEventScoreSubType[15]       (s32; SetCalvalryBurningRouteBest stores 5)
//   +0xB8 (4)   miNumScores                   (s32; asserted >=0 and < KI_MAX_EVENT_SCORES_TO_UPLOAD)
// Total 188 bytes.
//
// NOTE on the three parallel arrays: only their stride/offset and the literals stored into them
// (6 / 50000 / 5) are X360-authoritative. No DWARF survives for this type, so the field NAMES
// below are descriptive placeholders chosen to match the values (the 50000 slot is plainly the
// score). FLAG: rename maEventScoreType / maEventScoreSubType if a DWARF or richer call site
// later pins their true meaning; the s32[15] size/offset must not change.
// ===================================================================================

namespace BrnNetwork
{
    class EventScoreData
    {
    public:
        // Maximum number of per-event scores carried in one upload batch. X360-authoritative:
        // SetCalvalryBurningRouteBest asserts miNumScores < 15 (cmpwi r11, 0xF).
        static const s32 KI_MAX_EVENT_SCORES_TO_UPLOAD = 15;

        EventScoreData();
        virtual ~EventScoreData();

        // Append one scored event. The bounds asserts the debug component triggers
        // (miNumScores >= 0 and < KI_MAX_EVENT_SCORES_TO_UPLOAD) live in this type's TU
        // (BrnNetworkEventScoreData.cpp), so the per-slot store + count bump is owned here.
        // Bodied in another TU; declared here for the debug component's `cl /c` gate.
        void AddEventScore( s32 liEventScoreType, s32 liScore, s32 liEventScoreSubType );

    private:
        s32 maEventScoreType[KI_MAX_EVENT_SCORES_TO_UPLOAD];     // +0x04
        s32 maScore[KI_MAX_EVENT_SCORES_TO_UPLOAD];              // +0x40
        s32 maEventScoreSubType[KI_MAX_EVENT_SCORES_TO_UPLOAD];  // +0x7C
        s32 miNumScores;                                         // +0xB8
    };
} // namespace BrnNetwork
