#pragma once
// BrnWorld::PropEntityIO::PropToTrafficInterface (DWARF BrnPropToTrafficInterface.h:102) --
// the prop->traffic hand-off interface: two event queues the prop module fills to ask the
// traffic system to knock down / restore traffic lights. NOT an IOBuffer (plain struct).
//
// ⭐ RE-HOMED 2026-08-12 (prop-boot wave, agent B8). The two element structs
// (TrafficLightKnockDownEvent / TrafficLightRestoreEvent) used to live in
// BrnPropEntityModuleIO.h and this header included that one to reach them. That direction is
// now REVERSED: the DWARF homes both events in THIS file (BrnPropToTrafficInterface.h, the
// same file that homes the interface holding their queues), and reversing the edge is what
// lets BrnPropEntityModuleIO.h hold OutputBuffer_PrePhysics::mPropToTrafficInterface BY VALUE
// as the real PropToTrafficInterface instead of a 1-byte opaque span. Nothing is duplicated:
// the events have exactly one definition and BrnPropEntityModuleIO.h now includes this header,
// so every existing includer still sees them.
//
// (The DWARF names the events' single member muInstanceID with a Construct/GetInstanceID pair;
//  the committed tree spells it muPayload and several EventQueue/BaseEventQueue TUs already
//  reference that spelling, so it is kept -- a later slice may reconcile the naming.)
#include "types.hpp"                                     // u32
#include "GameShared/GameClasses/Module/CgsEventQueue.h" // CgsModule::EventQueue

namespace BrnWorld
{
namespace PropEntityIO
{
    // ========================================================================
    // BrnWorld::PropEntityIO::TrafficLightKnockDownEvent -- a "this traffic light has been
    // knocked down" notification queued by the prop entity module's pre-physics input/output
    // buffers (the producers OutputBuffer_PrePhysics::Construct / BrnTrafficIO::
    // InputBuffer_PrePhysics::Construct build EventQueue<TrafficLightKnockDownEvent,32>).
    //
    // SIZE (X360, authoritative): sizeof == 4. Pinned by
    // BaseEventQueue<TrafficLightKnockDownEvent>::AddEvent @ 0x822C8D78, which copies the event
    // with `slwi r11,miLength,2; stwx r10,r11,mpEvents` -- a single 4-byte store at stride 4
    // (no two-half split, no index scaling beyond *4). EventQueue<...,32>::Construct
    // @ 0x822E4CE0 places the inline maEvents[32] at +0xC (base mpEvents@0, miMaxLength@4,
    // miLength@8, maEvents@12 -- no alignment pad, confirming 4-byte element alignment) with
    // miMaxLength = 32 (0x20).
    //
    // FLAG (opaque interior): the 4-byte payload's internal field layout is not recovered by
    // this slice -- every observed body (the queue Construct/AddEvent/Append) treats the
    // element only as a 4-byte blob (single stwx / 4*count XMemCpy). Modelled as exactly one
    // 4-byte word so the asm-attested stride and the +0xC inline-buffer offset are exact; the
    // interior (likely a traffic-light/junction id or packed index) is honestly opaque.
    struct TrafficLightKnockDownEvent
    {
        u32 muPayload;   // +0x00  4-byte payload (interior opaque -- see FLAG)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::TrafficLightRestoreEvent -- the paired "restore this knocked-down
    // traffic light" notification, queued alongside TrafficLightKnockDownEvent by the same
    // producers (EventQueue<TrafficLightRestoreEvent,80>).
    //
    // SIZE (X360, authoritative): sizeof == 4. Pinned by
    // BaseEventQueue<TrafficLightRestoreEvent>::AddEvent (@ 0x822C8F.., `slwi r11,miLength,2;
    // stwx r10,r11,mpEvents`) and ::Append (XMemCpy with `slwi count,miLength,2` == 4*count) --
    // a single 4-byte element at stride 4. EventQueue<...,80>::Construct @ 0x822E4D50 places
    // the inline maEvents[80] at +0xC (base mpEvents@0, miMaxLength@4, miLength@8, maEvents@12 --
    // no alignment pad, confirming 4-byte element alignment) with miMaxLength = 80 (0x50).
    //
    // FLAG (opaque interior): as TrafficLightKnockDownEvent above -- the 4-byte payload's
    // internal field layout is not recovered by this slice; modelled as one 4-byte word so the
    // asm-attested stride and +0xC inline-buffer offset are exact.
    struct TrafficLightRestoreEvent
    {
        u32 muPayload;   // +0x00  4-byte payload (interior opaque -- see FLAG)
    };

    // DWARF :102. Members :130/:131 (queue order). Capacities from DWARF: 32 / 80.
    struct PropToTrafficInterface
    {
        void Construct();                                    // :111
        // X360 0x822CD378 (this TU): assert luInstanceID != 0, then AddEvent a knock-down.
        void RequestTrafficLightKnockDown(u32 luInstanceID); // :117
        void RequestTrafficLightRestore(u32 luInstanceID);   // :122
        const CgsModule::EventQueue<TrafficLightKnockDownEvent, 32>* GetTrafficLightKnockDownQueue() const; // :125
        const CgsModule::EventQueue<TrafficLightRestoreEvent, 80>*   GetTrafficLightRestoreQueue()   const; // :126

    private:
        CgsModule::EventQueue<TrafficLightKnockDownEvent, 32> mTrafficLightKnockDownQueue;  // :130 (+0x00)
        CgsModule::EventQueue<TrafficLightRestoreEvent, 80>   mTrafficLightRestoreQueue;     // :131
    };
}
}
