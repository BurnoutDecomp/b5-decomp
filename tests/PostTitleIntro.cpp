// The runner extracts the production state methods and event declarations.
// The queue, music/video payloads and StateInterface emitters use real headers.
// Cache, hashing, registration and assert reporting are fixture boundaries.
#include <cstdio>
#include <cstring>
#include <memory>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Gui/BrnGuiVideoEvents.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"

static unsigned guChecks, guFailures, guAsserts;
static void Check(bool lbGood, const char* lpcName)
{
    ++guChecks;
    if (!lbGood) { ++guFailures; std::printf("FAIL %s\n", lpcName); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
}}
namespace CgsDev {
namespace Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}
namespace CgsResource {
u64 ID::HashString(const u8*) { return 0x1A05C520u; }
}
namespace CgsSound { namespace Playback {
uintptr_t Name::MakeHash(const char* lpcName)
{
    return std::strcmp(lpcName, "intro") == 0 ? 0xBA18C1D7u : 0x428607BFu;
}
}}
namespace CgsGui {
GuiStackEventQueue::GuiEventQueueLarge* StateInterface::GetOutputEventQueue()
{
    return &mOutEventQueue;
}
}
namespace BrnGui {
class GuiCache {
public:
    bool mbPlay = true;
    bool ShouldPlayIntroVideo() const { return mbPlay; }
};
struct PostTitleScreenLoad {
    enum EState { E_IDLE, E_PLAYING_VIDEO, E_FINISHED_VIDEO };
    GuiCache* mpGuiCache = nullptr;
    CgsGui::StateInterface* mpStateInterface = nullptr;
    void* mpInGuiEventQueue = nullptr;
    EState meState = E_IDLE;
    bool mbVideoFinished = false;
    void HandleIncomingEvents();
    void Update();
};
#include "post_title_intro.inc"
}

int main()
{
    using Queue = CgsModule::VariableEventQueue<18432, 16>;
    auto lpInput = std::make_unique<Queue>();
    auto lpInterface = std::make_unique<CgsGui::StateInterface>();
    lpInput->Construct();
    using OutputQueue = CgsModule::VariableEventQueue<65536, 16>;
    static_cast<OutputQueue&>(lpInterface->mOutEventQueue).Construct();
    BrnGui::GuiCache lCache;
    BrnGui::PostTitleScreenLoad lState;
    lState.mpGuiCache = &lCache;
    lState.mpStateInterface = lpInterface.get();
    lState.mpInGuiEventQueue = lpInput.get();
    auto* lpOutput = static_cast<OutputQueue*>(lpInterface->GetOutputEventQueue());
    auto lCheckRecords = [&](const s32* lpaExpected, unsigned luCount) {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = lpOutput->GetFirstEvent(&lpEvent, &liSize);
        unsigned luIndex = 0;
        for (; lpEvent; liType = lpOutput->GetNextEvent(lpEvent, &lpEvent, &liSize)) {
            Check(liType == 40, "all state commands use channel 40");
            const auto* lpHeader = reinterpret_cast<const u32*>(lpEvent);
            Check(luIndex < luCount && lpHeader[1] == static_cast<u32>(lpaExpected[luIndex]),
                  "command order and inner event id");
            if (lpHeader[1] == 23) {
                Check(liSize == 20 && lpHeader[0] == 8 && lpHeader[2] == 12,
                      "music event has the original eight-byte boxed payload");
                const u32 luExpectedHash = lState.meState == BrnGui::PostTitleScreenLoad::E_PLAYING_VIDEO
                    ? 0u : 0x428607BFu;
                Check(lpHeader[3] == luExpectedHash, "music stops before intro and resumes afterward");
                const auto* lpBytes = reinterpret_cast<const u8*>(lpEvent);
                Check(lpBytes[16] == 0 && lpBytes[17] == 0, "music flags preserved");
            }
            if (lpHeader[1] == 508 || lpHeader[1] == 509) {
                Check(liSize == 64 && lpHeader[0] == 48 && lpHeader[2] == 16,
                      "video definition boxed with native alignment");
                if (lpHeader[1] == 508) {
                    const auto* lpVideo = reinterpret_cast<const BrnGui::GuiEventPlayVideo*>(
                        reinterpret_cast<const u8*>(lpEvent) + lpHeader[2]);
                    Check(lpVideo->muVideoResourceId == 0x1A05C520u &&
                          lpVideo->muSoundStreamName == 0xBA18C1D7u &&
                          lpVideo->mbDisableCustomSoundtracks,
                          "intro keeps its video identity, audio stream and custom-music gate");
                }
            }
            ++luIndex;
        }
        Check(luIndex == luCount, "exactly the expected commands emitted");
        lpOutput->Clear();
    };
    lState.Update();
    const s32 laStart[] = {20, 23, 508};
    lCheckRecords(laStart, 3);
    Check(lState.meState == BrnGui::PostTitleScreenLoad::E_PLAYING_VIDEO,
          "new save waits for movie completion");
    lState.Update();
    lCheckRecords(nullptr, 0);
    CgsModule::Event lEvent;
    lpInput->AddEvent(&lEvent, 6, 1);
    lState.Update();
    const s32 laSkip[] = {509};
    lCheckRecords(laSkip, 1);
    Check(!lState.mbVideoFinished, "skip still waits for the player's completion reply");
    lpInput->AddEvent(&lEvent, 510, 1);
    lState.Update();
    const s32 laFinish[] = {23, 19, 70};
    lCheckRecords(laFinish, 3);
    Check(lState.meState == BrnGui::PostTitleScreenLoad::E_FINISHED_VIDEO,
          "completion resumes loading");
    lState.Update();
    lCheckRecords(nullptr, 0);
    lCache.mbPlay = false;
    lState.meState = BrnGui::PostTitleScreenLoad::E_IDLE;
    lState.Update();
    const s32 laReturning[] = {70};
    lCheckRecords(laReturning, 1);
    Check(guAsserts == 0, "new and returning save sequences do not assert");
    std::printf("PostTitleIntro: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
