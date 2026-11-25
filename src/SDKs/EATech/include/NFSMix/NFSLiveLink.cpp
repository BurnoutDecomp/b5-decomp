#include "SDKs/EATech/include/NFSMix/NFSLiveLink.hpp"
#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"
#include "SDKs/EATech/include/NFSMix/NFSMixShape.hpp"
#include "SDKs/EA/GameTalk/GameTalk.h"
#include "rw/core/stdc/stdc.h"
#include <new>

NFSLiveLink* g_pNFSLiveLink = 0; // off_8325000C
extern NFSMixMaster* g_pNFSMixMaster;
static float g_fMonitorTime = 0.0f; // flt_83250010
namespace Nicotine { extern SnapshotMixer* g_pSnapshotMixer; }

NFSLiveLink::NFSLiveLink(Nicotine::IDynamicMixer* apMixer)
    : mpMixer(apMixer), mpRefreshedMap(0), mpRefreshedSnapshots(0)
{
    g_pNFSLiveLink = this;
    rw::core::stdc::MemFill8(mInput, 0, sizeof(mInput));
    rw::core::stdc::MemFill8(mMonitorOutput, 0, sizeof(mMonitorOutput));
    miSnapshotChannel = 0;
    mpSnapshotWrite = 0;
    miMonitorChannel = 0;
    mbMonitorEnabled = false;
    mbSnapshotMonitoring = false;
    mfSnapshotTime = 0.0f;
    mpMonitorWrite = 0;
    mfLastPingTime = 0.0f;
    rw::core::stdc::MemFill8(mMonitorChannels, 0, sizeof(mMonitorChannels));
    EA::GameTalk::GameTalkManager::RegisterMessageHandler(
        EA::GameTalk::GameTalkManager::GetInstance(), DynMixReceiveGameTalkMessage,
        "Nicotine.Runtime");
}

void NFSLiveLink::SetupChannelMonitoring()
{
    const int* lpInput = mInput;
    mfTime = 0.0f;
    if (*lpInput > 0)
    {
        mbMonitorEnabled = true;
        ++lpInput;
        miMonitorChannel = *lpInput++;
        miMonitorState = *lpInput++;
        rw::core::stdc::MemCopy(mMonitorChannels, lpInput, sizeof(mMonitorChannels));
        mpMonitorWrite = mMonitorOutput;
        rw::core::stdc::MemFill8(mMonitorOutput, 0, sizeof(mMonitorOutput));
        mbFirstMonitorRecord = true;
        miMonitorRecordSize = 0;
    }
    else
    {
        mbMonitorEnabled = false;
    }
}

void NFSLiveLink::ReadSnapshots()
{
    Nicotine::SnapshotMixer* lpSnapshots = Nicotine::g_pSnapshotMixer;
    bool lbMore = true;
    unsigned int luCount = 0;
    lpSnapshots->SetSnapshotOverride(lbMore);
    lpSnapshots->TurnOffAllSnapshots();
    for (unsigned int luIndex = 0; lbMore; ++luIndex)
    {
        if (luIndex >= 512)
            break;
        const int liSnapshot = mInput[luIndex];
        if (liSnapshot == 0xffff)
            lbMore = false;
        else
        {
            ++luCount;
            lpSnapshots->TurnOnSnapshot(liSnapshot);
        }
    }
    if (luCount == 0)
        lpSnapshots->SetSnapshotOverride(false);
}

void NFSLiveLink::ReadSnapshotData()
{
    const int liSnapshot = mInput[0];
    bool lbMore = true;
    Nicotine::SnapshotMixer* lpSnapshots = Nicotine::g_pSnapshotMixer;
    for (unsigned int luIndex = 1; lbMore; luIndex += 3)
    {
        if (luIndex >= 512)
            break;
        const int* lpEntry = mInput + luIndex;
        if (*lpEntry == 0xffff)
            lbMore = false;
        else
            lpSnapshots->UpdateSnapshotChannelData(liSnapshot, lpEntry, lpEntry[2]);
    }
}

void NFSLiveLink::ReadMixControlVolumes()
{
    NFSMixMaster* lpMaster = g_pNFSMixMaster;
    const int* lpInput = mInput;
    if (!lpMaster) return;
    int liID = *lpInput;
    for (; liID != 0; liID = *lpInput)
    {
        const int liVolume = *++lpInput;
        ++lpInput;
        NFSMixMap* lpMap = lpMaster->m_pMainMixMap;
        const int liState = (liID >> 16) & 0xff;
        if (!lpMap->m_StateRefCount[liState]) continue;
        NFSMixMapState* lpState = lpMap->m_pStateProcs[liState];
        if (!lpState) continue;
        if ((liID >> 28) == 0)
        {
            stMixCtlProc* lpProc = lpState->GetMixCtlProc(liID, 0);
            if (!lpProc) continue;
            if (!(liVolume & 0x8000))
            {
                lpProc->psdata->nOffset = liVolume & 0x7fff;
                stMixCtlSharedData* lpShared = lpProc->psdata;
                lpShared->nRatio =
                    0x7fff - NFSMixShape::GetQ15FromHundredthsdB(-lpShared->nOffset);
            }
            else
            {
                lpProc->psdata->nRatio =
                    0x7fff - NFSMixShape::GetQ15FromHundredthsdB(liVolume | static_cast<int>(0xffff0000u));
            }
        }
        else if ((liID >> 28) == 5)
        {
            stEvtMixCtlProc* lpProc = lpState->GetEvtMixCtlProc(liID, 0);
            if (!lpProc) continue;
            if (!(liVolume & 0x8000))
            {
                lpProc->pData_S->nOffset = liVolume & 0x7fff;
                stEvtMixCtlSharedData* lpShared = lpProc->pData_S;
                lpShared->nRatio =
                    0x7fff - NFSMixShape::GetQ15FromHundredthsdB(-lpShared->nOffset);
            }
            else
            {
                lpProc->pData_S->nRatio =
                    0x7fff - NFSMixShape::GetQ15FromHundredthsdB(liVolume | static_cast<int>(0xffff0000u));
            }
        }
    }
}

void NFSLiveLink::ReadMasterVolumes()
{
    if (!g_pNFSMixMaster) return;
    int* lpInput = mInput;
    while (*lpInput)
    {
        const int liID = *lpInput++;
        const int liVolume = *lpInput++;
        const unsigned int luState = (static_cast<unsigned int>(liID) >> 8) & 0xff;
        NFSMixMap* lpMap = g_pNFSMixMaster->m_pMainMixMap;
        if (!lpMap->m_StateRefCount[luState]) continue;
        NFSMixMapState* lpState = lpMap->m_pStateProcs[luState];
        if (!lpState) continue;
        stMasterMixChParams* lpParams = lpState->m_pFirstInstance->m_MixStateParams.pMasterMixChProcs[static_cast<unsigned char>(liID)].pMixChData_S->pMapParams;
        lpParams->MixData = (lpParams->MixData & 0xffff) | static_cast<int>((0u - static_cast<unsigned int>(liVolume)) << 16);
        if (Nicotine::g_pSnapshotMixer) Nicotine::g_pSnapshotMixer->UpdateNicotineChannelData(liID);
    }
}

// Raw ARTIST 82B45ED8: the selected master channel's preset table is a
// count word followed by packed preset-id/value words.
void NFSLiveLink::ReadVolPresets()
{
    const unsigned int luID = static_cast<unsigned int>(mInput[0]);
    const unsigned int luValue = static_cast<unsigned int>(mInput[1]);
    const unsigned int luState = (luID >> 16) & 0xff;
    const unsigned int luChannel = (luID >> 8) & 0xff;
    const unsigned int luPreset = (luValue >> 16) & 0x1f;
    NFSMixMap* lpMap = g_pNFSMixMaster->m_pMainMixMap;
    if (!lpMap->m_StateRefCount[luState]) return;
    NFSMixMapState* lpState = lpMap->m_pStateProcs[luState];
    if (!lpState || !lpState->m_NumStateCopies) return;
    lpState = lpState->m_pFirstInstance;
    if (lpState->m_MasterChannelsAdded < static_cast<int>(luChannel + 1)) return;
    int* lpPreset = lpState->m_MixStateParams.pMasterMixChProcs[luChannel].pMixChData_S->pPRESETS;
    const int liCount = *lpPreset++ & 0x1f;
    for (int i = 0; i < liCount; ++i, ++lpPreset)
    {
        if ((static_cast<unsigned int>(*lpPreset) >> 16 & 0x1f) == luPreset)
        {
            *lpPreset = (*lpPreset & static_cast<int>(0xffff0000u)) | (luValue & 0xffff);
            return;
        }
    }
}

void NFSLiveLink::ReadSnapshotRecall()
{
    Nicotine::SnapshotMixer* lpSnapshots = Nicotine::g_pSnapshotMixer;
    const float lfDuration = static_cast<float>(mInput[1]);
    const int liSnapshot = mInput[0];
    if (lfDuration < 0.0f)
        lpSnapshots->Recall(liSnapshot);
    else
        lpSnapshots->Recall(liSnapshot, lfDuration);
}

void NFSLiveLink::RefreshMap(const void* apMap, int aiMapSize, const void* apSnapshots, int aiSnapshotSize)
{
    if (!mpRefreshedMap)
    {
        mpRefreshedMap = g_pMixerAllocator->Alloc(2 * aiMapSize, "Nicotine::RefreshedMapData", 0);
        if (!mpRefreshedMap) mpMixer->DMixPrintf("Failed to Allocate memory for Refresh Mix Map");
    }
    if (!mpRefreshedSnapshots)
    {
        mpRefreshedSnapshots = g_pMixerAllocator->Alloc(2 * aiSnapshotSize, "Nicotine::RefreshedSnapshotData", 0);
        if (!mpRefreshedSnapshots) mpMixer->DMixPrintf("Failed to Allocate memory for Refresh Mix Map (Snapshot Data)");
    }
    Nicotine::SnapshotMixer* lpSnapshots = Nicotine::g_pSnapshotMixer;
    mpMixer->DMixAssert(lpSnapshots->miNumSnapshots < 32, "Number of snapshots is greater than 32");
    const int liCount = lpSnapshots->miNumSnapshots;
    bool labActive[32];
    for (int i = 0; i < liCount; ++i) labActive[i] = lpSnapshots->IsStatusActive(i);
    mpMixer->DestroyMap();
    rw::core::stdc::MemCopy(mpRefreshedMap, apMap, aiMapSize);
    rw::core::stdc::MemCopy(mpRefreshedSnapshots, apSnapshots, aiSnapshotSize);
    mpMixer->InitMap(static_cast<int*>(mpRefreshedMap));
    mpMixer->InitSnapshots(mpRefreshedSnapshots);
    for (int i = 0; i < liCount; ++i) lpSnapshots->SetSnapshot(i, labActive[i]);
}

void NFSLiveLink::CheckInputBuffer(int aiCommand)
{
    if (!aiCommand) return;
    switch (aiCommand)
    {
    case 1: case 4: ReadMasterVolumes(); break;
    // ARTIST 0x82B461D8 places the controls block before the presets block.
    case 3: ReadMixControlVolumes(); break;
    case 2: ReadVolPresets(); break;
    case 5: SetupChannelMonitoring(); break;
    case 10: mbSnapshotMonitoring = mInput[0] != 0; break;
    case 11: ReadSnapshotRecall(); break;
    }
    rw::core::stdc::MemFill8(mInput, 0, sizeof(mInput));
}

void NFSLiveLink::UpdateChannelMonitoring()
{
    if (!mbMonitorEnabled || mfTime - mfLastPingTime > 10.0f)
    {
        rw::core::stdc::MemFill8(mMonitorOutput, 0, sizeof(mMonitorOutput));
        mbMonitorEnabled = false;
        return;
    }
    if (mfTime > 0.3f)
    {
        mfTime = 0.0f;
        void* lpMemory = EA::GameTalk::Alloc(sizeof(EA::GameTalk::GameTalkMessage));
        EA::GameTalk::GameTalkMessage* lpMessage = lpMemory
            ? ::new (lpMemory) EA::GameTalk::GameTalkMessage("Tool.Nicotine") : 0;
        lpMessage->AddKeyContent("MonitorControl", 1, mMonitorOutput, sizeof(mMonitorOutput));
        EA::GameTalk::GameTalkManager::GetInstance();
        EA::GameTalk::GameTalkManager::SendMessage("Tool.Nicotine", lpMessage);
        delete lpMessage;
    }
    if (miMonitorRecordSize > static_cast<int>(static_cast<unsigned int>(mMonitorOutput + 512 - mpMonitorWrite) & 0x3fffffffu))
        mpMonitorWrite = mMonitorOutput;
    const unsigned int luChannel = static_cast<unsigned int>(miMonitorChannel);
    const unsigned int luState = (luChannel >> 8) & 0xff;
    NFSMixMap* lpMap = g_pNFSMixMaster->m_pMainMixMap;
    if (!lpMap || lpMap->m_StateRefCount[luState] < miMonitorState) return;
    NFSMixMapState* lpState = lpMap->m_pStateProcs[luState];
    if (!lpState) return;
    *mpMonitorWrite++ = miMonitorChannel;
    g_fMonitorTime += 0.015f;
    *mpMonitorWrite++ = static_cast<int>(g_fMonitorTime * 1000.0f);
    int** lppInputs;
    const int* lpInputIDs;
    int liNumInputs;
    if (luChannel & 0x10000000u)
    {
        stSubMixChProc* lpProc = lpState->GetSubMixChProc(static_cast<unsigned char>(luChannel), miMonitorState);
        *mpMonitorWrite++ = lpProc->pMixChData_U->Output;
        lppInputs = lpProc->pMixChData_U->pInputs;
        liNumInputs = lpProc->pMixChData_S->NumInputs;
        lpInputIDs = reinterpret_cast<const int*>(lpProc->pMixChData_S->pMapParams + 1);
    }
    else
    {
        stMasterMixChProc* lpProc = lpState->GetMasterMixChProc(static_cast<unsigned char>(luChannel), miMonitorState);
        *mpMonitorWrite++ = lpProc->pMixChData_U->Output;
        lppInputs = lpProc->pMixChData_U->pInputs;
        liNumInputs = (static_cast<unsigned int>(lpProc->pMixChData_S->pMapParams->MIXCHID) >> 16) & 0xff;
        lpInputIDs = reinterpret_cast<const int*>(lpProc->pMixChData_S->pMapParams + 1);
    }
    int liSeen = 0;
    while (liSeen < liNumInputs)
    {
        const unsigned int luInput = static_cast<unsigned int>(*lpInputIDs++);
        if ((luInput & 0xe0000000u) != 0x80000000u)
        {
            const unsigned int luInputState = (luInput >> 16) & 0xff;
            if (luInputState == luState)
            {
                *mpMonitorWrite++ = static_cast<int>(luInput);
                ++liSeen;
                *mpMonitorWrite++ = **lppInputs++;
            }
            else
            {
                int liCopies = g_pNFSMixMaster->m_pMainMixMap->m_StateRefCount[luInputState];
                if (liCopies > 0)
                {
                    liSeen += liCopies;
                    do
                    {
                        *mpMonitorWrite++ = static_cast<int>(luInput);
                        *mpMonitorWrite++ = **lppInputs++;
                    } while (--liCopies);
                }
            }
        }
    }
    *mpMonitorWrite++ = 0xffff;
    if (mbFirstMonitorRecord)
    {
        mbFirstMonitorRecord = false;
        miMonitorRecordSize = static_cast<int>(mpMonitorWrite - mMonitorOutput);
    }
}

void NFSLiveLink::UpdateActiveSnapshots(Nicotine::SnapshotMixer* apSnapshots, float afDeltaTime)
{
    if (!mbSnapshotMonitoring) return;
    if (mfTime - mfLastPingTime > 10.0f)
    {
        mbSnapshotMonitoring = false;
        return;
    }
    mfSnapshotTime += afDeltaTime;
    if (mfSnapshotTime <= 0.25f) return;
    mpSnapshotWrite = mSnapshotOutput;
    mfSnapshotTime = 0.0f;
    *mpSnapshotWrite++ = apSnapshots->mpSnapshotHdr->meType;
    int* const lpLast = mSnapshotOutput + 511;
    const int liSnapshots = apSnapshots->miNumSnapshots;
    for (int i = 0; i < liSnapshots && mpSnapshotWrite < lpLast; ++i)
        if (apSnapshots->IsStatusActive(i)) *mpSnapshotWrite++ = i;
    *mpSnapshotWrite++ = 0xffff;
    if (mpSnapshotWrite > lpLast) return;
    const int liChannels = apSnapshots->miNumChannels;
    const int liBatch = liChannels > 16 ? 16 : liChannels;
    for (int i = 0; i < liBatch && mpSnapshotWrite < lpLast; ++i)
    {
        Nicotine::SnapshotChannel* lpChannel = apSnapshots->GetChannel(miSnapshotChannel);
        *mpSnapshotWrite++ = apSnapshots->GetSnapshotChannelId(miSnapshotChannel);
        *mpSnapshotWrite++ = static_cast<s16>(lpChannel->GetCurrentVolume());
        *mpSnapshotWrite++ = static_cast<s16>(lpChannel->GetCeilingVolume());
        miSnapshotChannel = (miSnapshotChannel + 1) % liChannels;
    }
    *mpSnapshotWrite++ = 0xffff;
    mpMixer->DMixAssert(mpSnapshotWrite - mSnapshotOutput < 2048,
        "This buffer just stomped over memory. Increase KU_BUFFER_SIZE");
    EA::GameTalk::GameTalkMessage lMessage("Tool.Nicotine");
    lMessage.AddKeyContent("ActiveSnapshots", 1, mSnapshotOutput, sizeof(mSnapshotOutput));
    EA::GameTalk::GameTalkManager::GetInstance();
    EA::GameTalk::GameTalkManager::SendMessage("Tool.Nicotine", lMessage);
    rw::core::stdc::MemFill8(mSnapshotOutput, 0, sizeof(mSnapshotOutput));
}

void NFSLiveLink::ProcessLiveLink(Nicotine::SnapshotMixer* apSnapshots, float afDeltaTime)
{
    mfTime += afDeltaTime;
    UpdateChannelMonitoring();
    if (apSnapshots) UpdateActiveSnapshots(apSnapshots, afDeltaTime);
}

void NFSLiveLink::DynMixReceiveGameTalkMessage(EA::GameTalk::GameTalkMessage* apMessage)
{
    using namespace EA::GameTalk;
    using rw::core::stdc::StringCompare;
    NFSLiveLink* lpLink = g_pNFSLiveLink;
    if (!StringCompare(apMessage->GetKey(0), "Ping"))
    {
        lpLink->mfLastPingTime = lpLink->mfTime;
        void* lpMemory = Alloc(sizeof(GameTalkMessage));
        GameTalkMessage* lpReply = lpMemory ? ::new (lpMemory) GameTalkMessage("Tool.Nicotine") : 0;
        lpReply->AddKeyContent("Ping", 0, "Nicotine.xenon",
                               rw::core::stdc::StringLength("Nicotine.xenon"));
        GameTalkManager::GetInstance();
        GameTalkManager::SendMessage("Tool.Nicotine", lpReply);
        delete lpReply;
    }
    else if (!StringCompare(apMessage->GetKey(0), "RefreshMap"))
    {
        int liSnapshotSize = apMessage->GetSize(1);
        const void* lpSnapshot = apMessage->GetContent(1);
        int liMapSize = apMessage->GetSize(0);
        const void* lpMap = apMessage->GetContent(0);
        lpLink->RefreshMap(lpMap, liMapSize, lpSnapshot, liSnapshotSize);
    }
    else
    {
        int liSize = apMessage->GetSize(0);
        const void* lpContent = apMessage->GetContent(0);
        rw::core::stdc::MemCopy(lpLink->mInput, lpContent, liSize);
        if (!StringCompare(apMessage->GetKey(0), "WriteMasterMagnitudes")) lpLink->CheckInputBuffer(1);
        else if (!StringCompare(apMessage->GetKey(0), "WritePresetMagnitudes")) lpLink->CheckInputBuffer(2);
        else if (!StringCompare(apMessage->GetKey(0), "MuteChannels")) lpLink->CheckInputBuffer(4);
        else if (!StringCompare(apMessage->GetKey(0), "MonitorControl")) lpLink->CheckInputBuffer(5);
        else if (!StringCompare(apMessage->GetKey(0), "WriteMixControlVolumes")) lpLink->CheckInputBuffer(3);
        else if (!StringCompare(apMessage->GetKey(0), "WriteSnapshotData")) lpLink->ReadSnapshotData();
        else if (!StringCompare(apMessage->GetKey(0), "SetSnapshots")) lpLink->ReadSnapshots();
        else if (!StringCompare(apMessage->GetKey(0), "ActiveSnapshots")) lpLink->CheckInputBuffer(10);
        else if (!StringCompare(apMessage->GetKey(0), "RecallSnapshot")) lpLink->CheckInputBuffer(11);
    }
}
