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
    // ------------------------------------------------------------------------
    // PropToTrafficInterface::Construct (DWARF :111)      NEW 2026-08-12 (prop-boot wave, B8)
    // ------------------------------------------------------------------------
    // The X360 emits NO out-of-line body for this: it is a header inline the compiler folds
    // into every owning IO buffer's Construct. Its one attested expansion is
    // BrnWorld::PropEntityIO::OutputBuffer_PrePhysics::Construct @0x822EFCF0, whose tail is
    //     bl BrnWorld::PropEntityIO::TrafficLightKnockDownEvent_32_::Construct  (buffer+11296)
    //     bl BrnWorld::PropEntityIO::TrafficLightRestoreEvent_80_::Construct    (buffer+11436)
    // -- i.e. the two embedded queues, in member order, and nothing else (11436-11296 == 140
    // == 12 + 32*4, the exact size of EventQueue<TrafficLightKnockDownEvent,32>, which is what
    // identifies the two calls as this interface's two members rather than buffer members).
    // EventQueue<T,N>::Construct already points mpEvents at the inline maEvents and zeroes
    // miLength, so there is no separate Clear pass here and none is invented.
    void PropToTrafficInterface::Construct()
    {
        mTrafficLightKnockDownQueue.Construct();
        mTrafficLightRestoreQueue.Construct();
    }

    // X360 0x822CD378 (:117) -- queue a knock-down request for the given traffic-light instance.
    void PropToTrafficInterface::RequestTrafficLightKnockDown(u32 luInstanceID)
    {
        CGS_ASSERT(luInstanceID != 0, "luInstanceID != 0");

        TrafficLightKnockDownEvent lEvent = { luInstanceID };
        mTrafficLightKnockDownQueue.AddEvent(lEvent);
    }

    // (:122) -- queue a restore request for the given traffic-light instance. The twin of
    // RequestTrafficLightKnockDown: same 'luInstanceID != 0' tripwire (no trailing newline;
    // BrnPropToTrafficInterface.h:163, inlined at the PropZoneManager::SendTrafficLightRestoreEvents
    // call site), a single 4-byte event materialised on the stack, then AddEvent onto the
    // interface's mTrafficLightRestoreQueue (its 2nd member, +0x8C).
    void PropToTrafficInterface::RequestTrafficLightRestore(u32 luInstanceID)
    {
        CGS_ASSERT(luInstanceID != 0, "luInstanceID != 0");

        TrafficLightRestoreEvent lEvent = { luInstanceID };
        mTrafficLightRestoreQueue.AddEvent(lEvent);
    }
}
}
