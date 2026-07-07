#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.h"

#include <cstdlib>                                                                    // qsort
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                          // CgsDev::StrStream
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"          // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // CgsNetwork::NetworkPlayer (GetRegisteredSendMessage)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"            // CgsNetwork::Message
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h" // GetBandwidthUsedInBits + EAverageType
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
//   MaybeDrawText (X360 0x82824048) -- "draw debug text iff on-screen" -> the renderer's
//     DrawText(text, x, y, scale, colour) gated on a bounds test. The extra register args the
//     decompiler shows are artifacts.
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

//   DebugDraw2DBox (X360 sub_8281C3E0) -- the corner-based filled 2D box primitive: takes two
//     screen-space corners (x1,y1)-(x2,y2) and hands the box {x1,y1}+{x2-x1,y2-y1} to DrawBox.
void DebugDraw2DBox(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfX1, f32 lfY1, f32 lfX2, f32 lfY2,
                    CgsDev::RGBA lColour);

namespace CgsNetwork
{
    namespace
    {
        // Bandwidth -> kbps conversion factors (X360 float literals): a byte count is scaled by
        // 8 bits/byte then by 1/1000 to give kilobits-per-second.
        const f32 KF_BITS_PER_BYTE    = 8.0f;    // flt_82004C88
        const f32 KF_BITS_TO_KILOBITS = 0.001f;  // flt_82013F90

        // DrawBar's first float argument (X360 flt_82001CC0). The value is an un-dumped .rdata
        // float; DrawBar's asm never reads it, so its magnitude is not observable. Reproduced as
        // the baseline 0.0f so the call surface matches the callers' constant.
        const f32 KF_DRAWBAR_BASELINE = 0.0f;    // flt_82001CC0 (UNCERTAIN literal; unused by DrawBar)

        // Debug-menu option labels for the "Measurement Type" enum variable (X360 unk_820E964C,
        // a StringList[2] per DWARF). The .rdata label bytes are not in the available exports, so
        // the display strings are DERIVED from the CompressionAndEncryptionUtils::EAverageType
        // constant names; the value column IS attested (SetRange(0,1) + the enum).
        const CgsDev::DebugUI::StringList KA_AVERAGE_TYPES[2] =
        {
            { CompressionAndEncryptionUtils::E_AVERAGE_TYPE_INSTANT, "Instantaneous" },
            { CompressionAndEncryptionUtils::E_AVERAGE_TYPE_RUNNING, "Running"       },
        };

        // The X360 reads a message-type's display name through the Message vtable slot at byte
        // +0x0C (index 3): `(*(*lpMessage + 0xC))(lpMessage)`. Message models its vtable by hand
        // (mpVTable @ +0x00), so dispatch through it directly here.
        const char* GetMessageTypeName(Message* lpMessage)
        {
            typedef const char* (*NameFn)(Message*);
            void** lppVTable = *reinterpret_cast<void***>(lpMessage);
            return reinterpret_cast<NameFn>(lppVTable[3])(lpMessage);
        }
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

        if (liMaxA <= liMaxB)
        {
            return (liMaxA < liMaxB) ? 1 : 0;
        }
        return -1;
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
                lStream << GetMessageTypeName(lpMessage);
            }
        }
        else
        {
            lStream.Append("ACK / NACKS");
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
    // The per-message-type byte usage the X360 inlines as a direct index into the packeting build's
    // bandwidth ledger (dword_83060940[828*averageType + messageType], i.e. [averageType][send][
    // player 0][messageType]) is reconstructed here as the equivalent call to the homed accessor
    // CompressionAndEncryptionUtils::GetBandwidthUsedInBits -- which carries the same bounds asserts
    // the X360 inlined -- so the inlined assert chain is not duplicated in this body.
    // ================================================================================================
    void PlayerManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        s32 liIndex = 0;

        if (mbDrawBandwidthUsage)
        {
            // Bar 0: the CompAndEnc application bandwidth = the "all message types" aggregate cell
            // (instant average, send direction, player 0, message-type KE_ALL_MESSAGE_TYPES).
            const s32 liBits = CompressionAndEncryptionUtils::GetBandwidthUsedInBits(
                CompressionAndEncryptionUtils::E_AVERAGE_TYPE_INSTANT,
                CompressionAndEncryptionUtils::E_SEND, 0,
                CompressionAndEncryptionUtils::KE_ALL_MESSAGE_TYPES);
            const s32 liBytes = (liBits + 7) / 8;
            DrawBar(0, "CompAndEnc Application Bandwidth (kpbs)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(liBytes) * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            DrawBar(1, "DirtySock Application Bandwidth (kpbs)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(mpPlayerManager->GetTotalBytesSentToDirtySock())
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            DrawBar(2, "DirtySock Actual Bandwidth (kbps)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(mpPlayerManager->GetTotalBytesSent())
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            // NOTE: the 4th bar's X360 label string is truncated in the available export
            // ("DirtySock Estimated Bandwidth with esti"...); the tail is reconstructed here.
            DrawBar(3, "DirtySock Estimated Bandwidth with estimated overhead (kbps)", KF_DRAWBAR_BASELINE,
                    mfMaxBandwidth, mfMaxBandwidthForGraph,
                    static_cast<f32>(mpPlayerManager->GetTotalBytesSentWithOverhead())
                        * KF_BITS_PER_BYTE * KF_BITS_TO_KILOBITS, lpRender);

            liIndex = 4;
        }

        if (mbShowUsedRegisteredMessages)
        {
            NetworkPlayerID liPlayerID = -1;
            if (mpPlayerManager->GetNextRemotePlayerID(&liPlayerID))
            {
                NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
                CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");

                if (miAverageType != CompressionAndEncryptionUtils::E_AVERAGE_TYPE_INSTANT)
                {
                    // Running-average view: collect each message type's max byte count, sort them
                    // largest-first, then draw the non-empty rows in that order.
                    MessageTypeAndMax laMessages[45];
                    for (s32 liMessageType = 0; liMessageType <= 44; ++liMessageType)
                    {
                        const s32 liTypeBits = CompressionAndEncryptionUtils::GetBandwidthUsedInBits(
                            miAverageType, CompressionAndEncryptionUtils::E_SEND, 0, liMessageType);
                        laMessages[liMessageType].meMessageType = liMessageType;
                        laMessages[liMessageType].miMaxBytes    = (liTypeBits + 7) / 8;
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
                else
                {
                    // Instant view: draw each message type in order, skipping the empty ones.
                    for (s32 liMessageType = 0; liMessageType <= 44; ++liMessageType)
                    {
                        const s32 liTypeBits = CompressionAndEncryptionUtils::GetBandwidthUsedInBits(
                            miAverageType, CompressionAndEncryptionUtils::E_SEND, 0, liMessageType);
                        const s32 liMaxBytes = (liTypeBits + 7) / 8;
                        if (liMaxBytes > 0)
                        {
                            DrawRow(liIndex, liMessageType, liMaxBytes, lpNetworkPlayer, lpRender);
                            ++liIndex;
                        }
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
        const f32 lfPlotWidth   = lpRender->GetVirtualScreenSize().x - 100.0f;
        const f32 lfBarLength   = lfPlotWidth * (lfMaxOnGraph * lfInvMaxGraph);

        // Row geometry.
        const f32 lfIndex   = static_cast<f32>(liIndex);
        const f32 lfLabelY  = (lfIndex * 15.0f) * 2.0f + 50.0f;                        // index*30 + 50
        const f32 lfRowY    = ((lfIndex + 1.0f) * 15.0f + lfIndex * 15.0f) + 50.0f;    // index*30 + 65
        const f32 lfBottomY = ((lfIndex + 1.0f) * 15.0f) * 2.0f + 50.0f;              // index*30 + 80

        // 1) The row label, top line.
        MaybeDrawText(lpRender, lpcName, 50.0f, lfLabelY, 15.0f, KU_TEXT_COLOUR, false);

        // 2) The filled bandwidth bar, from the 50px left margin to the scaled length.
        DebugDraw2DBox(lpRender, 50.0f, lfRowY, lfBarLength + 50.0f, lfBottomY, lBarColour);

        // 3) The numeric value (kbps), streamed and drawn under the label.
        char              lacValue[128];
        CgsDev::StrStream lValueStream(lacValue, sizeof(lacValue));
        lValueStream << lfMaxOnGraph << "\n";
        MaybeDrawText(lpRender, lValueStream.GetBuffer(), 50.0f, lfRowY, 15.0f, KU_TEXT_COLOUR, false);

        // 4) The max-bandwidth threshold marker: a vertical line at lfMax's scaled position (no +50).
        const f32     lfMaxLength   = lfPlotWidth * (lfMax * lfInvMaxGraph);
        const Vector2 lv2LineTop    = { lfMaxLength, lfBottomY, 0.0f, 0.0f };
        const Vector2 lv2LineBottom = { lfMaxLength, lfRowY,    0.0f, 0.0f };
        lpRender->DrawLine(lv2LineTop, lv2LineBottom, 0xFFFF0000u);
    }
}
