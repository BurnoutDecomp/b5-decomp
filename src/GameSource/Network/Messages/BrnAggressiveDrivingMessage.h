// ===================================================================================
// BrnNetwork::AggressiveDrivingMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnAggressiveDrivingMessage.h
//
// A CgsNetwork::Message subclass carrying up to KI_MAX_AGGRESSIVE_MOVES aggressive-move
// records. Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnAggressiveDrivingMessage.h);
// the base class (CgsNetwork::Message) is the committed home in CgsMessage.h and is
// reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::AggressiveDrivingMessage::GetName  @ 0x827DE0A8
//     -> returns the literal "Aggressive Driving Message" (lis/addi a rodata string,
//        blr). No member or base access.
//
// The remaining declared methods (Construct/PrepareForSend/Release/Destruct/
// GetPackedMessageSize/GetNumberOfAggressiveMoves/GetAggressiveMove/Retrieve/PackOrUnpack)
// live in sibling .cpp TUs (BrnAggressiveDrivingMessage.cpp) and are declared here for
// the class shape but NOT bodied in this TU.
#pragma once

#include "types.hpp"                                                       // s32, u8, bool, f32
#include "rw/math/vpu/types.h"                                             // rw::math::vpu::Vector3
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::Message (committed base)
#include "GameSource/GameState/BrnTakedownType.h"                          // BrnGameState::ETakedownType (committed home)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    using ::rw::math::vpu::Vector3;

    // DWARF BrnAggressiveDrivingMessage.h:38 -- the kind of aggressive move recorded.
    enum EAggressiveMoveType : s32
    {
        E_AGGRESSIVE_MOVE_TAKE_DOWN      = 0,
        E_AGGRESSIVE_MOVE_SLAM           = 1,
        E_AGGRESSIVE_MOVE_SHUNT          = 2,
        E_AGGRESSIVE_MOVE_TRADING_PAINT  = 3,
        E_AGGRESSIVE_MOVE_BATTLING       = 4,
        E_AGGRESSIVE_MOVE_COUNT          = 5,
    };

    // DWARF BrnAggressiveDrivingMessage.h:49-55 -- record-count / index bounds.
    const s32 KI_MAX_AGGRESSIVE_MOVE_TYPE = 5;
    const s32 KI_MAX_AGGRESSIVE_MOVES     = 5;
    const s32 KI_MAX_RACE_CARS            = 8;

    // DWARF BrnAggressiveDrivingMessage.h:61 -- one aggressive-move record. Member names
    // and logical types are DWARF-derived; project scalar types used (f32, not float32_t).
    // The DWARF spells the two player-id fields RoadRulesRecvData::NetworkPlayerID -- that
    // nested name is an alias of the committed BrnNetwork::NetworkPlayerID (== s32), reused
    // here by name.
    struct AggressiveMoveData
    {
        EAggressiveMoveType      meAggressiveMoveType;       // +0x00
        BrnGameState::ETakedownType meTakedownType;          // +0x04
        NetworkPlayerID          mAggressorNetworkPlayerID;  // +0x08
        NetworkPlayerID          mVictimNetworkPlayerID;     // +0x0C
        bool                     mbMarkedMan;                // +0x10
        bool                     mbSettledScore;             // +0x11
        u8                       muImpactScore;              // +0x12
        Vector3                  mDirection;                 // +0x20 (alignas(16))
        f32                      mfMagnitude;                // +0x30
        f32                      mfDuration;                 // +0x34
        f32                      mfSteeringDirection;        // +0x38
        f32                      mfRecoveryTime;             // +0x3C
        s32                      miNumberOfTimesTriedToSend; // +0x40

        void Clear();   // DWARF BrnAggressiveDrivingMessage.h:80 (sibling .cpp; declared only).
    };

    // DWARF BrnAggressiveDrivingMessage.h:108 -- the network message itself.
    struct AggressiveDrivingMessage : public CgsNetwork::Message
    {
    private:
        s32               miNumberOfAggressiveMoves;                       // DWARF :150
        AggressiveMoveData maAggressiveMoveDataArray[KI_MAX_AGGRESSIVE_MOVES]; // DWARF :151

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void PrepareForSend(u16 lu16Frame, s32 liNumberOfMoves, AggressiveMoveData* lpMoves);
        void Release();
        void Destruct();
        virtual s32  GetPackedMessageSize();
        s32          GetNumberOfAggressiveMoves();
        const AggressiveMoveData* GetAggressiveMove(s32 liIndex);
        bool         Retrieve(s32* lpiNumberOfMoves, AggressiveMoveData* lpMoves);

        // LEDGER func @ 0x827DE0A8 -- bodied in this TU.
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };

    // BrnNetwork::AggressiveDrivingMessage::GetName  @ 0x827DE0A8
    inline const char* AggressiveDrivingMessage::GetName() const
    {
        return "Aggressive Driving Message";
    }
} // namespace BrnNetwork
