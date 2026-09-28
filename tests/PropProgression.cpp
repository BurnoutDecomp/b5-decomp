// L4 (owner's list 2026-09-27/28) -- "The already broken smashes/billboards are not visually broken".
//
// The console hands the profile's hit-prop bits back to the prop world in a handshake inside
// GameStateModule::ProcessGameEvents @0x823A0A18:
//   OnProfileLoaded @0x82397310 (case 8 at the boot, case 109 on an in-game load) posts action 194 (size 1);
//   case 112 (E_EVENT_REQUEST_PROP_PROGRESSION)   -> `*(this+292288) = 1` (mbPropSystemNeedsProgression);
//   the tail (LABEL_648)                          -> if the flag is 1: post action 199 carrying gsm+107224 ==
//                                                    &profile.mabHitPropBitArray (size 4), clear the flag.
// This fixture compiles the PRODUCTION arm (GameStateModule::ProcessGameEventsPropProgressionBringUp, extracted by
// run_prop_progression.py into propprog_body.inc) with the PRODUCTION action records (propprog_records.inc) against
// minimal stand-ins for the queue, the module (OnProfileLoaded records its calls) and the profile, and checks what
// the arm does for 109, for 112, for a flag raised earlier, and for neither.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef float    f32;

namespace CgsDev { namespace Log {
    struct Stream
    {
        Stream& operator<<(const char*) { return *this; }
        Stream& operator<<(u32)         { return *this; }
    };
    static Stream* gpDebugPrint = 0;   // the diag lines stay off: this checks behaviour, not the witness
} }

namespace CgsContainers
{
    template <u32 tuNumBits>
    class BitArray
    {
    public:
        bool IsBitSet(u32 luBit) const { return (mau64[luBit >> 6] >> (luBit & 63u)) & 1u; }
        void SetBit(u32 luBit)         { mau64[luBit >> 6] |= u64(1) << (luBit & 63u); }
    private:
        u64 mau64[(tuNumBits + 63u) / 64u];
    };
}

namespace CgsModule
{
    struct Event { u8 mau8[1]; };

    // A queue of (type, size, payload) records, walked the way the production arms walk the merged queue.
    template <s32 BUFSIZE, s32 ALIGN>
    class VariableEventQueue
    {
    public:
        struct Record { s32 miType; s32 miSize; u8 mau8Payload[64]; };
        Record maRecords[16];
        s32    miCount;

        bool AddEvent(const Event* lpEvent, s32 liType, s32 liSize)
        {
            Record& lr = maRecords[miCount++];
            lr.miType = liType;
            lr.miSize = liSize;
            std::memcpy(lr.mau8Payload, lpEvent, static_cast<size_t>(liSize));
            return true;
        }
        s32 GetFirstEvent(const Event** lppEvent, s32* lpiSize) const
        {
            if (miCount == 0) { *lppEvent = 0; return 0; }
            *lppEvent = reinterpret_cast<const Event*>(maRecords[0].mau8Payload);
            *lpiSize  = maRecords[0].miSize;
            return maRecords[0].miType;
        }
        s32 GetNextEvent(const Event* lpEvent, const Event** lppNextEvent, s32* lpiSize) const
        {
            for (s32 li = 0; li < miCount; ++li)
            {
                if (reinterpret_cast<const Event*>(maRecords[li].mau8Payload) == lpEvent && li + 1 < miCount)
                {
                    *lppNextEvent = reinterpret_cast<const Event*>(maRecords[li + 1].mau8Payload);
                    *lpiSize      = maRecords[li + 1].miSize;
                    return maRecords[li + 1].miType;
                }
            }
            *lppNextEvent = 0;
            return 0;
        }
    };
}

namespace BrnProgression
{
    class Profile
    {
    public:
        typedef CgsContainers::BitArray<300000u> HitPropsBitArray;
        const HitPropsBitArray& GetHitProps() const { return mabHitPropBitArray; }
        HitPropsBitArray mabHitPropBitArray;
    };
    class ProgressionManager
    {
    public:
        Profile* GetProfile() { return &mProfile; }
        Profile mProfile;
    };
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueue;
    enum EGameEventType
    {
#include "propprog_event_ids.inc"      // the production E_EVENT_PROGRESSION_PROFILE_LOADED / _REQUEST_PROP_PROGRESSION
    };
    enum EGameActionType
    {
#include "propprog_action_ids.inc"     // the production E_ACTION_LOAD_PROFILE / _PROP_SMASH_PROGRESSION
    };
    template <EGameActionType T>
    struct GameAction { };
#include "propprog_records.inc"        // the production LoadProfileAction / PropSmashReportAction
    struct OutputBuffer { u8 mau8[1]; };
}

    class GameStateModule
    {
    public:
        void ProcessGameEventsPropProgressionBringUp(const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
                                                     GameStateModuleIO::GameActionQueue* lpActionQueue);
        // Stand-in for OnProfileLoaded @0x82397310 (bodied and tested in run_profile_delivery.py): record the call.
        void OnProfileLoaded(GameStateModuleIO::OutputBuffer* lpOutput, GameStateModuleIO::GameActionQueue* lpQueue)
        {
            ++miOnProfileLoadedCalls;
            mpLastOutput = lpOutput;
            mpLastQueue  = lpQueue;
        }
        bool                               mbPropSystemNeedsProgression = false;
        BrnProgression::ProgressionManager mProgressionManager;
        GameStateModuleIO::OutputBuffer    mOutputBuffer;
        GameStateModuleIO::OutputBuffer*   mpOutputBuffer = &mOutputBuffer;
        s32                                miOnProfileLoadedCalls = 0;
        GameStateModuleIO::OutputBuffer*   mpLastOutput = 0;
        GameStateModuleIO::GameActionQueue* mpLastQueue = 0;
    };

#include "propprog_body.inc"           // the production arm
}

using namespace BrnGameState;

static u32 guChecks   = 0;
static u32 guFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s\n", lbPassed ? "PASS" : "FAIL", lpcLabel);
}

typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

int main()
{
    static GameStateModule sModule;                       // static: the profile's 37.5 KB bit array
    sModule.mProgressionManager.mProfile.mabHitPropBitArray.SetBit(600 * 9 + 1);    // the owner's first hit prop
    sModule.mProgressionManager.mProfile.mabHitPropBitArray.SetBit(600 * 145 + 12);
    const u8 lau8Empty[8] = { 0 };

    // ---- 1. case 109: a profile finished loading in game -> OnProfileLoaded(out, queue) ----------------------------
    {
        GameEventQueue lEvents = {};
        lEvents.AddEvent(reinterpret_cast<const CgsModule::Event*>(lau8Empty),
                         GameStateModuleIO::E_EVENT_PROGRESSION_PROFILE_LOADED, 4);
        GameStateModuleIO::GameActionQueue lActions = {};
        sModule.ProcessGameEventsPropProgressionBringUp(&lEvents, &lActions);
        Check(sModule.miOnProfileLoadedCalls == 1 && sModule.mpLastOutput == sModule.mpOutputBuffer
              && sModule.mpLastQueue == &lActions && lActions.miCount == 0,
              "event 109 (profile loaded) calls OnProfileLoaded once with the output buffer and the arm's queue "
              "(0x823A33D4: r4 = r27, r5 = r22) -- action 194 is OnProfileLoaded's own post");
        Check(!sModule.mbPropSystemNeedsProgression, "event 109 does not raise the progression flag");
    }

    // ---- 2. case 112 + the tail: the world asks -> action 199 carrying &profile.mabHitPropBitArray ------------
    {
        GameEventQueue lEvents = {};
        lEvents.AddEvent(reinterpret_cast<const CgsModule::Event*>(lau8Empty),
                         GameStateModuleIO::E_EVENT_REQUEST_PROP_PROGRESSION, 1);
        GameStateModuleIO::GameActionQueue lActions = {};
        sModule.ProcessGameEventsPropProgressionBringUp(&lEvents, &lActions);
        Check(lActions.miCount == 1 && lActions.maRecords[0].miType == 199,
              "event 112 then the tail post action 199 in the same update (case 112 flag, LABEL_648)");
        Check(lActions.miCount == 1 && lActions.maRecords[0].miSize == static_cast<s32>(sizeof(void*)),
              "action 199's record is one pointer (console size 4 == one console pointer)");
        const void* lpPosted = 0;
        if (lActions.miCount == 1)
            std::memcpy(&lpPosted, lActions.maRecords[0].mau8Payload, sizeof(lpPosted));
        Check(lpPosted == &sModule.mProgressionManager.mProfile.mabHitPropBitArray,
              "action 199 points at the profile's own hit-prop bits (gsm+107224 == &mabHitPropBitArray)");
        const BrnProgression::Profile::HitPropsBitArray* lpBits =
            static_cast<const BrnProgression::Profile::HitPropsBitArray*>(lpPosted);
        Check(lpBits != 0 && lpBits->IsBitSet(600 * 9 + 1) && lpBits->IsBitSet(600 * 145 + 12)
              && !lpBits->IsBitSet(600 * 9 + 2),
              "the world reads the profile's hit props through it (zone 9 prop 1, zone 145 prop 12 set; zone 9 prop 2 not)");
        Check(!sModule.mbPropSystemNeedsProgression, "the tail clears mbPropSystemNeedsProgression after posting");
    }

    // ---- 3. the flag alone (set on an earlier update) is answered even with no event this frame --------------
    {
        sModule.mbPropSystemNeedsProgression = true;
        GameEventQueue lEvents = {};
        GameStateModuleIO::GameActionQueue lActions = {};
        sModule.ProcessGameEventsPropProgressionBringUp(&lEvents, &lActions);
        Check(lActions.miCount == 1 && lActions.maRecords[0].miType == 199 && !sModule.mbPropSystemNeedsProgression,
              "the tail runs after the walk whatever the queue holds: a raised flag posts 199 and clears");
    }

    // ---- 4. nothing asked, nothing loaded: no action ------------------------------------------------------------
    {
        GameEventQueue lEvents = {};
        lEvents.AddEvent(reinterpret_cast<const CgsModule::Event*>(lau8Empty), 111, 8);   // a prop hit, another arm's
        GameStateModuleIO::GameActionQueue lActions = {};
        sModule.ProcessGameEventsPropProgressionBringUp(&lEvents, &lActions);
        Check(lActions.miCount == 0, "no 109 / 112 and no raised flag: the arm posts nothing");
    }

    std::printf("PropProgression: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
