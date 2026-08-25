#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress ("GMInvAccept" / "GMStrOffline")
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf (telemetry "%i")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (formatted default-case asserts)
#include "GameSource/GameState/BrnCgsPlayerName.h"                    // CgsNetwork::PlayerName
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"  // BrnGameState::TrainingManager
#include "SharedClasses/Progression/BrnTrainingTypes.h"              // BrnProgression::ETrainingType
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"            // BrnNetwork::...::NetworkInSelectScoreboardEvent

#include <cstring>   // std::memcpy / std::memset (models the Xbox XMemCpy / memcpy block-copies)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGame::BrnGameModule::BridgeGuiToReplay_PostSim      @ 0x823CCA00
//   BrnGame::BrnGameModule::BridgeGuiToSound               @ 0x823C0A58
//   BrnGame::BrnGameModule::BridgeGuiToGameState           @ 0x823DDB78
//     ^^ MOVED OUT (2026-08-25, P1 sim-pause) to GameBridgeGUIToX_GameState.cpp so it can be
//        MOUNTED on its own: this TU's other two members reference six symbols with no home
//        in the linked set (see that file's banner), one of which -- TelemetryData::
//        AddParameter -- has no reconstructed home anywhere. Moved, not copied.
//   BrnGame::BrnGameModule::TranslateGuiEventsToNetworkEvents @ 0x823DEEB8
//
// GUI-output bridge family: each per-frame bridge walks the GUI output buffer's out-event
// queue (VariableEventQueue<18432,16> @ +0x814, via GetGuiOutEventQueue) and re-publishes /
// translates the queued GUI events into a downstream subsystem's INPUT buffer.
//
// The two event-translating members (BridgeGuiToGameState -- now next door -- and
// TranslateGuiEventsToNetworkEvents)
// drain the GUI queue and, per recognised GUI event type, build a downstream event into a
// correctly-sized RAW local buffer (the exact stack span the X360 writes) and AddEvent it with
// the attested integer event-type TAG + byte size. This mirrors the committed
// GameBridgeNetworkToX.cpp translators. Event-type tags + sizes are asm-authoritative; the
// downstream event payload layouts are opaque (no field names invented). One-byte "signal"
// events carry an uninitialised payload byte, exactly as the X360 passes a pointer to
// uninitialised stack.

// ---------------------------------------------------------------------------
// FLAG (by-name, un-homed collaborators). Declared here so this TU compiles/links against the
// real X360 symbols; their homes are the not-yet-reconstructed GameState / Network module-IO TUs.
// ---------------------------------------------------------------------------
// [P1 sim-pause] PostWorldInput's declaration moved to its single canonical home
// (BrnGameStateModule.h, included via BrnGameModule.hpp above); the PC body lives in
// GameStateModule_gUI_00.cpp and returns the carry queue (the named reduction there).

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // X360 BrnNetwork::BrnNetworkModuleIO::TelemetryData::AddParameter -- appends one
        // (SPrintf'd) parameter string to the telemetry record the case-286 GUI event builds.
        // The record layout + the AddParameter body are owned by the network telemetry TU;
        // modelled here as an opaque record with the attested member (no fields invented).
        struct TelemetryData
        {
            void AddParameter(const char* lpcParameter);
        };
    }
}

namespace BrnGame
{
    // @ 0x823CCA00 -- forward replay-bound GUI events to the replay module's post-sim GUI
    // event queue (ids 349 / 525..533 / 594), and register the case-595 serialiser with the
    // replay request interface.
    void BrnGameModule::BridgeGuiToReplay_PostSim(
        BrnReplays::ReplayIO::InputBuffer_PostSim* lpReplayModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpReplayModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   "lpReplayModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            switch (liType)
            {
                case 349: case 525: case 526: case 527: case 528: case 529:
                case 530: case 531: case 532: case 533: case 594:
                {
                    BrnReplays::ReplayIO::InputBuffer_PostSim::GuiEventQueue* lpDestQueue =
                        lpReplayModuleInputBuffer->GetGuiEventQueue();
                    lpDestQueue->AddEvent(lpEvent, liType, liSize);
                    break;
                }
                case 595:
                {
                    BrnReplays::BaseSerialiser* lpSerialiser =
                        *reinterpret_cast<BrnReplays::BaseSerialiser* const*>(lpEvent);
                    BrnReplays::ReplayIO::RequestInterface* lpRequestInterface =
                        lpReplayModuleInputBuffer->GetRequestInterface();
                    lpRequestInterface->RegisterSerialiser(lpSerialiser);
                    break;
                }
                default: break;
            }
            liType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // @ 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
    // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
    void BrnGameModule::BridgeGuiToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpSoundModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   "lpSoundModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

        lpSoundModuleInputBuffer->SetGuiEventQueue(
            reinterpret_cast<const BrnSound::Module::Io::RootInputBuffer::GuiEventQueue*>(
                lpGuiEventQueue));
    }

    // =========================================================================
    // TranslateGuiEventsToNetworkEvents  (X360 0x823DEEB8)
    //
    // Drain a GUI event queue (VariableEventQueue<18432,16>) and translate each network-bound
    // GUI event into the matching network IN-event, AddEvent'd into the network module's in-event
    // queue (VariableEventQueue<14000,16>). Returns the final queue-walk status. Called by
    // DoUpdate_NetworkPostSim. (The X360 method receives + uses only the two queue pointers; the
    // implicit `this` is unreferenced.)
    // =========================================================================
    int BrnGameModule::TranslateGuiEventsToNetworkEvents(
        CgsModule::VariableEventQueue<14000, 16>* lpNetworkInputQueue,
        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        int liResult = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            const unsigned char* lp = reinterpret_cast<const unsigned char*>(lpEvent);
            const s32* lpW = reinterpret_cast<const s32*>(lp);

            alignas(16) unsigned char lBuf[64];
            unsigned char* lpOut = lBuf;
            s32 liType = 0;
            s32 liSize = 0;
            bool lbEmit = false;

            switch (liResult)
            {
                case 120:   // gamercard
                    CGS_ASSERT(lp != 0, "lpGuiGamercardEvent");
                    std::memcpy(lpOut, lp, 16);
                    liType = 47; liSize = 16; lbEmit = true;
                    break;

                case 49:
                    liType = 27; liSize = 1; lbEmit = true;    // signal
                    break;
                case 80:
                    liType = 28; liSize = 1; lbEmit = true;    // signal
                    break;
                case 94:
                    if (lp[0] == 0)
                        break;
                    reinterpret_cast<s32*>(lpOut)[0] = 45;
                    lpOut[4] = 0;
                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                case 97:
                    liType = 1; liSize = 1; lbEmit = true;     // signal
                    break;
                case 98:
                    liType = 2; liSize = 1; lbEmit = true;     // signal
                    break;
                case 99:
                    CGS_ASSERT(lp != 0, "lpGuiProfileEvent");
                    std::memcpy(lpOut, lp, 16);
                    liType = 13; liSize = 16; lbEmit = true;
                    break;
                case 100:   // invite action sub-switch on src word 0
                    switch (lpW[0])
                    {
                        case 0:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 8; liSize = 16; lbEmit = true;
                            break;
                        case 1:
                            std::memcpy(lpOut, lp + 4, 16);
                            lpOut[16] = lp[20];
                            liType = 9; liSize = 17; lbEmit = true;
                            break;
                        case 2:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 10; liSize = 16; lbEmit = true;
                            break;
                        case 3:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 11; liSize = 16; lbEmit = true;
                            break;
                        case 4:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 12; liSize = 16; lbEmit = true;
                            break;
                        case 5:
                            reinterpret_cast<CgsNetwork::PlayerName*>(lpOut)->Construct("");
                            liType = 8; liSize = 16; lbEmit = true;
                            break;
                        case 6:
                            liType = 14; liSize = 1; lbEmit = true;   // signal
                            break;
                        default:
                        {
                            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStream << "Invalid invite action : " << lpW[0] << "\n";
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert(lStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                            break;
                        }
                    }
                    break;
                case 110:   // select-scoreboard: fixed selector 1
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 1;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                case 111:   // select indexes (category = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetIndexes(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[0] = lSel.miCategory;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 112:   // select variations (index = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetVariations(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[1] = lSel.miIndex;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 113:   // show scoreboard (variation = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetScoreboard(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[2] = lSel.miVariation;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 114:   // select-scoreboard: fixed selector 5
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 5;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                case 115:   // select-scoreboard: fixed selector 6
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 6;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;

                case 121:   // req score target (36-byte record)
                    CGS_ASSERT(lp != 0, "lpGuiReqScoreTargetEvent");
                    std::memcpy(lpOut, lp + 16, 16);
                    reinterpret_cast<s32*>(lpOut)[4] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[5] = lpW[1];
                    reinterpret_cast<s32*>(lpOut)[6] = lpW[2];
                    reinterpret_cast<s32*>(lpOut)[7] = lpW[3];
                    lpOut[32] = lp[32];
                    liType = 50; liSize = 36; lbEmit = true;
                    break;
                case 124:
                    liType = 51; liSize = 1; lbEmit = true;    // signal
                    break;
                case 126:
                    lpOut[0] = lp[0]; lpOut[1] = lp[1]; lpOut[2] = lp[2];
                    liType = 52; liSize = 3; lbEmit = true;
                    break;
                case 270:
                    liType = 21; liSize = 1; lbEmit = true;    // signal
                    break;
                case 278:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 23; liSize = 4; lbEmit = true;
                    break;
                case 281:
                    liType = 25; liSize = 1; lbEmit = true;    // signal
                    break;

                case 286:   // telemetry (two "%i" parameters)
                {
                    reinterpret_cast<s32*>(lpOut)[0] = 44;
                    lpOut[4] = 0;
                    BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry =
                        reinterpret_cast<BrnNetwork::BrnNetworkModuleIO::TelemetryData*>(lpOut);

                    char lacParam0[16];
                    CgsCore::SPrintf(lacParam0, 16, "%i", lpW[1]);
                    lpTelemetry->AddParameter(lacParam0);

                    char lacParam1[16];
                    CgsCore::SPrintf(lacParam1, 16, "%i", lpW[0]);
                    lpTelemetry->AddParameter(lacParam1);

                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                }

                case 287:
                    liType = 6; liSize = 1; lbEmit = true;     // signal
                    break;
                case 293:
                    liType = 26; liSize = 1; lbEmit = true;    // signal
                    break;
                case 323:
                    std::memcpy(lpOut, lp, 20);
                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                case 353:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 24; liSize = 4; lbEmit = true;
                    break;

                case 474:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 17; liSize = 4; lbEmit = true;
                    break;
                case 568:
                    CGS_ASSERT(lp != 0, "lpReqCompCamPicEvent");
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[1] = lpW[1];
                    reinterpret_cast<s32*>(lpOut)[2] = lpW[2];
                    liType = 53; liSize = 12; lbEmit = true;
                    break;

                default:
                    break;
            }

            if (lbEmit)
                lpNetworkInputQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lpOut), liType, liSize);

            liResult = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        return liResult;
    }
}
