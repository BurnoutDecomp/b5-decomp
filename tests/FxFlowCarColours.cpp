// FX-FLOW (crash parity 2026-09-24, the drive-thru paint-shop stop): the PRODUCTION
// GameStateModule::Prepare stages 11/12 (E_PREPARESTAGE_REQUEST/RECEIVE_PLAYERCARCOLOURS), extracted
// from src/GameSource/GameState/BrnGameStateModule.cpp by run_fxflow_car_colours.py, run against the
// REAL RequestInterface<3072>::AcquireResource, EventReceiverQueue, ResourcePtr / CreateFromHandle and
// ID::HashString bodies, with a fake GameData reply standing in for the pool.
//
// Checked against the ARTIST asm of GameStateModule::Prepare @0x8239E578:
//   case 11 @0x8239E9D4  stw 11 -> the stage word; rec = {mpUser = &mReceiverQueue, miEventId 0,
//                        miPoolId 5 (`li r10,5`), mResourceId = HashString("CarColours")};
//                        RequestInterface<3072> AddEvent(&rec, type 4, 0x18); mReceiverQueue.Clear()
//   case 12 @0x8239EA34  stw 12; `cmpwi len,1 ; blt` -> not done; else CreateFromHandle(
//                        this+284400 (mpPlayerCarColours), firstEvent+0x18) and on to stage 13
//                        (no assert, no Clear of the reply queue)
// DriveThruManager::ProcessDriveThru's paint arm then reads maPalettes[2].miNumColours (+0x20)
// through that pointer (@0x8239BD0C).
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the stage's diagnostic line stays silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The pre-fix stage body's only call.
static void LogPrepareStageOnce(s32, const char*) {}

// Stands in for GameStateModuleIO::OutputBuffer: the stages only reach its request interface.
struct FixtureOutputBuffer
{
    BrnResource::GameDataIO::RequestInterface<3072> mRequests;
    BrnResource::GameDataIO::RequestInterface<3072>* GetResourceRequestInterface() { return &mRequests; }
};

// Stands in for GameStateModule: the members the two stages touch, under their real names.
struct FixtureGameStateModule
{
    enum EPrepareStage
    {
        E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS = 11,
        E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS = 12,
        E_PREPARESTAGE_MODEMANAGER              = 13,
    };
    EPrepareStage                                           mePrepareStage;
    CgsModule::EventReceiverQueue<3072, 16>                 mReceiverQueue;
    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mpPlayerCarColours;

    // true once the pass reaches stage 13 (the stages' "done" edge), false while waiting.
    bool Stages(FixtureOutputBuffer* lpOutputBuffer);
};

// The production stage text (see the runner).
#include "car_colours.inc"

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// A resident "CarColours" resource as the pool hands it out: the reply's mpResourceMemory points at
// the entry's SmallResource slot, whose first word is the main-memory resource (the palette).
static BrnWorld::GlobalColourPalette gPalette;
static void* gaSmallResource[3] = { &gPalette, nullptr, nullptr };

int main()
{
    for (s32 liPalette = 0; liPalette < BrnWorld::E_NUM_PALETTES; ++liPalette)
        gPalette.maPalettes[liPalette].miNumColours = 10 + liPalette;   // palette 2 -> 12 colours

    static FixtureOutputBuffer    lOutput;
    static FixtureGameStateModule lGsm;
    lOutput.mRequests.Construct();
    lGsm.mReceiverQueue.Construct();
    lGsm.mePrepareStage = FixtureGameStateModule::E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS;

    // ---- pass 1: stage 11 posts the request, stage 12 finds no reply and waits -----------------
    const bool lbFirstPassDone = lGsm.Stages(&lOutput);
    const CgsModule::Event* lpRequestEvent = nullptr;
    s32 liRequestSize = 0;
    const s32 liRequestType = (lOutput.mRequests.mRequestQueue.GetLength() > 0)
        ? lOutput.mRequests.mRequestQueue.GetFirstEvent(&lpRequestEvent, &liRequestSize) : -1;
    const CgsResource::Events::AcquireResourceRequest* lpRequest =
        reinterpret_cast<const CgsResource::Events::AcquireResourceRequest*>(lpRequestEvent);

    Check(lOutput.mRequests.mRequestQueue.GetLength() == 1 && liRequestType == 4,
          "case 11: one AcquireResourceRequest (type 4) on the output buffer's request interface");
    Check(lpRequest != nullptr && lpRequest->mpUser == &lGsm.mReceiverQueue,
          "case 11: the reply target is the module's mReceiverQueue (r30 = this+232384)");
    Check(lpRequest != nullptr && lpRequest->miEventId == 0 && lpRequest->miPoolId == 5,
          "case 11: miEventId 0 (r20), miPoolId 5 (`li r10,5`)");
    Check(lpRequest != nullptr &&
          lpRequest->mResourceId.GetHash() == static_cast<u64>(static_cast<u32>(
              CgsResource::ID::HashString(reinterpret_cast<const u8*>("CarColours")))),
          "case 11: mResourceId = ID::HashString(\"CarColours\") (zero-extended CRC, no pool bits)");
    Check(!lbFirstPassDone &&
          lGsm.mePrepareStage == FixtureGameStateModule::E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS &&
          !lGsm.mpPlayerCarColours.HasMemoryResource(),
          "case 12: no reply yet -> the pass stops at stage 12 with nothing bound (`cmpwi len,1 ; blt`)");

    // ---- the pool's reply arrives -----------------------------------------------------------------
    CgsResource::Events::AcquireResourceResponse lReply;
    std::memset(&lReply, 0, sizeof(lReply));
    lReply.mpUser          = &lGsm.mReceiverQueue;
    lReply.miEventId       = 0;
    lReply.miPoolId        = 5;
    lReply.mpResourceMemory = gaSmallResource;
    lReply.mpSourceEntry   = nullptr;
    lGsm.mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReply), 6,
                                 static_cast<s32>(sizeof(lReply)));

    // ---- pass 2: stage 12 binds the palette and moves on to stage 13 ------------------------------
    const bool lbSecondPassDone = lGsm.Stages(&lOutput);
    Check(lbSecondPassDone && lGsm.mePrepareStage == FixtureGameStateModule::E_PREPARESTAGE_MODEMANAGER,
          "case 12: with the reply queued the pass runs on into stage 13");
    Check(lGsm.mpPlayerCarColours.HasMemoryResource() &&
          static_cast<const void*>(lGsm.mpPlayerCarColours.operator->()) == &gPalette,
          "case 12: CreateFromHandle(mpPlayerCarColours, reply+0x18) binds the palette memory");
    Check(lGsm.mpPlayerCarColours.HasMemoryResource() &&
          lGsm.mpPlayerCarColours->maPalettes[2].miNumColours == 12,
          "the paint shop's read (maPalettes[2].miNumColours, +0x20) resolves through the bound pointer");
    Check(lGsm.mReceiverQueue.GetLength() == 1,
          "case 12 does not Clear the reply queue (the console leaves it to the next request)");
    Check(gAsserts == 0, "no assert on the way");

    // ---- a fresh module re-entering stage 12 with an empty queue keeps waiting ---------------------
    {
        static FixtureGameStateModule lWaiting;
        lWaiting.mReceiverQueue.Construct();
        lWaiting.mePrepareStage = FixtureGameStateModule::E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS;
        static FixtureOutputBuffer lQuiet;
        lQuiet.mRequests.Construct();
        const bool lbDone = lWaiting.Stages(&lQuiet);
        Check(!lbDone && !lWaiting.mpPlayerCarColours.HasMemoryResource() &&
              lQuiet.mRequests.mRequestQueue.GetLength() == 0,
              "re-entering case 12 with no reply neither binds nor re-requests");
    }

    std::printf("FxFlowCarColours: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
