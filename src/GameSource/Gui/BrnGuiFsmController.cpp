// BrnGuiFsmController.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiFsmController::IsTransitionPending @ 0x824ECCF8
//
//   if !(luFlowId in [0,2]):  build "Invalid Flow Id passed" and fire (DWARF :190)
//   if mapFlows[luFlowId] == 0: build "Error, flow not set" and fire (DWARF :191)
//   return mabTransitionPending[luFlowId]    ; lbz r3, 0x94(luFlowId + this)
//
//   - flow lookup: `slwi r,a2,2 ; lwzx r,r,this`  -> mapFlows[a2] (4-byte slots @ +0x00)
//   - return byte: `add r,a2,this ; lbz r3,0x94(r)` -> mabTransitionPending[a2] (1-byte
//     slots @ +0x94, zero-extended).
//
// Both guards are non-fatal: the X360 returns the stored byte regardless. The X360
// builds each assert message through a CgsDev::StrStream over the shared assert buffer;
// reproduced here with the same stream. The X360-baked file/line are discarded per
// project convention; the stringized condition text matches the X360 messages.

#include "GameSource/Gui/BrnGuiFsmController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert::Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream

namespace BrnGui
{

// @ 0x824ECCF8
bool GuiFsmController::IsTransitionPending( u32 luFlowId ) const
{
    // ---- guard 1: valid flow id (X360: signed<0 || >=3) ----
    if ( luFlowId >= KU_NUM_FLOWS )
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Invalid Flow Id passed";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    // ---- guard 2: flow is set ----
    if ( mapFlows[luFlowId] == nullptr )
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Error, flow not set";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    return mabTransitionPending[luFlowId] != 0;
}

} // namespace BrnGui
