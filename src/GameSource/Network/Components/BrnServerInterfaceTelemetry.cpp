#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"

#include "GameSource/Network/BrnNetworkModuleIO.h"                 // BrnNetworkModuleIO::PostSim / PostSimulationInputBuffer / NetworkEventQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<14000,16> / CgsModule::Event

// ===========================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceTelemetry::Prepare  @ 0x82583650
//   BrnNetwork::BrnServerInterfaceTelemetry::Release  @ 0x825836C8
//   BrnNetwork::BrnServerInterfaceTelemetry::Update   @ 0x8258EF90
//
// The Burnout telemetry server-interface leaf. Prepare/Release wrap the shared
// CgsNetwork::ServerInterfaceTelemetry base lifecycle with the game's own country-block
// list, buffer caps and event-id -> keys mapping table; Update drains the inbound post-sim
// network-event queue and hands each telemetry event to the base CaptureEvent recorder.
//
// Every branch / store / early-out below is taken store-for-store from the X360 asm.
//
// maEventDataKeys (DWARF BrnServerInterfaceTelemetry.cpp:30 "extern EventDataKeys[50]
// maEventDataKeys") is the file-scope event-id -> telemetry-keys mapping table: a writable
// static of 50 CgsNetwork::EventDataKeys records (24 bytes each == 1200 bytes == the 0x4B0
// XMemSet count in Release). Its address is the single symbol both Prepare (passed to
// SetEventMappingTable) and Release (zeroed via XMemSet) reference. The only recoverable
// bytes of its static initializer are the first eight, which the X360 rodata renders as the
// ASCII "UNKWUNKW" -- i.e. record[0].luModuleID == record[0].luGroupID == the 'UNKW'
// ("unknown") fourcc. The remaining initializer bytes (record[0].macDescription and
// records[1..49]) live in a data segment that is NOT present in the function-keyed exports,
// so they are un-recovered and honestly zero-filled here. This is SAFE for the three
// functions in this TU (none of them read the table's contents -- Prepare only passes its
// address, Release only zeroes it, Update never touches it); only the base
// SetEventMappingTable consumer (its own TU) reads the records. FLAGGED as a known
// uncertainty: the populated key rows are not recoverable in this scope.
// ===========================================================================

namespace BrnNetwork
{

namespace
{
    // XMemSet -- the platform memset the X360 tail-calls in Release (declared locally exactly
    // as the committed CgsServerInterfaceGames sibling does; homed in the platform TU).
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);

    // The 'UNKW' ("unknown") fourcc that spells the recoverable "UNKWUNKW" prefix of the
    // mapping table's static initializer (record[0].luModuleID / .luGroupID).
    const u32 KU_UNKNOWN_KEY = 0x554E4B57u;   // 'U','N','K','W'

    // The Burnout telemetry-disabled country list Prepare hands to the base (X360 rodata,
    // verbatim). The base KAC_TELEMETRY_DISABLED_COUNTRIES(char[128]) lives in its own TU.
    const char* const KPC_TELEMETRY_DISABLED_COUNTRIES =
        "AD,AT,BE,CY,DK,EU,FI,FR,FX,DE,GR,HU,IS,IE,IT,LI,LT,LU,NL,NO,PL,PT,SM,SK,SI,ES,SE,CH,VA";

    // The first-usage / normal-usage telemetry buffer caps Prepare passes (X360 li r7,0xFA /
    // li r8,0x64). DWARF CgsServerInterfaceTelemetry.h:69/70 names these KI_FIRST_USAGE_BUFFER_SIZE
    // (250) and KI_NORMAL_USAGE_BUFFER_SIZE (100).
    const s32 KI_FIRST_USAGE_BUFFER_SIZE  = 250;
    const s32 KI_NORMAL_USAGE_BUFFER_SIZE = 100;

    // 0x4B0 -- sizeof(EventDataKeys) * 50 == 24 * 50; the Release XMemSet count.
    const u32 KU_EVENT_DATA_KEYS_TABLE_BYTES = 50u * sizeof(CgsNetwork::EventDataKeys);

    // The inbound network-event type Update captures (X360 `cmpwi r3, 0x10`). GetFirstEvent /
    // GetNextEvent return the event's type; only type 16 is forwarded to CaptureEvent.
    const s32 KI_TELEMETRY_NETEVENT_TYPE = 16;

    // BrnNetworkModuleIO::PostSim / the PostSim accessor return the queue against an incomplete
    // forward (BrnNetworkModuleIO::NetworkEventQueue); the concrete queue is the 14000/16
    // variable-event queue (same reinterpret pattern the committed EventScoresManager uses).
    inline const CgsModule::VariableEventQueue<14000, 16>* AsConcreteQueue(
        const BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
    {
        return reinterpret_cast<const CgsModule::VariableEventQueue<14000, 16>*>(lpQueue);
    }
}

// The file-scope event-id -> telemetry-keys mapping table (see the TU note above). Only
// record[0]'s two id words are recoverable ("UNKWUNKW"); the rest is un-recovered and zeroed.
CgsNetwork::EventDataKeys maEventDataKeys[50] =
{
    { KU_UNKNOWN_KEY, KU_UNKNOWN_KEY, "" },
    // records [1..49]: un-recovered static initializer (data segment absent from the exports).
};

// ---------------------------------------------------------------------------
// Prepare @ 0x82583650 -- run the base telemetry Prepare (country-block list + buffer caps);
// on success latch the game's event-id -> keys mapping table, then report success.
bool BrnServerInterfaceTelemetry::Prepare(CgsNetwork::ServerInterfaceDirtySock* lpServerInterface,
                                          bool lbConnectImmediately)
{
    if (!CgsNetwork::ServerInterfaceTelemetry::Prepare(lpServerInterface, lbConnectImmediately,
                                                       KPC_TELEMETRY_DISABLED_COUNTRIES,
                                                       KI_FIRST_USAGE_BUFFER_SIZE,
                                                       KI_NORMAL_USAGE_BUFFER_SIZE))
    {
        return false;
    }

    SetEventMappingTable(maEventDataKeys);
    return true;
}

// ---------------------------------------------------------------------------
// Release @ 0x825836C8 -- run the base telemetry Release; on success wipe the whole mapping
// table (1200 bytes) back to zero, then report success.
bool BrnServerInterfaceTelemetry::Release()
{
    if (!CgsNetwork::ServerInterfaceTelemetry::Release())
        return false;

    XMemSet(maEventDataKeys, 0, KU_EVENT_DATA_KEYS_TABLE_BYTES);
    return true;
}

// ---------------------------------------------------------------------------
// Update @ 0x8258EF90 -- pump the base telemetry feed, then walk the inbound post-sim
// network-event queue: every event of type 16 is captured (its first word is the event id,
// the bytes after it are the payload). The queue pointer is re-fetched from the buffer on
// each step exactly as the X360 does (PostSim is a pure accessor, so this is behaviour-neutral).
void BrnServerInterfaceTelemetry::Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
{
    CgsNetwork::ServerInterfaceTelemetry::Update();

    const CgsModule::Event* lpEventOut = nullptr;   // v8 -- the queue's out-pointer slot
    s32 liSize = 0;                                 // v9 -- size out (X360 over-sizes this stack slot)
    s32 leEventType = AsConcreteQueue(BrnNetworkModuleIO::PostSim(lpInput))
                          ->GetFirstEvent(&lpEventOut, &liSize);

    for (const CgsModule::Event* lpCurrent = lpEventOut; lpEventOut != nullptr; lpCurrent = lpEventOut)
    {
        if (leEventType == KI_TELEMETRY_NETEVENT_TYPE)
        {
            const u8* lpBytes = reinterpret_cast<const u8*>(lpCurrent);
            CaptureEvent(*reinterpret_cast<const s32*>(lpBytes),
                         reinterpret_cast<const char*>(lpBytes + 4));
        }

        leEventType = AsConcreteQueue(BrnNetworkModuleIO::PostSim(lpInput))
                          ->GetNextEvent(lpCurrent, &lpEventOut, &liSize);
    }
}

// ---------------------------------------------------------------------------
// ~BrnServerInterfaceTelemetry -- polymorphic teardown slot. The X360 deleting destructor
// restores this leaf's component vtable at this+0 and conditionally `delete this`; emitting the
// destructor as virtual makes the compiler synthesise exactly that thunk. No owned heap members
// (the table is a file-scope static), so the body is empty -- mirrors the committed
// BrnServerInterfaceDownloadableConfig / BrnServerInterfaceCustomCommands destructor pattern.
BrnServerInterfaceTelemetry::~BrnServerInterfaceTelemetry()
{
}

// The default constructor is trivial (the base Construct chain nulls the component members and
// the mapping table is a file-scope static); provided so the leaf is instantiable.
BrnServerInterfaceTelemetry::BrnServerInterfaceTelemetry()
{
}

}  // namespace BrnNetwork
