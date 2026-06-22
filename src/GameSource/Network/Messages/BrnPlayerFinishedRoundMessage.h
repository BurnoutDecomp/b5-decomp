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
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::PlayerFinishedRoundMessage::GetName  @ 0x827DFCD0
//     -> returns the literal "Player Finished Round Message" (lis/addi a rodata string,
//        blr). No member or base access.
//
// The other declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack and the inline getters) live in the sibling
// BrnPlayerFinishedRoundMessage.cpp TU and are declared here for class shape but NOT
// bodied in this TU.
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
        bool            mbTimedOut;                  // DWARF :109
        u8              mu8RoundNumber;              // DWARF :110
        NetworkPlayerID mEliminatorNetworkPlayerID;  // DWARF :111
        f32             mfDistanceFromFinish;        // DWARF :112
        Time            mFinishTime;                 // DWARF :113
        s32             miEliminations;              // DWARF :114

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, u8 lu8RoundNumber, Time lFinishTime,
                            f32 lfDistanceFromFinish, NetworkPlayerID lEliminatorNetworkPlayerID,
                            s32 liEliminations, bool lbTimedOut);
        bool Retrieve(u8* lpu8RoundNumber, Time* lpFinishTime, f32* lpfDistanceFromFinish,
                      NetworkPlayerID* lpEliminatorNetworkPlayerID, s32* lpiEliminations,
                      bool* lpbTimedOut);
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
