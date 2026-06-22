#pragma once

// ===================================================================================
// BrnNetwork::CollectableMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCollectableMessage.h
//
// A CgsNetwork::ReliableMessage subclass announcing a collectable pickup (jump / smash
// / billboard) keyed by CgsID. Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnCollectableMessage.h)
// and gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed
// home in CgsReliableMessage.h and is reused BY NAME here -- not forked. The base is
// size 0x28 (adds no data of its own), so the first leaf member lands at +0x28.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CollectableMessage::GetName  @ 0x827DFBB8
//     -> returns the literal "Collectable Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/PrepareForSend/Retrieve/
// GetPackedMessageSize/OldMessagesAreValid/PackOrUnpack) live in the sibling .cpp TU
// and are declared here for the class shape but NOT bodied in this TU.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                 // CgsID (committed typedef)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)

namespace BrnNetwork
{
    struct CollectableMessage : public CgsNetwork::ReliableMessage
    {
        // DWARF BrnCollectableMessage.h:47 -- which kind of collectable was picked up.
        enum ECollectableType
        {
            E_COLLECTABLE_TYPE_JUMP      = 0,
            E_COLLECTABLE_TYPE_SMASH     = 1,
            E_COLLECTABLE_TYPE_BILLBOARD = 2,
            E_COLLECTABLE_TYPE_MAX       = 3,
        };

        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          PrepareForSend(u16 lu16Frame, CgsID lCollectableID, ECollectableType leType);
        bool          Retrieve(CgsID* lpCollectableID, ECollectableType* lpeType);
        virtual s32   GetPackedMessageSize();        // DWARF :131 (sibling .cpp)

        // LEDGER func @ 0x827DFBB8 -- bodied in this TU (DWARF BrnCollectableMessage.h:103).
        virtual const char* GetName() const;

        virtual bool  OldMessagesAreValid() const;   // DWARF :173 (sibling .cpp)

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        CgsID            mCollectableID;             // DWARF :92 (+0x28, first leaf member)
        ECollectableType meType;                     // DWARF :93
    };

    // BrnNetwork::CollectableMessage::GetName  @ 0x827DFBB8
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* CollectableMessage::GetName() const
    {
        return "Collectable Message";
    }
} // namespace BrnNetwork
