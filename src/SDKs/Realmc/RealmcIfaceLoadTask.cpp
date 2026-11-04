#include "SDKs/Realmc/RealmcIfaceLoadTask.h"

#include "SDKs/Realmc/RealmcCore.h"
#include "SDKs/Realmc/RealmcCoreMessageClear.h"
#include "SDKs/Realmc/RealmcMemcardState.h"
#include "SDKs/Realmc/RealmcMessageQueue.h"
#include "SDKs/Realmc/RealmcTrc.h"
#include "SDKs/Realmc/RealmcIfaceMessageCheckLoadedData.h"
#include "SDKs/Realmc/RealmcIfaceMessageLoadDone.h"

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
                  int iValue)
{
    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(RealmcCore::MessageTrc));
    RealmcCore::MessageTrc* const lpMessage = lpMemory
        ? new (lpMemory) RealmcCore::MessageTrc(rTrc, iValue)
        : 0;

    const RealmcCore::MessagePtr lMessage(lpMessage);
    const RealmcCore::ResponsePtr lResponse = rTask.SendMessage(lMessage, 0);
    return lResponse.GetValue();
}

} // namespace

LoadTask::LoadTask(RealmcCore::MessageQueue* pMessageQueue,
                   RealmcCore::MemcardState* pMemcardState,
                   XenonUtil::State* pState,
                   const GameInfo* pGameInfo,
                   const EntryContentName& rEntryContentName,
                   s32 iNumEntries,
                   const LoadEntryInfo* pEntries,
                   s32 iFlags,
                   const TitleInfo& rTitleInfo)
    : XenonRunnableTask(pMessageQueue, pMemcardState, pState)
    , miNumEntries(iNumEntries)
    , maEntries()
    , mEntryContentName(rEntryContentName)
    , mpGameInfo(pGameInfo)
    , mTitleInfo(rTitleInfo)
    , miFlags(iFlags)
{
    for (s32 i = 0; i < miNumEntries; ++i)
        maEntries[i] = pEntries[i];
}

// ARTIST LoadTask::DisplayTcrMessages @0x82B57138.
void LoadTask::DisplayTcrMessages(const s32* pCardStatus)
{
    const int liStatus = *pCardStatus;
    if (liStatus <= 0)
        return;

    if (liStatus <= 2)
    {
        RealmcCore::String16 lEntryTitle(
            reinterpret_cast<const char16_t*>(mEntryContentName.GetTitle()),
            RealmcCore::allocator());
        const RealmcCore::Trc lTrc(11, 3, lEntryTitle);
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

    if (liStatus == 5)
    {
        const RealmcCore::Trc lTrc(23, 3);
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

// ARTIST LoadTask::Execute @0x82B57410.
void LoadTask::Execute()
{
    s32 liResult = 4;
    int liCardResult = 0;

    mpMemcardState->SetAutosaveState((miFlags & 1) != 0);
    const int liSelectResult = SelectDevice(
        static_cast<int>(mTitleInfo.muTitle), (miFlags & 1) != 0, 0);
    if (liSelectResult == 7)
        liResult = 2;

    if (liSelectResult == 0)
    {
        XCONTENT_DATA lContentData;
        liCardResult = XenonUtil::OpenContainer(
            mpState, &mEntryContentName, 1025, static_cast<int>(mTitleInfo.muTitle),
            &lContentData, 0);

        if (!liCardResult)
        {
            if (lContentData.szDisplayName[0])
                mEntryContentName.SetTitle(lContentData.szDisplayName);

            liCardResult = XenonUtil::LoadFiles(
                mpState, 1025, miNumEntries, maEntries);

            void* const lpClearMemory = RealmcCore::AllocateMem(
                0, sizeof(RealmcCore::MessageClear));
            RealmcCore::MessageClear* const lpMessageClear = lpClearMemory
                ? new (lpClearMemory) RealmcCore::MessageClear()
                : 0;
            SendMessageToGame(*this, lpMessageClear);

            mEntryContentName.SetTitle(lContentData.szDisplayName);

            if (!liCardResult)
            {
                void* const lpMemory = RealmcCore::AllocateMem(
                    0, sizeof(MessageCheckLoadedData));
                MessageCheckLoadedData* const lpMessage = lpMemory
                    ? new (lpMemory) MessageCheckLoadedData(
                          static_cast<u32>(miNumEntries), maEntries)
                    : 0;

                const RealmcCore::MessagePtr lMessage(lpMessage);
                const RealmcCore::ResponsePtr lResponse = SendMessage(lMessage, 0);
                liCardResult = lResponse.GetValue() ? 8 : 0;
            }
        }

        const ThumbnailData lEmptyThumbnail = { 0, 0 };
        XenonUtil::CloseContainer(mpState, &lContentData, &lEmptyThumbnail);
    }

    s32 liCardStatus = XenonUtil::CardResultToCardStatus(
        static_cast<u32>(liCardResult), mpState);
    if (liCardStatus &&
        XenonUtil::CardExists(&mpState->mCardData.maCurrent.DeviceID))
    {
        liCardStatus = 8;
    }

    if (liResult != 2)
    {
        DisplayTcrMessages(&liCardStatus);
        liResult = liCardStatus != 0;
    }

    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(MessageLoadDone));
    MessageLoadDone* const lpMessage = lpMemory
        ? new (lpMemory) MessageLoadDone(
              static_cast<u32>(liResult), liCardStatus,
              mEntryContentName.GetFileName(),
              static_cast<u32>(miNumEntries), maEntries)
        : 0;
    SendMessageToGame(*this, lpMessage);
}

} // namespace RealmcIface
