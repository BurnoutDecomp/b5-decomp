#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"

#include "GameSource/Network/BrnNetworkModuleIO.h"          // PostSimulationInputBuffer / NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"   // NetworkInTelemetryEvent

// ===========================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceTelemetry::Prepare
//   BrnNetwork::BrnServerInterfaceTelemetry::Release
//   BrnNetwork::BrnServerInterfaceTelemetry::Update
//
// The game's telemetry component. Prepare/Release wrap the DirtySock
// CgsNetwork::ServerInterfaceTelemetry lifecycle with the game's country-block list,
// buffer caps and event-id -> keys mapping table; Update drains the inbound post-sim
// network-event queue and hands each telemetry event to the base CaptureEvent.
//
// maEventDataKeys is the class-static event-id -> telemetry-keys table (50 records of
// 24 bytes == the 0x4B0 bytes Release zeroes). Its initialiser is dumped from the
// image's writable data: each record's module / group fourcc pair; every description
// is empty.
// ===========================================================================

// Platform memset (declared exactly as the other network TUs declare it).
extern "C"
{
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);
}

namespace BrnNetwork
{

namespace
{
    // The country list the base blocks telemetry for (rodata, verbatim).
    const char* const KPC_TELEMETRY_DISABLED_COUNTRIES =
        "AD,AT,BE,CY,DK,EU,FI,FR,FX,DE,GR,HU,IS,IE,IT,LI,LT,LU,NL,NO,PL,PT,SM,SK,SI,ES,SE,CH,VA";

    // First-usage / normal-usage telemetry buffer sizes Prepare passes (250 / 100).
    const s32 KI_FIRST_USAGE_BUFFER_SIZE  = 250;
    const s32 KI_NORMAL_USAGE_BUFFER_SIZE = 100;
}

CgsNetwork::EventDataKeys BrnServerInterfaceTelemetry::maEventDataKeys[KI_NUM_EVENT_DATA_KEYS] =
{
    { 0x554E4B57u, 0x554E4B57u, "" },   // UNKW UNKW
    { 0x47414D45u, 0x45565354u, "" },   // GAME EVST
    { 0x47414D45u, 0x45564F4Eu, "" },   // GAME EVON
    { 0x47414D45u, 0x4556464Eu, "" },   // GAME EVFN
    { 0x47414D45u, 0x45565154u, "" },   // GAME EVQT
    { 0x47414D45u, 0x45564443u, "" },   // GAME EVDC
    { 0x43525348u, 0x57524C44u, "" },   // CRSH WRLD
    { 0x43525348u, 0x54524146u, "" },   // CRSH TRAF
    { 0x43525348u, 0x52414352u, "" },   // CRSH RACR
    { 0x43525348u, 0x53545348u, "" },   // CRSH STSH
    { 0x544B444Eu, 0x544B444Eu, "" },   // TKDN TKDN
    { 0x544B444Eu, 0x54445654u, "" },   // TKDN TDVT
    { 0x544B444Eu, 0x54444154u, "" },   // TKDN TDAT
    { 0x544B444Eu, 0x54445442u, "" },   // TKDN TDTB
    { 0x544B444Eu, 0x4D41524Bu, "" },   // TKDN MARK
    { 0x4E455457u, 0x52564144u, "" },   // NETW RVAD
    { 0x4E455457u, 0x5256524Du, "" },   // NETW RVRM
    { 0x47414D45u, 0x4D534750u, "" },   // GAME MSGP
    { 0x47414D45u, 0x4D535048u, "" },   // GAME MSPH
    { 0x524F5255u, 0x5042544Du, "" },   // RORU PBTM
    { 0x524F5255u, 0x50424352u, "" },   // RORU PBCR
    { 0x47414D45u, 0x4341524Du, "" },   // GAME CARM
    { 0x47414D45u, 0x4C494345u, "" },   // GAME LICE
    { 0x4E455457u, 0x434F4E4Eu, "" },   // NETW CONN
    { 0x4E455457u, 0x474D4352u, "" },   // NETW GMCR
    { 0x4E455457u, 0x474D4A4Eu, "" },   // NETW GMJN
    { 0x4E455457u, 0x474D5354u, "" },   // NETW GMST
    { 0x4E455457u, 0x43485354u, "" },   // NETW CHST
    { 0x4E455457u, 0x4348464Eu, "" },   // NETW CHFN
    { 0x4E455457u, 0x4348434Eu, "" },   // NETW CHCN
    { 0x4F4E474Du, 0x4E554D50u, "" },   // ONGM NUMP
    { 0x4F4E474Du, 0x54524944u, "" },   // ONGM TRID
    { 0x4F4E474Du, 0x4E554D52u, "" },   // ONGM NUMR
    { 0x4F4E474Du, 0x524E4B44u, "" },   // ONGM RNKD
    { 0x47414D45u, 0x464E544Du, "" },   // GAME FNTM
    { 0x47414D45u, 0x544D4F54u, "" },   // GAME TMOT
    { 0x47414D45u, 0x44545053u, "" },   // GAME DTPS
    { 0x47414D45u, 0x44544253u, "" },   // GAME DTBS
    { 0x47414D45u, 0x44544A59u, "" },   // GAME DTJY
    { 0x47414D45u, 0x44544357u, "" },   // GAME DTCW
    { 0x47414D45u, 0x44544150u, "" },   // GAME DTAP
    { 0x47414D45u, 0x44544753u, "" },   // GAME DTGS
    { 0x4E455457u, 0x4E455753u, "" },   // NETW NEWS
    { 0x4E455457u, 0x4C445242u, "" },   // NETW LDRB
    { 0x4E455457u, 0x43555354u, "" },   // NETW CUST
    { 0x4E455457u, 0x45535944u, "" },   // NETW ESYD
    { 0x4E455457u, 0x474D4C54u, "" },   // NETW GMLT
    { 0x47414D45u, 0x41434856u, "" },   // GAME ACHV
    { 0x4453434Bu, 0x55504E50u, "" },   // DSCK UPNP
    { 0x4453434Bu, 0x47434F4Eu, "" },   // DSCK GCON
};

BrnServerInterfaceTelemetry::BrnServerInterfaceTelemetry()
{
}

// The deleting destructor only restores the component vtable and conditionally frees.
BrnServerInterfaceTelemetry::~BrnServerInterfaceTelemetry()
{
}

// Run the base Prepare with the country-block list and buffer caps; on success latch the
// mapping table.
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

// Run the base Release; on success wipe the whole mapping table.
bool BrnServerInterfaceTelemetry::Release()
{
    if (!CgsNetwork::ServerInterfaceTelemetry::Release())
    {
        return false;
    }

    XMemSet(maEventDataKeys, 0, sizeof(maEventDataKeys));
    return true;
}

// Pump the base telemetry feed, then walk the inbound post-sim network events and capture
// every telemetry event. The queue is re-fetched from the buffer for each step.
void BrnServerInterfaceTelemetry::Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
{
    CgsNetwork::ServerInterfaceTelemetry::Update();

    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 leEventType = lpInput->GetNetworkEventQueue()->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != nullptr)
    {
        if (leEventType == BrnNetworkModuleIO::NetworkInTelemetryEvent::KI_EVENT_TYPE)
        {
            const BrnNetworkModuleIO::NetworkInTelemetryEvent* lpTelemetryEvent =
                reinterpret_cast<const BrnNetworkModuleIO::NetworkInTelemetryEvent*>(lpEvent);
            CaptureEvent(lpTelemetryEvent->mEventData.meHook, lpTelemetryEvent->mEventData.macBuffer);
        }

        leEventType = lpInput->GetNetworkEventQueue()->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

}  // namespace BrnNetwork
