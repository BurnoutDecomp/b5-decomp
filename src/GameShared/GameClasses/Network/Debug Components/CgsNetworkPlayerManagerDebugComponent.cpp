#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.h"

#include <cstdlib>                                                                    // qsort
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                          // CgsDev::StrStream
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"          // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // CgsNetwork::NetworkPlayer (GetRegisteredSendMessage)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"            // CgsNetwork::Message
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h" // bandwidth ledger lookup
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender (DrawBar: DrawLine + GetVirtualScreenSize + RGBA/Vector2)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::PlayerManagerDebugComponent::_QSortMessageAndMax  @ 0x8287F370
//   CgsNetwork::PlayerManagerDebugComponent::OnActivate           @ 0x8287F278
//   CgsNetwork::PlayerManagerDebugComponent::DrawRow              @ 0x82893B50
//   CgsNetwork::PlayerManagerDebugComponent::RenderHUD            @ 0x82893C88
//   CgsNetwork::PlayerManagerDebugComponent::DrawBar              @ 0x8288BA28

// The three shared debug-HUD draw helpers DrawBar dispatches to. They live in their own TUs (the
// X360 bl's them directly); declared here so the compile gate sees their shape, mirroring the
// sibling DebugComponent reconstructions.
//
//   Debug2DImmediateRender::DrawText(text, x, y, scale, colour) (X360 0x82824048, which IDA names
//     "MaybeDrawText"): packs (x, y) into a Vector2 and calls the Vector2 overload.

namespace CgsNetwork
{
    const f32 PlayerManagerDebugComponent::KF_DEFAULT_MAX_BANDWIDTH_FOR_GRAPH = 256.0f;
    const f32 PlayerManagerDebugComponent::KF_DEFAULT_MAX_BANDWIDTH = 128.0f;

    namespace
    {
        // Bandwidth -> kbps conversion factors (X360 float literals): a byte count is scaled by
        // 8 bits/byte then by 1/1000 to give kilobits-per-second.
        const f32 KF_BITS_PER_BYTE    = 8.0f;    // flt_82004C88
        const f32 KF_BITS_TO_KILOBITS = 0.001f;  // flt_82013F90

        // DrawBar's first float argument: the rodata word the callers load is 0.0f, and DrawBar
        // never reads it.
        const f32 KF_DRAWBAR_BASELINE = 0.0f;

        // Exact two-entry ARTIST table at 0x820E964C. The console data has no null-name terminator;
        // the registered range is 0..1. Keep the original table extent instead of appending a row.
        const CgsDev::DebugUI::StringList KA_AVERAGE_TYPES[2] =
        {
            { CompressionAndEncryptionUtils::E_AVERAGE_OVER_SECOND, "Average Over Last Second" },
            { CompressionAndEncryptionUtils::E_MAX_OVER_SECOND, "Maximum Over Second"      },
        };
    }

    // qsort comparator: descending order by miMaxBytes (larger first).
    // X360 0x8287F370 -- reads the int32 field at +4 (miMaxBytes) of each MessageTypeAndMax.
    //   lwz r11,4(r3); lwz r10,4(r4); cmpw cr6,r11,r10 (SIGNED)
    //   ble ->L1; else return -1        (r11 > r10  -> -1)
    //   L1: li r3,1; bltlr cr6           (r11 < r10  ->  1)
    //       li r3,0; blr                 (r11 == r10 ->  0)
    int PlayerManagerDebugComponent::_QSortMessageAndMax(const void* lpA, const void* lpB)
    {
        const s32 liMaxA = static_cast<const MessageTypeAndMax*>(lpA)->miMaxBytes;
        const s32 liMaxB = static_cast<const MessageTypeAndMax*>(lpB)->miMaxBytes;

        if (liMaxA > liMaxB)
        {
            return -1;
        }
        if (liMaxA < liMaxB)
        {
            return 1;
        }
        return 0;
    }

    // ================================================================================================
    // OnActivate  @ 0x8287F278
    // ------------------------------------------------------------------------------------------------
    // Register the bandwidth-HUD debug-menu surface: the two on-screen toggles, the graph/absolute
    // max-bandwidth floats (the graph max clamped to [10, 3000] in steps of 10), and the "Measurement
    // Type" enum (instant vs running average), whose option labels come from KA_AVERAGE_TYPES.
    // The X360 sub_8282D800/D6B0/D720/F598 helpers are the RegisterVariable / SetRange overloads.
    // ================================================================================================
    void PlayerManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mbDrawBandwidthUsage, "Draw Bandwidth usage on screen");
        RegisterVariable(&mbShowUsedRegisteredMessages, "Show Used registered messages");

        RegisterVariable(&mfMaxBandwidthForGraph, "Max bandwidth for graph");
        SetRange(&mfMaxBandwidthForGraph, 10.0f, 3000.0f);
        SetStep(&mfMaxBandwidthForGraph, 10.0f);

        RegisterVariable(&mfMaxBandwidth, "Max bandwidth");

        RegisterVariable(&miAverageType, "Measurement Type");
        SetRange(&miAverageType, 0, 1);
        SetOptions(&miAverageType, KA_AVERAGE_TYPES);
    }

    // ================================================================================================
    // DrawRow  @ 0x82893B50
    // ------------------------------------------------------------------------------------------------
    // Build one message-type row's label into a stack StrStream, then forward it (with the row's
    // max byte count converted to kbps) to DrawBar. Message-type 0 is the ACK/NACK aggregate,
    // message-type 44 is the packet headers row, and everything else names its registered send
    // message via the message's vtable name slot.
    // ================================================================================================
    void PlayerManagerDebugComponent::DrawRow(s32 liIndex, s32 liMessageType, s32 liMaxBytes,
                                              NetworkPlayer* lpPlayer,
                                              CgsDev::Debug2DImmediateRender* lpRender)
    {
        char lacBuffer[128];
        CgsDev::StrStream lStream(lacBuffer, sizeof(lacBuffer));

        if (liMessageType != 0)
        {
            if (liMessageType == 44)
            {
                lStream << "HEADERS";
            }
            else
            {
                Message* lpMessage = lpPlayer->GetRegisteredSendMessage(liMessageType);
                lStream << lpMessage->GetName();
            }
        }
        else
        {
            lStream << "ACK / NACKS";
        }

        lStream << " (kbps)\n";

        DrawBar(liIndex, lacBuffer, KF_DRAWBAR_BASELINE, mfMaxBandwidth, mfMaxBandwidthForGraph,
                static_cast<f32>(liMaxBytes) * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);
    }

    // ================================================================================================
    // RenderHUD  @ 0x82893C88
    // ------------------------------------------------------------------------------------------------
    // The network bandwidth overlay. When mbDrawBandwidthUsage is set it draws four aggregate bars
    // (the CompAndEnc application total plus the three DirtySock send tallies). When
    // mbShowUsedRegisteredMessages is set it then lists the first remote player's per-message-type
    // byte usage -- sorted largest-first in running-average mode, or in message-type order in instant
    // mode -- skipping the empty rows.
    //
    // Bandwidth counts use the all-players row. The first remote player provides the message names.
    // ================================================================================================
    void PlayerManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        s32 liIndex = 0;

        if (mbDrawBandwidthUsage)
        {
            // Bar 0: the CompAndEnc application bandwidth = the "all message types" aggregate cell
            // (last-second average, send direction, all players, all message types).
            const s32 liBytes = mCompressionUtils.GetBandwidthUsedInBytes(
                CompressionAndEncryptionUtils::E_AVERAGE_OVER_SECOND,
                CompressionAndEncryptionUtils::E_SEND_BANDWIDTH,
                CompressionAndEncryptionUtils::KI_ALL_PLAYERS,
                CompressionAndEncryptionUtils::KE_ALL_MESSAGE_TYPES);
            DrawBar(0, "CompAndEnc Application Bandwidth (kpbs)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(liBytes) * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            DrawBar(1, "DirtySock Application Bandwidth (kpbs)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(static_cast<s32>(mpPlayerManager->GetTotalBytesSentToDirtySock()))
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            DrawBar(2, "DirtySock Actual Bandwidth (kbps)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(static_cast<s32>(mpPlayerManager->GetTotalBytesSent()))
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            DrawBar(3, "DirtySock Estimated Bandwidth with estimated overhead (kbps)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(static_cast<s32>(mpPlayerManager->GetTotalBytesSentWithOverhead()))
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            liIndex = 4;
        }

        if (mbShowUsedRegisteredMessages)
        {
            NetworkPlayerID liPlayerID = -1;
            if (mpPlayerManager->GetNextRemotePlayerID(&liPlayerID))
            {
                NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
                CGS_ASSERT_AT(lpNetworkPlayer != 0, "lpNetworkPlayer", "..\\..\\..\\GameShared\\GameClasses\\Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.cpp", 246);

                if (miAverageType == CompressionAndEncryptionUtils::E_AVERAGE_OVER_SECOND)
                {
                    // Instant view: draw each message type in order, skipping the empty ones.
                    for (s32 liMessageType = 0; liMessageType <= 44; ++liMessageType)
                    {
                        const s32 liMaxBytes = mCompressionUtils.GetBandwidthUsedInBytes(
                            static_cast<CompressionAndEncryptionUtils::EAverageType>(miAverageType), CompressionAndEncryptionUtils::E_SEND_BANDWIDTH,
                            CompressionAndEncryptionUtils::KI_ALL_PLAYERS, liMessageType);
                        if (liMaxBytes > 0)
                        {
                            DrawRow(liIndex, liMessageType, liMaxBytes, lpNetworkPlayer, lpRender);
                            ++liIndex;
                        }
                    }
                }
                else
                {
                    // Running-average view: collect each message type's max byte count, sort them
                    // largest-first, then draw the non-empty rows in that order.
                    MessageTypeAndMax laMessages[45];
                    for (s32 liMessageType = 0; liMessageType <= 44; ++liMessageType)
                    {
                        laMessages[liMessageType].meMessageType = liMessageType;
                        laMessages[liMessageType].miMaxBytes = mCompressionUtils.GetBandwidthUsedInBytes(
                            static_cast<CompressionAndEncryptionUtils::EAverageType>(miAverageType), CompressionAndEncryptionUtils::E_SEND_BANDWIDTH,
                            CompressionAndEncryptionUtils::KI_ALL_PLAYERS, liMessageType);
                    }

                    qsort(laMessages, 44, sizeof(MessageTypeAndMax), _QSortMessageAndMax);

                    for (s32 liRow = 0; liRow <= 44; ++liRow)
                    {
                        if (laMessages[liRow].miMaxBytes <= 0)
                        {
                            break;
                        }
                        DrawRow(liIndex, laMessages[liRow].meMessageType, laMessages[liRow].miMaxBytes,
                                lpNetworkPlayer, lpRender);
                        ++liIndex;
                    }
                }
            }
        }
    }

    // ================================================================================================
    // DrawBar  @ 0x8288BA28
    // ------------------------------------------------------------------------------------------------
    // Draw one labelled bandwidth bar for HUD row liIndex: the row label (top line), a filled bar whose
    // length is the value's fraction of the graph's full scale, the numeric value beneath the label,
    // and a vertical marker line at the max-bandwidth threshold.
    //
    // Rows are 30px tall (label on the row's top line at y=index*30+50, the value + bar on the line
    // 15px below at y=index*30+65..80). The plot area is the virtual-screen width less a 100px inset;
    // a value of lfMaxOnGraph maps to (screenWidth-100) * (lfMaxOnGraph / lfX) pixels from the 50px left
    // margin. The bar is green while under lfMax and red once the value meets/exceeds it. The threshold
    // marker line is placed at lfMax's scaled position (X360: no left-margin offset on the line's x).
    //
    // lfValue (the X360's first float arg) is never read by the body -- it is the callers' baseline
    // constant, kept only for the call surface.
    // ================================================================================================
    void PlayerManagerDebugComponent::DrawBar(s32 liIndex, const char* lpcName, f32 /*lfValue*/,
                                              f32 lfMax, f32 lfX, f32 lfMaxOnGraph,
                                              CgsDev::Debug2DImmediateRender* lpRender)
    {
        const CgsDev::RGBA KU_TEXT_COLOUR = 0xFFFFFFFFu;   // white (X360 r26 = -1)

        // Bar fill: green under the max-bandwidth threshold, red once value >= lfMax.
        // (X360: 0xFF000000 | (lfMaxOnGraph >= lfMax ? 0x0000FF : 0x008000).)
        const CgsDev::RGBA lBarColour = (lfMaxOnGraph >= lfMax) ? 0xFF0000FFu : 0xFF008000u;

        // Horizontal scale: 1 / graph-full-scale; the plot area is the virtual screen width less a
        // 100px inset. (X360 reads mfVirtualScreenWidth @0x34 directly; reached here by name.)
        const f32 lfInvMaxGraph = 1.0f / lfX;
        const f32 lfPlotWidth   = lpRender->GetVirtualScreenSize().mV.x - 100.0f;
        const f32 lfBarLength   = lfPlotWidth * (lfMaxOnGraph * lfInvMaxGraph);

        // Row geometry.
        const f32 lfIndex   = static_cast<f32>(liIndex);
        const f32 lfLabelY  = (lfIndex * 15.0f) * 2.0f + 50.0f;                        // index*30 + 50
        const f32 lfRowY    = ((lfIndex + 1.0f) * 15.0f + lfIndex * 15.0f) + 50.0f;    // index*30 + 65
        const f32 lfBottomY = ((lfIndex + 1.0f) * 15.0f) * 2.0f + 50.0f;              // index*30 + 80

        // 1) The row label, top line.
        lpRender->DrawText(lpcName, 50.0f, lfLabelY, 15.0f, KU_TEXT_COLOUR);

        // 2) The filled bandwidth bar, from the 50px left margin to the scaled length.
        lpRender->DrawBox(50.0f, lfRowY, lfBarLength + 50.0f, lfBottomY, lBarColour);

        // 3) The numeric value (kbps), streamed and drawn under the label.
        char              lacValue[128];
        CgsDev::StrStream lValueStream(lacValue, sizeof(lacValue));
        lValueStream << lfMaxOnGraph << "\n";
        lpRender->DrawText(lValueStream.GetBuffer(), 50.0f, lfRowY, 15.0f, KU_TEXT_COLOUR);

        // 4) The max-bandwidth threshold marker: a vertical line at lfMax's scaled position (no +50).
        const f32     lfMaxLength   = lfPlotWidth * (lfMax * lfInvMaxGraph);
        const Vector2 lv2LineTop(lfMaxLength, lfBottomY);
        const Vector2 lv2LineBottom(lfMaxLength, lfRowY);
        lpRender->DrawLine(lv2LineTop, lv2LineBottom, 0xFFFF0000u);
    }
}
