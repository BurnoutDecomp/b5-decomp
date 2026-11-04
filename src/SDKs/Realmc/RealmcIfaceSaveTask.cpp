#include "SDKs/Realmc/RealmcIfaceSaveTask.h"

#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcMemcardState.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"
#include "SDKs/Realmc/RealmcTrc.h"
#include "SDKs/Realmc/RealmcIfaceMessageSaveDone.h"
#include "SDKs/Realmc/RealmcCoreMessageClear.h"

#include <new>

namespace RealmcIface
{

namespace
{

void SendMessageToGame(RealmcCore::IRunnableTask& rTask, RealmcCore::Message* pMessage)
{
    const RealmcCore::MessagePtr lMessage(pMessage);
    const RealmcCore::ResponsePtr lResponse = rTask.SendMessage(lMessage, 0);
}

int SendTrcToGame(RealmcCore::IRunnableTask& rTask, const RealmcCore::Trc& rTrc,
                  int iValue, u32* lpuSentTime = 0)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(RealmcCore::MessageTrc));
    RealmcCore::MessageTrc* const lpMessage = lpMemory
        ? new (lpMemory) RealmcCore::MessageTrc(rTrc, iValue)
        : 0;

    const RealmcCore::MessagePtr lMessage(lpMessage);
    const RealmcCore::ResponsePtr lResponse = rTask.SendMessage(lMessage, lpuSentTime);
    return lResponse.GetValue();
}

RealmcCore::String16 MakeTitleString(const EntryContentName& rName)
{
    return RealmcCore::String16(
        reinterpret_cast<const char16_t*>(rName.GetTitle()), RealmcCore::allocator());
}

} // namespace

SaveTask::SaveTask(RealmcCore::MessageQueue* pMessageQueue,
                   RealmcCore::MemcardState* pMemcardState,
                   XenonUtil::State* pState,
                   const GameInfo* pGameInfo,
                   const SaveInfo& rSaveInfo,
                   s32 iNumEntries,
                   const LoadEntryInfo* pEntries,
                   s32 iFlags,
                   const TitleInfo& rTitleInfo)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , mpGameInfo(pGameInfo)
    , miNumEntries(iNumEntries)
    , maEntries()
    , mSaveInfo(rSaveInfo)
    , mTitleInfo(rTitleInfo)
    , miFlags(iFlags)
{
    for (s32 i = 0; i < miNumEntries; ++i)
        maEntries[i] = pEntries[i];
}

// ARTIST SaveTask::DisplayTcrMessages @0x82B56380.
void SaveTask::DisplayTcrMessages(const s32* pCardStatus)
{
    const int liStatus = *pCardStatus;
    if (liStatus <= 0)
        return;

    if (liStatus <= 2)
    {
        RealmcCore::String16 lSaveTitle = MakeTitleString(mSaveInfo.mContentName);
        const RealmcCore::Trc lTrc(16, 3, lSaveTitle);
        SendTrcToGame(*this, lTrc, liStatus);
        return;
    }

    if (liStatus == 3)
    {
        RealmcCore::String16 lCardName = GetCardName();
        const RealmcCore::Trc lTrc(27, 3, lCardName);
        SendTrcToGame(*this, lTrc, liStatus);
        return;
    }

    if (liStatus == 4)
    {
        const RealmcCore::Trc lTrc(22, 3);
        SendTrcToGame(*this, lTrc, liStatus);
        return;
    }

    if (liStatus == 5)
    {
        const RealmcCore::Trc lTrc(23, 3);
        SendTrcToGame(*this, lTrc, liStatus);
        return;
    }

    if (liStatus >= 6 && liStatus <= 8)
    {
        RealmcCore::String16 lCardName = GetCardName();
        const RealmcCore::Trc lTrc(17, 3, lCardName);
        SendTrcToGame(*this, lTrc, liStatus);
    }
}

// ARTIST SaveTask::Execute @0x82B56620.
void SaveTask::Execute()
{
    s32 liResult = 4;
    bool lbSkipFreeSpaceCheck = false;
    int liCardResult = 0;
    s32 liCardStatus = 0;

    mpMemcardState->SetAutosaveState((miFlags & 1) != 0);
    liCardResult = SelectDevice(
        static_cast<int>(mTitleInfo.muTitle), (miFlags & 1) != 0, 0);
    liCardStatus = liCardResult;

    if (liCardResult == 7)
        liResult = 2;

    if (liCardResult == 0)
    {
        liResult = 0;
        u32 luSentTime = 0xFFFFFFFFu;
        XCONTENT_DATA lContentData;
        const ThumbnailData lEmptyThumbnail = { 0, 0 };

        liCardResult = XenonUtil::OpenContainer(
            mpState, &mSaveInfo, 1025, static_cast<int>(mTitleInfo.muTitle),
            &lContentData, 0);
        XenonUtil::CloseContainer(mpState, &lContentData, &lEmptyThumbnail);

        if (liCardResult != 9 && liCardResult != 2)
        {
            int liPromptId = 14;
            if (liCardResult == 8 || liCardResult == 7)
                liPromptId = 25;

            RealmcCore::String16 lSaveTitle = MakeTitleString(mSaveInfo.mContentName);
            RealmcCore::String16 lCardName = GetCardName();
            const RealmcCore::Trc lErrorTrc(
                liPromptId, 1798, lSaveTitle, lCardName);
            if (SendTrcToGame(*this, lErrorTrc, liCardResult) == 2)
            {
                liResult = 2;
            }
            else if (VerifyActiveCard())
            {
                liResult = 1;
            }
            else if (liCardResult == 8 || liCardResult == 7)
            {
                XenonUtil::DeleteContainer(mpState, &lContentData);
            }
            else
            {
                lbSkipFreeSpaceCheck = true;
            }
        }

        if (liResult == 0)
        {
            const bool lbInsufficientSpace = !lbSkipFreeSpaceCheck &&
                static_cast<u64>(static_cast<s64>(mSaveInfo.mContentName.GetSize())) >
                    mpState->mCardData.maCurrent.ulDeviceFreeBytes;

            if (lbInsufficientSpace)
            {
                liCardResult = 6;
                liResult = 1;
            }
            else
            {
                liCardResult = XenonUtil::OpenContainer(
                    mpState, &mSaveInfo, 1539, static_cast<int>(mTitleInfo.muTitle),
                    &lContentData, 0);
                if (liCardResult)
                {
                    liResult = 1;
                }
                else
                {
                    RealmcCore::String16 lSaveTitle = MakeTitleString(mSaveInfo.mContentName);
                    RealmcCore::String16 lCardName = GetCardName();
                    const RealmcCore::Trc lCreateTrc(15, 0, lSaveTitle, lCardName);
                    SendTrcToGame(*this, lCreateTrc, 0, &luSentTime);

                    liCardResult = XenonUtil::SaveFiles(
                        mpState, &lContentData, &mpGameInfo->mThumbnail,
                        1539, miNumEntries, maEntries);
                }
            }
        }

        if (!liCardResult)
            XenonUtil::CloseContainer(
                mpState, &lContentData,
                reinterpret_cast<const ThumbnailData*>(&mSaveInfo.mContentInfo));
        else
        {
            XenonUtil::CloseContainer(mpState, &lContentData, &lEmptyThumbnail);
        }

        if (luSentTime != 0xFFFFFFFFu)
        {
            const EA::Thread::ThreadTime luNow = EA::Thread::GetThreadTime();
            if (luNow < luSentTime + 3000)
            {
                const u32 luDelay = luSentTime - luNow + 3000;
                EA::Thread::ThreadSleep(&luDelay);
            }

            void* const lpClearMemory = RealmcCore::AllocateMem(
                0, sizeof(RealmcCore::MessageClear));
            RealmcCore::MessageClear* const lpMessageClear = lpClearMemory
                ? new (lpClearMemory) RealmcCore::MessageClear()
                : 0;
            SendMessageToGame(*this, lpMessageClear);
        }

        liCardStatus = XenonUtil::CardResultToCardStatus(
            static_cast<u32>(liCardResult), mpState);
        if (liCardStatus &&
            XenonUtil::CardExists(&mpState->mCardData.maCurrent.DeviceID))
        {
            liCardStatus = 8;
        }
    }
    else if (liResult != 2)
    {
        liResult = 1;
    }

    if (liResult != 2)
    {
        DisplayTcrMessages(&liCardStatus);
        liResult = liCardStatus != 0;
    }

    RealmcCore::RealmcString lFileName(
        mSaveInfo.mContentName.GetFileName(), RealmcCore::allocator());
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(MessageSaveDone));
    MessageSaveDone* const lpMessage = lpMemory
        ? new (lpMemory) MessageSaveDone(static_cast<u32>(liResult), lFileName,
                                        static_cast<u32>(liCardStatus))
        : 0;
    SendMessageToGame(*this, lpMessage);
}

} // namespace RealmcIface
