#pragma once

// ===================================================================================
// BrnNetwork::SelectedRoutesMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnSelectedRoutesMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying one selected-routes event payload
// (a SpecificGameModeEventInterface::Event record) plus the round number it applies
// to. Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnSelectedRoutesMessage.h:47)
// and gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed
// home in CgsReliableMessage.h and the embedded Event type is the committed home in
// BrnGameStateSharedIO.h; both are reused BY NAME here -- not forked.
//
// LAYOUT: ReliableMessage adds no data of its own (sizeof == 0x28), so the first leaf
// member lands at +0x28:
//   +0x00  (CgsNetwork::ReliableMessage base, 0x28 bytes)
//   +0x28  SpecificGameModeEventInterface::Event mEvent (44 bytes -> +0x54)
//   +0x54  s32 miRoundNumber
//
// LEDGER FUNCTION bodied in this (header) TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::SelectedRoutesMessage::GetName  @ 0x827DFD30
//     -> returns the literal "Selected Routes Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/OldMessagesAreValid/PackOrUnpack) live in the sibling .cpp TU
// (BrnSelectedRoutesMessage.cpp) and are declared here for the class shape but NOT
// bodied in this TU.
// ===================================================================================

#include "types.hpp"                                                                  // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"     // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                // SpecificGameModeEventInterface::Event (committed home)

namespace BrnNetwork
{
    // DWARF BrnSelectedRoutesMessage.h:47 -- the selected-routes reliable message.
    struct SelectedRoutesMessage : public CgsNetwork::ReliableMessage
    {
        // The embedded per-event record type (committed home in BrnGameStateSharedIO.h),
        // reused BY NAME -- not forked.
        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event Event;

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16Frame, s32 liRoundNumber, Event* lpEvent);
        bool          Retrieve(s32* lpiRoundNumber, Event* lpEvent);
        virtual s32   GetPackedMessageSize();          // DWARF :207 (sibling .cpp)
        virtual bool  OldMessagesAreValid() const;     // DWARF :238 (sibling .cpp)

        // LEDGER func @ 0x827DFD30 -- bodied inline below (DWARF BrnSelectedRoutesMessage.h:99).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        Event mEvent;          // +0x28 (DWARF :88)
        s32   miRoundNumber;   // +0x54 (DWARF :89)
    };

    // BrnNetwork::SelectedRoutesMessage::GetName  @ 0x827DFD30
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* SelectedRoutesMessage::GetName() const
    {
        return "Selected Routes Message";
    }
} // namespace BrnNetwork
