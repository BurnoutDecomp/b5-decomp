// ===================================================================================
// BrnNetwork::PlayerFinishedRoundMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h
//
// A CgsNetwork::ReliableMessage subclass reporting that a player has finished an
// elimination round (round number, finish time, distance-from-finish, who eliminated
// them, eliminations scored, timed-out flag). Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h);
// the base (CgsNetwork::ReliableMessage) is the committed home in CgsReliableMessage.h and
// is reused BY NAME here -- not forked. CgsSystem::Time is the committed time value type.
//
// MEMBER LAYOUT / ORDER: the X360 ARTIST .XEX (rung 1) is authoritative for the byte
// offsets, and it does NOT match the DWARF's declaration order. Construct @ 0x8257A188,
// PrepareForSend @ 0x8257D9A0 and Retrieve @ 0x8257DB78 fix the real offsets (leaf data
// starts at +0x28, after the size-0x28 ReliableMessage/MessageWithPlayerIDs base):
//     +0x28  mFinishTime               (Time: miSeconds@+0x28, mfFraction@+0x2C)
//     +0x30  mEliminatorNetworkPlayerID(stw, NetworkPlayerID)
//     +0x34  miEliminations            (stw, s32; PackOrUnpack range [0,7])
//     +0x38  mfDistanceFromFinish      (stfs, f32)
//     +0x3C  mu8RoundNumber            (stb, u8; PackOrUnpack range [0,10])
//     +0x3D  mbTimedOut                (stb, bool)
//     +0x3E  mbWonRound                (stb, bool)  <-- see note below
// so the members are declared in OFFSET order here (the DWARF list -- mbTimedOut first,
// mFinishTime fifth -- would mis-place every field).
//
// X360-LEDGER-ATTESTED MISSING MEMBER: the binary carries a SEVENTH leaf scalar -- a
// second bool at +0x3E -- that the DWARF (which models this class with a single bool
// mbTimedOut) omits. Construct/GetPackedMessageSize zero it, PrepareForSend stores it
// (its last, stack-passed argument), Retrieve copies it out (the 7th out-pointer, r10),
// and PackOrUnpack (de)serialises it as a bool right after mbTimedOut (two back-to-back
// sub_8288DDA0 bool calls at +0x3D then +0x3E). Its source name is not attested; named
// mbWonRound here as a plausible round-outcome companion to mbTimedOut -- FLAGGED.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::PlayerFinishedRoundMessage::GetName  @ 0x827DFCD0
//     -> returns the literal "Player Finished Round Message" (lis/addi a rodata string,
//        blr). No member or base access.
//
// The other declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) are bodied in the sibling
// BrnPlayerFinishedRoundMessage.cpp TU; the inline getters are declared here for class
// shape (inlined-only in the binary, no standalone ledger body).
#pragma once

#include "types.hpp"                                                               // bool, s32, u8, f32
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                           // CgsSystem::Time (committed)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                        // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    // The DWARF/source spell CgsSystem::Time unqualified as `Time` (matches the established
    // BrnNetworkPlayerStats convention); aliased here so the message signatures read as in
    // the DWARF without re-forking the committed type.
    typedef CgsSystem::Time Time;

    // DWARF BrnPlayerFinishedRoundMessage.h:48. The DWARF spells the eliminator field
    // RoadRulesRecvData::NetworkPlayerID; that nested name is an alias of the committed
    // BrnNetwork::NetworkPlayerID (== s32), reused here by name.
    struct PlayerFinishedRoundMessage : public CgsNetwork::ReliableMessage
    {
    private:
        // Declared in X360 byte-offset order (rung 1), not DWARF declaration order.
        Time            mFinishTime;                 // +0x28  DWARF :113
        NetworkPlayerID mEliminatorNetworkPlayerID;  // +0x30  DWARF :111
        s32             miEliminations;              // +0x34  DWARF :114
        f32             mfDistanceFromFinish;        // +0x38  DWARF :112
        u8              mu8RoundNumber;              // +0x3C  DWARF :110
        bool            mbTimedOut;                  // +0x3D  DWARF :109
        bool            mbWonRound;                  // +0x3E  X360-attested, DWARF-omitted (FLAGGED name)

    public:
        // Sibling-.cpp methods (bodied in BrnPlayerFinishedRoundMessage.cpp).
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, u8 lu8RoundNumber, Time lFinishTime,
                            f32 lfDistanceFromFinish, NetworkPlayerID lEliminatorNetworkPlayerID,
                            s32 liEliminations, bool lbTimedOut, bool lbWonRound);
        bool Retrieve(u8* lpu8RoundNumber, Time* lpFinishTime, f32* lpfDistanceFromFinish,
                      NetworkPlayerID* lpEliminatorNetworkPlayerID, s32* lpiEliminations,
                      bool* lpbTimedOut, bool* lpbWonRound);
        virtual s32 GetPackedMessageSize();

        u8              GetRoundNumber() const;
        Time            GetFinishTime() const;
        f32             GetDistanceFromFinish() const;
        NetworkPlayerID GetEliminatorNetworkPlayerID() const;
        bool            GetTimedOut() const;

        // LEDGER func @ 0x827DFCD0 -- bodied in this TU.
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };

    // BrnNetwork::PlayerFinishedRoundMessage::GetName  @ 0x827DFCD0
    inline const char* PlayerFinishedRoundMessage::GetName() const
    {
        return "Player Finished Round Message";
    }
} // namespace BrnNetwork
