// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.cpp
//
// Out-of-line body for BrnWorld::PropEntityIO::PropToTrafficInterface::RequestTrafficLightKnockDown,
// reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   RequestTrafficLightKnockDown @ 0x822CD378:
//     assert luInstanceID != 0   (cmplwi r31,0; bne; FireAssert "luInstanceID != 0", :149)
//     TrafficLightKnockDownEvent lEvent = { luInstanceID };
//     mTrafficLightKnockDownQueue.AddEvent(lEvent);   (stw r31 ; addi r4,&ev ; mr r3,this ; bl AddEvent)
//
// The asm materialises the event on the stack as a single 4-byte word (`stw r31, var_20(r1)`) and
// passes its address to BaseEventQueue<TrafficLightKnockDownEvent>::AddEvent. Because
// mTrafficLightKnockDownQueue is the interface's first member (+0), `mr r3, this` reaches it
// directly. The assert is a plain expression tripwire (this struct is NOT an IOBuffer); its
// rodata 'luInstanceID != 0' has no trailing newline. CGS_ASSERT stamps __FILE__/__LINE__, so the
// X360-baked BrnPropToTrafficInterface.h:149 is not reproduced.
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace PropEntityIO
{
    // X360 0x822CD378 (:117) -- queue a knock-down request for the given traffic-light instance.
    void PropToTrafficInterface::RequestTrafficLightKnockDown(u32 luInstanceID)
    {
        CGS_ASSERT(luInstanceID != 0, "luInstanceID != 0");

        TrafficLightKnockDownEvent lEvent = { luInstanceID };
        mTrafficLightKnockDownQueue.AddEvent(lEvent);
    }
}
}
