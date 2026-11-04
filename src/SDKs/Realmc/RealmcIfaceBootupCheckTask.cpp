#include "SDKs/Realmc/RealmcIfaceBootupCheckTask.h"

#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcCoreMessageClear.h"
#include "SDKs/Realmc/RealmcIfaceMessages.h"
#include "SDKs/Realmc/RealmcMessage.h"
#include "SDKs/Realmc/RealmcMemcardState.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"
#include "SDKs/Realmc/RealmcTrc.h"

#include <cstring>
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

LoadEntryInfo* CopyLoadEntries(s32 iCount, const LoadEntryInfo* pSource)
{
    if (iCount <= 0 || !pSource)
        return 0;

    void* const lpMemory = ::operator new[](
        sizeof(LoadEntryInfo) * static_cast<size_t>(iCount), std::nothrow);
    LoadEntryInfo* const lpEntries = static_cast<LoadEntryInfo*>(lpMemory);
    if (lpEntries)
    {
        for (s32 i = 0; i < iCount; ++i)
            new (&lpEntries[i]) LoadEntryInfo();
    }

    // ARTIST's constructor memcpy follows the allocation/initializer loop
    // without an allocation-failure guard.
    std::memcpy(lpEntries, pSource, sizeof(LoadEntryInfo) * static_cast<size_t>(iCount));
    return lpEntries;
}

} // namespace

BootupCheckTask::BootupCheckTask(RealmcCore::MessageQueue* pMessageQueue,
                                 RealmcCore::MemcardState* pMemcardState,
                                 XenonUtil::State* pState,
                                 const GameInfo* pGameInfo,
                                 const SaveInfo& rSaveInfo,
                                 s32 iNumEntries,
                                 const LoadEntryInfo* pEntries,
                                 const TitleInfo& rTitleInfo)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , mpGameInfo(pGameInfo)
    , mSaveInfo(rSaveInfo)
    , mTitleInfo(rTitleInfo)
    , miNumEntries(iNumEntries)
    , mpEntries(0)
{
    if (miNumEntries > 0 && pEntries)
        mpEntries = CopyLoadEntries(miNumEntries, pEntries);
}

BootupCheckTask::~BootupCheckTask()
{
    ::operator delete[](mpEntries);
}

// ARTIST BootupCheckTask::DisplayTcrMessages @0x82B55668.
void BootupCheckTask::DisplayTcrMessages(const s32* pCardStatus)
{
    const int liStatus = *pCardStatus;
    if (liStatus <= 0)
        return;

    if (liStatus <= 2)
    {
        const RealmcCore::Trc lTrc(11, 3);
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

    if (liStatus >= 6 && liStatus <= 8)
    {
        RealmcCore::String16 lCardName = GetCardName();
        const RealmcCore::Trc lTrc(12, 3, lCardName);
        SendTrcToGame(*this, lTrc, liStatus);
    }
}

// ARTIST BootupCheckTask::Execute @0x82B55980.
void BootupCheckTask::Execute()
{
    s32 liResult = 0;
    int liCardResult = 0;

    if (miNumEntries != 0)
    {
        mpState->mCardData = CardData::Empty();
        liCardResult = SelectDevice(static_cast<int>(mTitleInfo.muTitle), true, 0);

        if (liCardResult == 7)
        {
            liResult = 2;
        }
        else if (liCardResult != 0)
        {
            // ARTIST bypasses CardResultToCardStatus on this selector-error
            // path and displays the selector result directly.
            liResult = liCardResult != 0;
            DisplayTcrMessages(&liCardResult);
        }
        else
        {
            XCONTENT_DATA lContentData;
            liCardResult = XenonUtil::OpenContainer(
                mpState, &mSaveInfo, 1025, static_cast<int>(mTitleInfo.muTitle),
                &lContentData, 0);
            if (!liCardResult)
                liCardResult = XenonUtil::LoadFiles(mpState, 1025, miNumEntries, mpEntries);

            const ThumbnailData lEmptyThumbnail = { 0, 0 };
            XenonUtil::CloseContainer(mpState, &lContentData, &lEmptyThumbnail);

            if (liCardResult && liCardResult != 2)
            {
                if (liCardResult != 9)
                {
                    const int liPromptId =
                        (liCardResult == 8 || liCardResult == 7) ? 10 : 11;
                    const RealmcCore::Trc lErrorTrc(liPromptId, 3);
                    SendTrcToGame(*this, lErrorTrc, 0);

                    const RealmcCore::Trc lDeleteTrc(18, 1798);
                    if (SendTrcToGame(*this, lDeleteTrc, 0) != 2)
                    {
                        if (VerifyActiveCard())
                            liResult = 1;
                        else if (liCardResult == 8 || liCardResult == 7)
                            XenonUtil::DeleteContainer(mpState, &lContentData);
                    }
                    else
                    {
                        liResult = 2;
                    }
                }

                if (liResult == 0)
                {
                    if (static_cast<u64>(static_cast<s64>(mSaveInfo.mContentName.GetSize())) >
                        mpState->mCardData.maCurrent.ulDeviceFreeBytes)
                    {
                        liCardResult = 6;
                        liResult = 1;
                    }
                    else
                    {
                        u32 luSentTime = 0xFFFFFFFFu;
                        RealmcCore::String16 lSaveName(
                            reinterpret_cast<const char16_t*>(mSaveInfo.mContentName.GetTitle()),
                            RealmcCore::allocator());
                        const RealmcCore::Trc lCreateTrc(15, 0, lSaveName);
                        SendTrcToGame(*this, lCreateTrc, liCardResult, &luSentTime);

                        void* const lpShowMemory = RealmcCore::AllocateMem(
                            0, sizeof(RealmcIface::MessageShowAutosaveIcon));
                        RealmcIface::MessageShowAutosaveIcon* const lpShowIcon = lpShowMemory
                            ? new (lpShowMemory) RealmcIface::MessageShowAutosaveIcon(1)
                            : 0;
                        SendMessageToGame(*this, lpShowIcon);

                        liCardResult = XenonUtil::OpenContainer(
                            mpState, &mSaveInfo, 1539, static_cast<int>(mTitleInfo.muTitle),
                            &lContentData, 0);
                        int liRetries = 2;
                        do
                        {
                            --liRetries;
                            if (!liCardResult)
                            {
                                liCardResult = XenonUtil::SaveFiles(
                                    mpState, &lContentData, &mpGameInfo->mThumbnail,
                                    1539, miNumEntries, mpEntries);
                                if (!liCardResult)
                                    XenonUtil::FlushContainer(mpState);
                            }
                        }
                        while (liRetries);

                        if (!liCardResult)
                            XenonUtil::CloseContainer(
                                mpState, &lContentData,
                                reinterpret_cast<const ThumbnailData*>(&mSaveInfo.mContentInfo));
                        else
                        {
                            XenonUtil::CloseContainer(mpState, &lContentData, &lEmptyThumbnail);
                            XenonUtil::DeleteContainer(mpState, &lContentData);
                        }

                        if (luSentTime != 0xFFFFFFFFu)
                        {
                            const EA::Thread::ThreadTime luNow = EA::Thread::GetThreadTime();
                            if (luNow < luSentTime + 3000)
                            {
                                const u32 luDelay = luSentTime - luNow + 3000;
                                EA::Thread::ThreadSleep(&luDelay);
                            }
                        }

                        void* const lpHideMemory = RealmcCore::AllocateMem(
                            0, sizeof(RealmcIface::MessageShowAutosaveIcon));
                        RealmcIface::MessageShowAutosaveIcon* const lpHideIcon = lpHideMemory
                            ? new (lpHideMemory) RealmcIface::MessageShowAutosaveIcon(0)
                            : 0;
                        SendMessageToGame(*this, lpHideIcon);

                        void* const lpClearMemory = RealmcCore::AllocateMem(
                            0, sizeof(RealmcCore::MessageClear));
                        RealmcCore::MessageClear* const lpMessageClear = lpClearMemory
                            ? new (lpClearMemory) RealmcCore::MessageClear()
                            : 0;
                        SendMessageToGame(*this, lpMessageClear);
                    }
                }
            }

            liCardResult = XenonUtil::CardResultToCardStatus(
                static_cast<u32>(liCardResult), mpState);
            if (liCardResult &&
                XenonUtil::CardExists(&mpState->mCardData.maCurrent.DeviceID))
            {
                liCardResult = 8;
            }

            if (liResult != 2)
            {
                liResult = liCardResult != 0;
                DisplayTcrMessages(&liCardResult);
            }
        }
    }

    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(MessageBootupDone));
    MessageBootupDone* const lpMessage = lpMemory
        ? new (lpMemory) MessageBootupDone(static_cast<u32>(liResult))
        : 0;
    SendMessageToGame(*this, lpMessage);
}

} // namespace RealmcIface
