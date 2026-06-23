// BrnGuiEventRoadRuleUpcomingRoads.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiEventRoadRuleUpcomingRoads::ConvertG (truncated) @ 0x824F6170
//
// Maps a 3-valued game-state enum to this event's road category id. Two non-fatal
// guards bracket the switch:
//
//   1. if !(luGameState in [0,2]):  build "Invalid enum after cast - original value was
//      <v>, cast to <v>\n" into the assert buffer and fire (DWARF :513).
//   2. switch: 0 -> return 0; 1 -> return 2; 2 -> return 1; default: build
//      "Invalid gamestate enum (<v>)\n" and fire (DWARF :539), then fall through to
//      return 0.
//
// The X360 builds each assert message by streaming the runtime value through a
// CgsDev::StrStream over the shared assert message buffer; reproduced here with the
// same stream so the formatted value is preserved. The X360-baked file/line are
// discarded per project convention.

#include "GameSource/Gui/Events/BrnGuiEventRoadRuleUpcomingRoads.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert::Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream

namespace BrnGui
{

// @ 0x824F6170
s32 GuiEventRoadRuleUpcomingRoads::ConvertGameStateToCategory( u32 luGameState )
{
    // ---- guard 1: post-cast range check (X360: signed<0 || >=3) ----
    if ( luGameState > 2u )
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Invalid enum after cast - original value was "
                   << static_cast<s32>( luGameState )
                   << ", cast to "
                   << static_cast<s32>( luGameState )
                   << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    // ---- the X360 branch table (0->0, 1->2, 2->1) ----
    switch ( luGameState )
    {
        case 0u:
            return 0;
        case 1u:
            return 2;
        case 2u:
            return 1;
        default:
            break;
    }

    // ---- guard 2: unreachable for valid input; the X360 default arm ----
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Invalid gamestate enum ("
                   << static_cast<s32>( luGameState )
                   << ")\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }
    return 0;
}

} // namespace BrnGui
