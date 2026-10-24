// Hosted regression fixture. The runner generates menu23_extracted.inc from the
// selected production revision; the case arm and helpers in that include are not
// hand-copied here.
#include <cstdio>
#include <cstring>
#include <string>

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"

#include "menu23_extracted.inc"

static int gAssertionCount = 0;

namespace
{
    int gChecks = 0;
    int gFailures = 0;

    void Check(bool lbCondition, const std::string& racLabel)
    {
        ++gChecks;
        if (!lbCondition)
        {
            ++gFailures;
            std::printf("FAIL %s\n", racLabel.c_str());
        }
    }

    using GuiQueue = CgsModule::VariableEventQueue<18432, 16>;
    using SoundQueue = CgsModule::VariableEventQueue<8192, 16>;
    using MusicMessage = CgsSound::Io::Message<BrnSound::MusicOnMenuStreamData>;

    static_assert(sizeof(Menu23Fixture::PcMenuStreamRecord) == 20,
                  "the retained PC GUI-23 record is a 12-byte header plus 8-byte payload");
    static_assert(sizeof(Menu23Fixture::MusicOnMenuStreamEvent) == 20,
                  "BootLegal's actual GUI-155 compatibility record is 20 bytes");
    static_assert(sizeof(BrnSound::MusicOnMenuStreamData) == 8,
                  "sound message 13 carries the actual 8-byte payload");
    static_assert(sizeof(CgsSound::Io::MessageHeader) == 16,
                  "sound message destination fields occupy the actual 16-byte header");

    struct ObservedMessage
    {
        bool mbQueueRecordValid;
        u32  muHash;
        u8   muFromVideo;
        u8   muOverCustom;
        u8   muReserved0;
        u8   muReserved1;
    };

    ObservedMessage ReadMessage(SoundQueue& arQueue, const std::string& racScenario)
    {
        ObservedMessage lObserved = {};
        const CgsModule::Event* lpMessageEvent = 0;
        s32 liMessageSize = 0;
        const s32 liMessageType = arQueue.GetFirstEvent(&lpMessageEvent, &liMessageSize);
        Check(liMessageType == 13, racScenario + ": queued message type is 13");
        Check(lpMessageEvent != 0, racScenario + ": message queue contains one record");
        Check(liMessageSize == static_cast<s32>(sizeof(MusicMessage)),
              racScenario + ": queued record uses actual Message<MusicOnMenuStreamData> size");

        if (lpMessageEvent && liMessageSize >= static_cast<s32>(sizeof(MusicMessage)))
        {
            MusicMessage lMessage = {};
            std::memcpy(&lMessage, lpMessageEvent, sizeof(lMessage));
            lObserved.mbQueueRecordValid = true;
            lObserved.muHash = lMessage.mData.muStreamNameHash;
            lObserved.muFromVideo = lMessage.mData.mbFromVideo;
            lObserved.muOverCustom = lMessage.mData.mbPlayOverCustomSoundtrack;
            lObserved.muReserved0 = lMessage.mData.maReserved[0];
            lObserved.muReserved1 = lMessage.mData.maReserved[1];

            Check(lMessage.GetEventId() == 13, racScenario + ": MessageHeader event id is 13");
            Check(lMessage.GetStateManagerId() == 0,
                  racScenario + ": destination state manager is 0");
            Check(lMessage.GetInstanceId() == 0, racScenario + ": destination instance is 0");
            Check(lMessage.GetEffectId() == 2, racScenario + ": destination effect is 2");
            Check(lMessage.GetEffectType() == CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
                  racScenario + ": effect type is OBJECT");
            std::printf("OBSERVED %s type=%d size=%d hash=%08X from=%u over=%u "
                        "reserved=%u,%u destination=%u,%u,%u effectType=%u\n",
                racScenario.c_str(), static_cast<int>(liMessageType),
                static_cast<int>(liMessageSize),
                static_cast<unsigned>(lObserved.muHash),
                static_cast<unsigned>(lObserved.muFromVideo),
                static_cast<unsigned>(lObserved.muOverCustom),
                static_cast<unsigned>(lObserved.muReserved0),
                static_cast<unsigned>(lObserved.muReserved1),
                static_cast<unsigned>(lMessage.GetStateManagerId()),
                static_cast<unsigned>(lMessage.GetInstanceId()),
                static_cast<unsigned>(lMessage.GetEffectId()),
                static_cast<unsigned>(lMessage.GetEffectType()));
        }

        if (lpMessageEvent)
        {
            const CgsModule::Event* lpNextEvent = 0;
            s32 liNextSize = 0;
            const s32 liNextType = arQueue.GetNextEvent(lpMessageEvent,
                                                        &lpNextEvent, &liNextSize);
            Check(liNextType == -1 && lpNextEvent == 0 && liNextSize == 0,
                  racScenario + ": case 23 queues exactly one sound message");
        }
        else
        {
            Check(false, racScenario + ": case 23 queues exactly one sound message");
        }
        return lObserved;
    }

    void CheckMessage(SoundQueue& arQueue, const std::string& racScenario,
                      u32 luHash, u8 luFromVideo, u8 luOverCustom)
    {
        const ObservedMessage lObserved = ReadMessage(arQueue, racScenario);
        if (!lObserved.mbQueueRecordValid)
            return;
        Check(lObserved.muHash == luHash, racScenario + ": stream hash payload");
        Check(lObserved.muFromVideo == luFromVideo, racScenario + ": from-video payload flag");
        Check(lObserved.muOverCustom == luOverCustom,
              racScenario + ": over-custom-soundtrack payload flag");
        Check(lObserved.muReserved0 == 0 && lObserved.muReserved1 == 0,
              racScenario + ": reserved payload bytes are zero");
    }

    bool QueueGuiBody(GuiQueue& arGuiQueue, SoundQueue& arSoundQueue,
                      const CgsModule::Event* apBody,
                      s32 liType, s32 liSize, const std::string& racScenario)
    {
        const bool lbAdded = arGuiQueue.AddEvent(apBody, liType, liSize);
        Check(lbAdded, racScenario + ": actual GUI queue accepts the body");
        if (!lbAdded)
            return false;

        const CgsModule::Event* lpEvent = 0;
        s32 liQueuedSize = 0;
        const s32 liQueuedType = arGuiQueue.GetFirstEvent(&lpEvent, &liQueuedSize);
        Check(liQueuedType == liType, racScenario + ": actual GUI queue preserves event type");
        Check(liQueuedSize == liSize, racScenario + ": actual GUI queue preserves body size");
        Check(lpEvent != 0, racScenario + ": actual GUI queue returns event bytes");
        if (lpEvent)
            BrnSound::Module::ProcessMenu23Case(arSoundQueue, liQueuedType,
                                                lpEvent, liQueuedSize);

        if (lpEvent)
        {
            const CgsModule::Event* lpNext = 0;
            s32 liNextSize = 0;
            const s32 liNextType = arGuiQueue.GetNextEvent(lpEvent, &lpNext, &liNextSize);
            Check(liNextType == -1 && lpNext == 0 && liNextSize == 0,
                  racScenario + ": input queue contains exactly one event");
        }
        return lpEvent != 0;
    }

    void RunQueuedScenario(GuiQueue& arGuiQueue, SoundQueue& arSoundQueue,
                           const char* lpacShape, u32 luHash, u8 luFlagA, u8 luFlagB)
    {
        char lacScenario[96];
        std::snprintf(lacScenario, sizeof(lacScenario), "%s flags=%u%u", lpacShape,
                      static_cast<unsigned>(luFlagA), static_cast<unsigned>(luFlagB));
        const std::string lScenario(lacScenario);
        arGuiQueue.Clear();
        arSoundQueue.Clear();

        bool lbQueued = false;
        if (std::strcmp(lpacShape, "raw8") == 0)
        {
            u8 lau8Body[8] = {};
            std::memcpy(lau8Body, &luHash, sizeof(luHash));
            lau8Body[4] = luFlagA;
            lau8Body[5] = luFlagB;
            lbQueued = QueueGuiBody(arGuiQueue, arSoundQueue,
                reinterpret_cast<const CgsModule::Event*>(lau8Body), 23, 8, lScenario);
        }
        else if (std::strcmp(lpacShape, "full23") == 0)
        {
            Menu23Fixture::PcMenuStreamRecord lRecord(luHash, luFlagA != 0, luFlagB != 0);
            lbQueued = QueueGuiBody(arGuiQueue, arSoundQueue,
                static_cast<const CgsModule::Event*>(&lRecord), 23,
                static_cast<s32>(sizeof(lRecord)), lScenario);
        }
        else
        {
            Menu23Fixture::MusicOnMenuStreamEvent lRecord(static_cast<s32>(luHash));
            lRecord.mu8FlagA = luFlagA;
            lRecord.mu8FlagB = luFlagB;
            lbQueued = QueueGuiBody(arGuiQueue, arSoundQueue,
                static_cast<const CgsModule::Event*>(&lRecord), 23,
                static_cast<s32>(sizeof(lRecord)), lScenario);
        }

        if (lbQueued)
            CheckMessage(arSoundQueue, lScenario, luHash, luFlagA ? 1 : 0, luFlagB ? 1 : 0);
    }

#if defined(MENU23_EXPECT_OLD_RAW_MISMATCH)
    void RunOldRawNegativeControl(SoundQueue& arSoundQueue, u32 luHash,
                                  u8 luFlagA, u8 luFlagB)
    {
        char lacScenario[96];
        std::snprintf(lacScenario, sizeof(lacScenario), "B595 raw8 flags=%u%u",
                      static_cast<unsigned>(luFlagA), static_cast<unsigned>(luFlagB));
        const std::string lScenario(lacScenario);
        arSoundQueue.Clear();

        // The logical event is 8 bytes. Keep the old consumer's legacy +12/+16/+17
        // reads inside a real type-23 object and use valid bool representations there.
        // The declared size remains 8, so GetGuiPayload follows the raw-body path.
        Menu23Fixture::PcMenuStreamRecord lPaddedCarrier(luHash, false, false);
        u8 lau8Raw[8] = {};
        std::memcpy(lau8Raw, &luHash, sizeof(luHash));
        lau8Raw[4] = luFlagA;
        lau8Raw[5] = luFlagB;
        std::memcpy(&lPaddedCarrier, lau8Raw, sizeof(lau8Raw));
        lPaddedCarrier.muStreamNameHash = 0xDEADBEEFu;
        lPaddedCarrier.mbFlagA = true;
        lPaddedCarrier.mbFlagB = false;

        BrnSound::Module::ProcessMenu23Case(arSoundQueue, 23,
            static_cast<const CgsModule::Event*>(&lPaddedCarrier), 8);
        const ObservedMessage lObserved = ReadMessage(arSoundQueue, lScenario);
        if (lObserved.mbQueueRecordValid)
        {
            Check(lObserved.muHash == 0xDEADBEEFu && lObserved.muFromVideo == 1 &&
                  lObserved.muOverCustom == 0,
                  lScenario + ": old consumer reads the padded legacy offsets deterministically");
            Check(lObserved.muHash != luHash || lObserved.muFromVideo != (luFlagA ? 1 : 0) ||
                  lObserved.muOverCustom != (luFlagB ? 1 : 0),
                  lScenario + ": B595 fails the raw-8 numeric expectation (required negative control)");
            std::printf("NEGATIVE_CONTROL_RAW8_MISMATCH flags=%u%u expected=%08X/%u/%u got=%08X/%u/%u\n",
                static_cast<unsigned>(luFlagA), static_cast<unsigned>(luFlagB),
                static_cast<unsigned>(luHash), static_cast<unsigned>(luFlagA),
                static_cast<unsigned>(luFlagB), static_cast<unsigned>(lObserved.muHash),
                static_cast<unsigned>(lObserved.muFromVideo),
                static_cast<unsigned>(lObserved.muOverCustom));
        }
    }
#endif
}

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { ++gAssertionCount; return 0; }
    int FireAssert(const char*, const char*, int) { ++gAssertionCount; return 0; }
    void* EndAssert() { return 0; }
}
namespace Log { StrStreamBase* gpDebugPrint = 0; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

int main()
{
    GuiQueue lGuiQueue;
    SoundQueue lSoundQueue;
    lGuiQueue.Construct();
    lSoundQueue.Construct();

    const u32 luHash = 0xA1B2C3D4u;
    for (u8 luFlagA = 0; luFlagA <= 1; ++luFlagA)
    {
        for (u8 luFlagB = 0; luFlagB <= 1; ++luFlagB)
        {
#if defined(MENU23_EXPECT_OLD_RAW_MISMATCH)
            RunOldRawNegativeControl(lSoundQueue, luHash, luFlagA, luFlagB);
#else
            RunQueuedScenario(lGuiQueue, lSoundQueue, "raw8",
                              luHash, luFlagA, luFlagB);
#endif
            RunQueuedScenario(lGuiQueue, lSoundQueue, "full23",
                              luHash, luFlagA, luFlagB);
            RunQueuedScenario(lGuiQueue, lSoundQueue, "translated155",
                              luHash, luFlagA, luFlagB);
        }
    }

    Check(gAssertionCount == 0, "no production queue assertion fired");
#if defined(MENU23_EXPECT_OLD_RAW_MISMATCH)
    std::printf("Menu23OldControl: %d checks, %d failures\n", gChecks, gFailures);
#else
    std::printf("Menu23Candidate: %d checks, %d failures\n", gChecks, gFailures);
#endif
    return gFailures ? 1 : 0;
}
