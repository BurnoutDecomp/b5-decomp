// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/X360/
//     BrnGameStateAchievementManagerX360.cpp
// ============================================================================
// BrnGameState::AchievementManagerX360 -- the Xbox 360 concrete achievement
// manager (derives from AchievementManagerBase). This TU reconstructs the four
// self-contained functions:
//
//   AchievementEarnt   (X360 0x82367000, base vtable slot 0)
//   IsAchievementEarnt (X360 0x82367240, base vtable slot 1)
//   Prepare            (X360 0x82372DD0)
//   Release            (X360 0x82372E28)
//
// The two achievement bit sets are 50-entry CgsContainers::BitArray. The X360
// build inlines every BitArray probe/mutate at the call site AND emits the
// container's internal bounds asserts here (the committed CgsBitArray.h stays
// assert-free by design -- see its banner); those bounds guards are restored as
// CGS_ASSERT call-site asserts, mirroring the sibling StreamedVaultAllocator /
// SceneSweeper reconstructions. The dynamic streamed "invalid index : <i> < 50" /
// "Index: <i>, Number of bits: 50" messages collapse to the project's static
// bit-array-bound token "luIndex < NUMBITS" (attested verbatim in this same TU's
// WriteAchievements asm, and used identically by StreamedVaultAllocator).
//
// The achievement pump (Update -> GetAchievementsFromX360Api -> WriteAchievements) is here
// too. Its local-bit <-> system achievement-id table KAU_ACHIEVEMENT_IDS was read out of the
// image (50 words, ending where the next rodata string starts).
//
// Original home (from the asm assert strings):
//   ..\GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.cpp
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"     // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // GameActionQueue::AddEvent
#include "GameSource/GameState/BrnGameActions.h"                 // AchievementsEarnedAction / E_ACTION_SEND_TELEMETRY / E_ACTION_NETWORK_CAUGHT_FEVER
#include "GameSource/GameState/BrnGameStateModuleIO.h"           // PreWorldInputBuffer / OutputBuffer
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // TelemetryData / E_TELEMETRY_ACHIEVEMENT_EARNT

// The system-software achievement calls (bodies in the PC platform layer, CgsXboxLivePC_wBT_01.cpp).
extern "C" u32 XUserGetSigninState(u32 luUserIndex);
extern "C" u32 XUserCreateAchievementEnumerator(u32 luTitleId, u32 luUserIndex, u64 luXuid,
                                                u32 luDetailFlags, u32 luStartingIndex, u32 luItems,
                                                u32* lpcbBuffer, void** lphEnum);
extern "C" u32 XEnumerate(void* lhEnum, void* lpvBuffer, u32 lcbBuffer, u32* lpcItemsReturned,
                          void* lpOverlapped);
extern "C" u32 XUserWriteAchievements(u32 luNumAchievements, const void* lpAchievements,
                                      void* lpOverlapped);

// Win32/XDK CloseHandle (real prototype lives in <winbase.h>); declared as an
// extern "C" free function, mirroring the CgsGuideIntegration / System glue
// precedent. mhEnumerator is a raw HANDLE (void*).
extern "C" int CloseHandle(void* lhObject);

namespace BrnGameState
{
namespace
{
    // Local achievement bit -> system achievement id, read from the image (50 entries).
    const u32 KAU_ACHIEVEMENT_IDS[AchievementManagerX360::KU_NUM_ACHIEVEMENTS] =
    {
        10, 11, 12, 13, 14, 15, 16, 17, 19, 20,
        21, 23, 26, 28, 30, 32, 39, 42, 43, 45,
        46, 48, 50, 52, 54, 56, 57, 58, 59, 61,
        62, 63, 64, 66, 67, 68, 69, 72, 74, 75,
        76, 77, 78, 80, 81, 83, 84, 85, 86, 87,
    };

    // The console's achievement numbering for "caught fever" (OnCaughtFever fires 0x31): once
    // the download shows it earnt, the network side is told the player has the fever.
    const EAchievement E_CONSOLE_ACHIEVEMENT_CAUGHT_FEVER = static_cast<EAchievement>(49);

    // The enumerate call's arguments: the running title, every achievement from the first,
    // and the detail-flags word the console passes.
    const u32 KU_ACHIEVEMENT_ENUMERATE_TITLE_ID     = 0;
    const u64 KU_ACHIEVEMENT_ENUMERATE_XUID         = 0;      // the signed-in user's own list
    const u32 KU_ACHIEVEMENT_ENUMERATE_DETAIL_FLAGS = 0x20;
    const u32 KU_ACHIEVEMENT_ENUMERATE_FIRST        = 0;


    // Win32 completion codes the pump compares against.
    const u32 KU_ERROR_SUCCESS    = 0;
    const u32 KU_ERROR_IO_PENDING = 997;

    // The pump states (miState).
    const s32 KI_STATE_IDLE        = 0;
    const s32 KI_STATE_ENUMERATING = 1;
    const s32 KI_STATE_DOWNLOADED  = 2;

    // The user indices the console accepts.
    const s32 KI_MIN_USER_INDEX = 0;
    const s32 KI_MAX_USER_INDEX = 3;
    const s32 KI_NO_USER        = -1;

    // CgsXOverlapped is the 28-byte system overlapped record itself; the system calls take it
    // by address.
    XOVERLAPPED* ToXOverlapped(CgsSystem::CgsXOverlapped* lpOverlapped)
    {
        return reinterpret_cast<XOVERLAPPED*>(lpOverlapped);
    }
}

// ----------------------------------------------------------------------------
// AchievementEarnt  (X360 0x82367000, base vtable slot 0)
//   Queue leAchievement into mAchievementsToWrite the first time it is seen. If it
//   is already flagged as earnt, fire the "written twice" dev assert and DO NOT
//   re-queue it (the X360 returns without touching the queue in that case).
// ----------------------------------------------------------------------------
void AchievementManagerX360::AchievementEarnt(EAchievement leAchievement)
{
    const u32 luIndex = static_cast<u32>(leAchievement);

    // BitArray<50>::IsBitSet's inlined bounds guard (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");

    if (mAchievementsEarnt.IsBitSet(luIndex))
    {
        // X360 line 544 -- streamed "Trying to write achievement index <i> more
        // than once."; collapsed to the project's static string.
        CGS_ASSERT(false, "Trying to write achievement index more than once.");
    }
    else
    {
        // BitArray<50>::SetBit's inlined bounds guard (CgsBitArray.h:222).
        CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
        mAchievementsToWrite.SetBit(luIndex);
    }
}

// ----------------------------------------------------------------------------
// IsAchievementEarnt  (X360 0x82367240, base vtable slot 1)
//   True iff leAchievement's bit is set in EITHER tracking set: mAchievementsEarnt
//   (already flushed to Xbox Live) OR mAchievementsToWrite (queued this frame).
//   Callers use this to avoid double-queuing an id via AchievementEarnt.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::IsAchievementEarnt(EAchievement leAchievement)
{
    const u32 luIndex = static_cast<u32>(leAchievement);

    // BitArray<50>::IsBitSet's inlined bounds guard (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");
    if (mAchievementsEarnt.IsBitSet(luIndex))
    {
        return true;
    }

    // Second probe -- same inlined bounds guard on the write queue (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
    if (mAchievementsToWrite.IsBitSet(luIndex))
    {
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Prepare  (X360 0x82372DD0, called by GameStateModule::Prepare)
//   Reset the manager to its idle pre-enumeration state and clear both bit sets.
//   Returns true.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::Prepare()
{
    miUserIndex   = -1;    // no signed-in user yet
    miState       = 0;     // idle
    mhEnumerator  = 0;     // no achievement enumerator open

    mOverLapped.Construct();

    mAchievementsEarnt.UnSetAll();
    mAchievementsToWrite.UnSetAll();

    return true;
}

// ----------------------------------------------------------------------------
// Release  (X360 0x82372E28, called by GameStateModule::Release)
//   Clear both bit sets, reset the user/state fields, close the achievement
//   enumerator handle if one is open, and re-construct the overlapped. Returns true.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::Release()
{
    mAchievementsEarnt.UnSetAll();
    mAchievementsToWrite.UnSetAll();

    void* lhEnumerator = mhEnumerator;

    miUserIndex = -1;
    miState     = 0;

    if (lhEnumerator)
    {
        CloseHandle(lhEnumerator);
    }
    mhEnumerator = 0;

    mOverLapped.Construct();

    return true;
}

// ----------------------------------------------------------------------------
// Update  (called by GameStateModule::PreWorldUpdate)
//   The active controller's port is the user index. A new index restarts the pump. While that
//   user is signed in and no system call is in flight: download the earnt set (once per user),
//   then write queued achievements.
// ----------------------------------------------------------------------------
void AchievementManagerX360::Update(const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                                    GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpPreWorldInputBuffer, "lpPreWorldInputBuffer");
    CGS_ASSERT(lpPreWorldInputBuffer->GetControllerToGameStateInterface(),
               "lpPreWorldInputBuffer->GetControllerToGameStateInterface()");

    const s32 liActiveControllerPort =
        lpPreWorldInputBuffer->GetControllerToGameStateInterface()->GetActiveControllerPort();
    if (miUserIndex != liActiveControllerPort)
    {
        miUserIndex = liActiveControllerPort;
        miState     = KI_STATE_IDLE;
    }

    const bool lbSignedIn = (miUserIndex != KI_NO_USER) &&
                            (XUserGetSigninState(static_cast<u32>(miUserIndex)) != 0);
    if (!lbSignedIn)
    {
        return;
    }

    CGS_ASSERT(miUserIndex >= KI_MIN_USER_INDEX, "miUserIndex >= 0");
    CGS_ASSERT(miUserIndex <= KI_MAX_USER_INDEX, "miUserIndex <= 3");

    if (mOverLapped.IsOperationInProgress())
    {
        return;
    }

    if (miState == KI_STATE_DOWNLOADED || GetAchievementsFromX360Api(lpOutputBuffer))
    {
        WriteAchievements(lpOutputBuffer);
    }
}

// ----------------------------------------------------------------------------
// GetAchievementsFromX360Api
//   Idle: create the user's achievement enumerator and start reading all 50 details.
//   Enumerating: wait for the read. On failure log it, drop back to idle and close the
//   enumerator. On success rebuild mAchievementsEarnt from every achieved entry (the write
//   queue must be empty by now), close the enumerator, tell the network about the fever
//   achievement, and report true with the pump in the downloaded state.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::GetAchievementsFromX360Api(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    if (miState == KI_STATE_IDLE)
    {
        CGS_ASSERT(miUserIndex >= KI_MIN_USER_INDEX, "miUserIndex >= 0");
        CGS_ASSERT(miUserIndex <= KI_MAX_USER_INDEX, "miUserIndex <= 3");

        if (XUserCreateAchievementEnumerator(KU_ACHIEVEMENT_ENUMERATE_TITLE_ID, static_cast<u32>(miUserIndex),
                                             KU_ACHIEVEMENT_ENUMERATE_XUID,
                                             KU_ACHIEVEMENT_ENUMERATE_DETAIL_FLAGS,
                                             KU_ACHIEVEMENT_ENUMERATE_FIRST, KU_NUM_ACHIEVEMENTS,
                                             &mcbBuffer, &mhEnumerator) != KU_ERROR_SUCCESS)
        {
            CloseHandle(mhEnumerator);
            return false;
        }

        mOverLapped.Construct();
        const u32 luResult = XEnumerate(mhEnumerator, maAchievementDetails, mcbBuffer, 0,
                                        ToXOverlapped(&mOverLapped));
        if (luResult != KU_ERROR_SUCCESS && luResult != KU_ERROR_IO_PENDING)
        {
            // The console streams the failure code after this text.
            CGS_ASSERT(false, "XEnumerate failed for reason: ");
        }
    }
    else if (miState != KI_STATE_ENUMERATING)
    {
        CGS_ASSERT(false, "Achievement manager is confused!");
        CGS_ASSERT(false, "The achievement manager logic is screwed up!");
        return false;
    }

    miState = KI_STATE_ENUMERATING;

    if (mOverLapped.IsOperationInProgress())
    {
        return false;
    }

    if (XGetOverlappedResult(ToXOverlapped(&mOverLapped), 0, 0) != KU_ERROR_SUCCESS)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            const char* lpcResult = CgsSystem::CgsXOverlapped::GetResultString(ToXOverlapped(&mOverLapped));
            *CgsDev::Log::gpDebugPrint << "Failed to download achievements " << lpcResult << "\n";
        }
        miState = KI_STATE_IDLE;
        CloseHandle(mhEnumerator);
        return false;
    }

    CGS_ASSERT(mAchievementsToWrite.GetFirstNonZeroBit() == CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS>::KI_INVALID_BITINDEX,
               "mAchievementsToWrite.GetFirstNonZeroBit() == -1");
    mAchievementsToWrite.UnSetAll();
    mAchievementsEarnt.UnSetAll();

    for (u32 luDetail = 0; luDetail < KU_NUM_ACHIEVEMENTS; ++luDetail)
    {
        const XACHIEVEMENT_DETAILS& lrDetails = maAchievementDetails[luDetail];
        if (lrDetails.dwAchievedLow == 0 && lrDetails.dwAchievedHigh == 0)
        {
            continue;
        }

        s32 liIndex = -1;
        for (u32 luId = 0; luId < KU_NUM_ACHIEVEMENTS; ++luId)
        {
            if (KAU_ACHIEVEMENT_IDS[luId] == lrDetails.dwId)
            {
                liIndex = static_cast<s32>(luId);
                break;
            }
        }
        if (liIndex == -1)
        {
            continue;
        }

        // BitArray<50>::SetBit's inlined bounds guard (CgsBitArray.h).
        CGS_ASSERT(static_cast<u32>(liIndex) < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");
        mAchievementsEarnt.SetBit(static_cast<u32>(liIndex));
    }

    CloseHandle(mhEnumerator);
    mhEnumerator = 0;
    mcbBuffer    = 0;
    mOverLapped.Construct();

    if (IsAchievementEarnt(E_CONSOLE_ACHIEVEMENT_CAUGHT_FEVER))
    {
        // A one-byte action whose payload nobody reads.
        u8 lu8CaughtFever = 0;
        lpOutputBuffer->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lu8CaughtFever),
            GameStateModuleIO::E_ACTION_NETWORK_CAUGHT_FEVER, 1);
    }

    miState = KI_STATE_DOWNLOADED;
    return true;
}

// ----------------------------------------------------------------------------
// WriteAchievements
//   Take queued achievements lowest bit first, at most three: each goes into the next write
//   record (user index + system id), moves from the write queue to the earnt set, and posts
//   its telemetry (the bit, then the new earnt count in hex). If any were taken, post the
//   earnt count and start the system write on the overlapped.
// ----------------------------------------------------------------------------
void AchievementManagerX360::WriteAchievements(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(!mOverLapped.IsOperationInProgress(), "!mOverLapped.IsOperationInProgress()");
    mOverLapped.Construct();

    s32 liNumAchievements = 0;
    for (;;)
    {
        const s32 liIndex = mAchievementsToWrite.GetFirstNonZeroBit();
        if (liIndex == CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS>::KI_INVALID_BITINDEX || liNumAchievements >= static_cast<s32>(KU_MAX_ACHIEVEMENTS_PER_WRITE))
        {
            break;
        }
        const u32 luIndex = static_cast<u32>(liIndex);

        // BitArray<50>::IsBitSet's inlined bounds guard (CgsBitArray.h), twice.
        CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");
        CGS_ASSERT(!mAchievementsEarnt.IsBitSet(luIndex), "Trying to earn an achievement twice");
        CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
        CGS_ASSERT(mAchievementsToWrite.IsBitSet(luIndex), "Trying to write an unachieved event");

        XUSER_ACHIEVEMENT& lrAchievement = maAchievementsToWrite[liNumAchievements];
        lrAchievement.dwUserIndex     = static_cast<u32>(miUserIndex);
        lrAchievement.dwAchievementId = KAU_ACHIEVEMENT_IDS[luIndex];

        // BitArray<50>::SetBit (CgsBitArray.h) and UnSetBit (CgsBitArray.h) guards.
        CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");
        mAchievementsEarnt.SetBit(luIndex);
        CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
        mAchievementsToWrite.UnSetBit(luIndex);

        BrnNetwork::BrnNetworkModuleIO::TelemetryData lTelemetry;
        lTelemetry.Construct(BrnNetwork::E_TELEMETRY_ACHIEVEMENT_EARNT);
        lTelemetry.AddParameter(liIndex);
        lTelemetry.AddParameter(static_cast<CgsID>(mAchievementsEarnt.CountSetBits()));
        lpOutputBuffer->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lTelemetry),
            GameStateModuleIO::E_ACTION_SEND_TELEMETRY, static_cast<s32>(sizeof(lTelemetry)));

        ++liNumAchievements;
    }

    if (liNumAchievements > 0)
    {
        GameStateModuleIO::AchievementsEarnedAction lAchievementsEarned;
        lAchievementsEarned.miAchivementCount = static_cast<s32>(mAchievementsEarnt.CountSetBits());
        lpOutputBuffer->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAchievementsEarned),
            GameStateModuleIO::E_ACTION_ACHIEVEMENTS_EARNED, static_cast<s32>(sizeof(lAchievementsEarned)));

        XUserWriteAchievements(static_cast<u32>(liNumAchievements), maAchievementsToWrite,
                               ToXOverlapped(&mOverLapped));
    }
}

}
