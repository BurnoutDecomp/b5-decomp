// Run the actual PC producer and frame bridge with native event records/queues.
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/PC/CgsAptResourceOwnerPC.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"

using ResourceQueue = CgsModule::VariableEventQueue<18432, 16>;
using ViewQueue = CgsModule::VariableEventQueue<65536, 16>;

static unsigned suChecks = 0, suFailures = 0, suAsserts = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcMessage); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int)
{
    ++suAsserts;
    // Stop a negative control before the original AddEvent writes out of bounds.
    throw std::runtime_error(lpcText ? lpcText : "assert");
}
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

static std::map<std::string, std::vector<CgsResource::Entry*>> saBundleMembers;
namespace CgsResource::PCAptOwnership
{
    // Bundle IO is a boundary here; real entry-list ownership is exercised separately
    // by PCBundleOwnership.cpp, including shared reloads and final unload.
    void VisitBundleEntries(Pool*, const char* lpacPath,
                            void (*lpVisitor)(Entry*, void*), void* lpContext)
    {
        for (Entry* lpEntry : saBundleMembers[lpacPath]) lpVisitor(lpEntry, lpContext);
    }
}

namespace BrnGui
{
    using ResourceInput = CgsGui::GuiResourceModuleIO::InputBuffer;
    using ResourceOutput = CgsGui::GuiResourceModuleIO::OutputBuffer;
    struct ResourceModuleFixture
    {
        bool mbReady = true;
        CgsResource::Entry* mpEntry = nullptr;
        void AddResourceRequests(const CgsGui::GuiEventQueueSmall*, ResourceInput*) {}
        void Update(ResourceInput*, ResourceOutput* lpOutput)
        {
            if (!mbReady) return;
            mbReady = false;
            lpOutput->LockForWrite();
            // The reporter's 5 x 19 header burst, followed by real completions.
            for (s32 liBundle = 0; liBundle < 5; ++liBundle)
            {
                const s32 laiTypes[] = {4, 5, 6, 7, 10};
                for (s32 liClip = 0; liClip < 19; ++liClip)
                    Load(lpOutput, laiTypes[liBundle], 0);
            }
            for (u32 luId = 120; luId < 123; ++luId) Load(lpOutput, 4, luId);
            Load(lpOutput, 1, 0); // id zero remains valid for non-Apt requests.
            CgsGui::GuiEventUnloadRequestNotification lRequest = {};
            lRequest.meRequestType = static_cast<CgsGui::ResourceRequestTypes>(4);
            lRequest.muLoadRequestId = 120;
            lpOutput->AddUnloadRequestNotification(&lRequest);
            CgsGui::GuiEventUnloadNotification lComplete = {};
            lComplete.meRequestType = lRequest.meRequestType;
            lComplete.muLoadRequestId = 120;
            lpOutput->AddUnloadNotification(&lComplete);
            lpOutput->UnlockForWrite();
        }
        void Load(ResourceOutput* lpOutput, s32 liType, u32 luId)
        {
            CgsGui::GuiEventLoadNotification lNote = {};
            lNote.meRequestType = static_cast<CgsGui::ResourceRequestTypes>(liType);
            lNote.muLoadRequestId = luId;
            lNote.mResourceHandle.mpSourceEntry = mpEntry;
            lNote.mResourceHandle.mpResourceMemory = &mpEntry->mResource.m_baseResources[0];
            lpOutput->AddLoadNotification(&lNote);
        }
        const ResourceInput::GuiEventQueue* GetLoadedNotifications(ResourceOutput* lpOutput)
        {
            return lpOutput->GetLoadNotifications();
        }
    };
    struct ViewInputFixture
    {
        CgsGui::GuiEventQueueBase<65536, 16> mQueue;
        void LockForWrite() {}
        void UnlockForWrite() {}
        CgsGui::GuiEventQueueBase<65536, 16>& GetViewStateQueue() { return mQueue; }
    };
    struct GuiModule
    {
        CgsGui::ModelIO::InputBuffer mModelInputBuffer;
        CgsGui::ModelIO::OutputBuffer mModelOutputBuffer;
        u64 mauCanary[4] = {0x123456789abcdef0ull, 1, 2, 3};
        ResourceInput mResourceInputBuffer;
        ResourceOutput mResourceOutputBuffer;
        ViewInputFixture mViewInputBuffer;
        ResourceModuleFixture mGuiResourceModule;
        std::vector<s32> maFlowEvents;
        void RouteEventToFlow(const CgsModule::Event*, s32 liId, s32) { maFlowEvents.push_back(liId); }
        void DispatchGuiResourceModule();
    };
}

#include "pc_gui_resource_notifications.inc"

struct AptType : CgsResource::Type { u32 GetTypeID() const override { return 30; } };
struct OtherType : CgsResource::Type { u32 GetTypeID() const override { return 1; } };
struct PoolFixture : CgsResource::Pool
{
    CgsResource::Entry maEntries[64] = {};
    u8 maStatuses[64] = {};
    u64 maHeaders[64] = {};
    PoolFixture(const CgsResource::Type* lpType)
    {
        mpResourceEntries = maEntries; mpx8ResourceStatuses = maStatuses; muMaxResources = 64;
        for (u32 i = 0; i < 64; ++i)
        {
            maStatuses[i] = 2;
            maEntries[i].mpResourceType = lpType;
            maEntries[i].mResource.m_baseResources[0] = &maHeaders[i];
        }
    }
};

int main()
{
    AptType lAptType; OtherType lOtherType;
    PoolFixture lPool(&lAptType);
    saBundleMembers["current.bundle"] = {&lPool.maEntries[1], &lPool.maEntries[2], &lPool.maEntries[3]};
    lPool.maEntries[2].mpResourceType = &lOtherType;
    lPool.maEntries[3].mResource.m_baseResources[0] = nullptr;
    try
    {
        auto lpOutput = std::make_unique<CgsGui::GuiResourceModuleIO::OutputBuffer>();
        lpOutput->Construct(); lpOutput->LockForWrite();
        const u32 luCount = CgsGui::EmitForFixture(&lPool, "current.bundle", 4, lpOutput.get());
        Check(luCount == 1 && lpOutput->GetLoadNotificationsNonConst()->ResourceQueue::GetLength() == 1,
              "producer emits only the loaded bundle's valid Apt members");
        lpOutput->UnlockForWrite(); lpOutput->LockForRead();
        const CgsModule::Event* lpEvent = nullptr; s32 liSize = 0;
        lpOutput->GetLoadNotifications()->GetFirstEvent(&lpEvent, &liSize);
        const auto* lpNote = reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
        Check(liSize == sizeof(*lpNote) && lpNote->muLoadRequestId == 0 &&
              lpNote->mResourceHandle.mpSourceEntry == &lPool.maEntries[1] &&
              lpNote->mResourceHandle.mpResourceMemory == &lPool.maEntries[1].mResource.m_baseResources[0],
              "producer preserves native handles and the view-registration marker");
        lpOutput->UnlockForRead(); lpOutput->LockForWrite();
        lpOutput->GetLoadNotificationsNonConst()->Clear();
        Check(CgsGui::EmitForFixture(&lPool, "current.bundle", 4, lpOutput.get()) == 1,
              "shared reload still registers the bundle's resident member");
        lpOutput->GetLoadNotificationsNonConst()->Clear();
        Check(CgsGui::EmitForFixture(&lPool, "missing.bundle", 4, lpOutput.get()) == 0 &&
              lpOutput->GetLoadNotificationsNonConst()->ResourceQueue::GetLength() == 0,
              "failed or absent bundle cannot register unrelated resident clips");
        lpOutput->UnlockForWrite();
    }
    catch (const std::exception& lrError) { Check(false, lrError.what()); }

    try
    {
        auto lpModule = std::make_unique<BrnGui::GuiModule>();
        lpModule->mModelInputBuffer.Construct(); lpModule->mModelOutputBuffer.Construct();
        lpModule->mResourceInputBuffer.Construct(); lpModule->mResourceOutputBuffer.Construct();
        lpModule->mViewInputBuffer.mQueue.ViewQueue::Construct();
        lpModule->mGuiResourceModule.mpEntry = &lPool.maEntries[1];
        bool lbAllFrames = true, lbAllHandles = true;
        for (s32 liFrame = 0; liFrame < 60; ++liFrame)
        {
            lpModule->mGuiResourceModule.mbReady = true;
            lpModule->DispatchGuiResourceModule();
            lpModule->mModelOutputBuffer.LockForRead();
            const auto* lpModel = lpModule->mModelOutputBuffer.GetLoadNotifications();
            lbAllFrames = lbAllFrames && lpModel->GetLength() == 6 &&
                lpModule->mViewInputBuffer.mQueue.ViewQueue::GetLength() == 99 && lpModule->maFlowEvents.size() == 5;
            const CgsModule::Event* lpEvent = nullptr; s32 liSize = 0;
            for (s32 liType = lpModel->GetFirstEvent(&lpEvent, &liSize); lpEvent;
                 liType = lpModel->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liType == 14)
                {
                    const auto* lpNote = reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
                    lbAllHandles = lbAllHandles && liSize == sizeof(*lpNote) &&
                        lpNote->mResourceHandle.mpSourceEntry == &lPool.maEntries[1] &&
                        (lpNote->meRequestType == 1 || lpNote->muLoadRequestId >= 120);
                }
            }
            lpModule->mModelOutputBuffer.UnlockForRead();
            lpModule->mModelOutputBuffer.Clear();
            lpModule->mViewInputBuffer.mQueue.ViewQueue::Clear(); lpModule->maFlowEvents.clear();
        }
        Check(lbAllFrames, "60 repeated bursts preserve all completions, view registrations and flow replies");
        Check(lbAllHandles, "model payloads retain native handles and real request ids");
        Check(lpModule->mauCanary[0] == 0x123456789abcdef0ull && lpModule->mauCanary[3] == 3,
              "the original 4KB model queue never overwrites adjacent state");
    }
    catch (const std::exception& lrError) { Check(false, lrError.what()); }
    Check(suAsserts == 0, "no queue overflow or lock assertions");
    std::printf("PCGuiResourceNotifications: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures != 0;
}
