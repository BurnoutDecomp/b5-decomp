#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"             // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

namespace BrnGameState
{
// X360 0x823565C0. Initialise the online car-select manager to its idle defaults: store the two
// owning pointers, clear the internal state, null the list pointers, clear the online flag, zero both
// spawn vectors (the X360 emits two stvx128 register stores at +32 / +48), zero the four car-id slots
// (the asm `li r11,0; std r11,0x40/0x48/0x50/0x58` proves 0), and clear the streaming flag + change-car
// state. The assert uses the literal file/line the build baked in (raw Begin/Fire/End rather than
// CGS_ASSERT, which would inject __FILE__/__LINE__). mfTimeLeftInCarSelect (+24) and miVehicleClassLimit
// are deliberately left uninitialised -- no store at those offsets in the X360 code.
void OnlineCarSelectManager::Construct(GameStateModule* lpGameStateModule,
                                       BrnProgression::ProgressionManager* lpProgressionManager)
{
    if (!lpGameStateModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpGameStateModule",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\CarSelect/BrnOnlineCarSelectManager.h",
            252);
        CgsDev::Assert::EndAssert();
    }

    meInternalState = E_INTERNAL_STATE_NONE;
    mpGameStateModule.Set(lpGameStateModule);
    mpProgressionManager.Set(lpProgressionManager);
    mpVehicleList.Set(nullptr);
    mpWheelList.Set(nullptr);
    mbIsInOnlineCarSelect = false;

    mSpawnPosition.SetZero();
    mSpawnDirection.SetZero();

    mStartCarId             = 0;
    mFreeburnCarId          = 0;
    mDesiredCarId           = 0;
    mCacheDuringChangeCarId = 0;

    mbWaitingForStreaming = false;
    meStateOfChangingCars = E_CAR_CHANGE_NONE;
}

// X360 0x8238EEA0. Enter the car-modification flow; only valid from the internal car-select state
// (asserts otherwise, with the verbatim baked file/line BrnOnlineCarSelectManager.h:340), then
// delegates to the modification state set-up helper. The X360 builds the assert message into the
// StrStream buffer; since the text is a plain string literal it is passed straight to FireAssert
// (the committed BrnCarSelectManager.cpp precedent).
void OnlineCarSelectManager::EnterModification(InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meInternalState != E_INTERNAL_STATE_CAR_SELECT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "OnlineCarSelectManager: Need to be in E_INTERNAL_STATE_CAR_SELECT state.",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\CarSelect/BrnOnlineCarSelectManager.h",
            340);
        CgsDev::Assert::EndAssert();
    }

    StartCarModificationState(lpActionQueue);
}

// X360 0x82356650. Transition into the wait-for-host-to-choose state; only valid from the internal
// car-select state (asserts otherwise, verbatim baked file/line BrnOnlineCarSelectManager.h:354).
// The action-queue parameter is part of the DWARF shape but unused by the body (the X360 Hex-Rays
// elided it), so it is left unnamed-but-present.
void OnlineCarSelectManager::EnterWaitForHost(InputBuffer::GameActionQueue* /*lpActionQueue*/)
{
    if (meInternalState != E_INTERNAL_STATE_CAR_SELECT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "OnlineCarSelectManager: Need to be in E_INTERNAL_STATE_CAR_SELECT state.",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\CarSelect/BrnOnlineCarSelectManager.h",
            354);
        CgsDev::Assert::EndAssert();
    }

    meInternalState = E_INTERNAL_STATE_WAIT_FOR_HOST_TO_CHOOSE;
}

// X360 0x82358AC8. Streaming-complete callback: when the car that finished streaming is the one we
// were waiting on (mDesiredCarId), emit the filter-gated debug line and clear the wait flag. The
// X360 does a full 64-bit cmpld of mDesiredCarId (+80) against the incoming id; modelled here as a
// logical CgsID equality on the named member. The debug spew is gated on
// CgsDev::Message::gxMessageFilterFlags bit 0 and streamed through the committed
// CgsDev::Log::gpDebugPrint (string-only operator<< chain).
void OnlineCarSelectManager::StreamingFinished(CgsID lActiveCarZeroId,
                                               InputBuffer::GameActionQueue* /*lpActionQueue*/)
{
    if (mDesiredCarId == lActiveCarZeroId)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "StreamingFinished\n" << "\n";
        }
        mbWaitingForStreaming = false;
    }
}
}
