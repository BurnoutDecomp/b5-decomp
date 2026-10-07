// Actual traffic dispatch IO factory and World frustum-result fan-out on poison.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks, failures, assertions, expectedAssertions;
static void Check(bool good, const char* label) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
}
}
#include "traffic_dispatch_io.inc"

int main() {
    using Input = BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch;
    using Queue = CgsModule::VariableEventQueue<32768, 16>;
    Check(sizeof(Queue) == 32768 + 4 * sizeof(s32), "native queue has the original inline capacity and control words");
    Check(sizeof(Input) == 0x8038, "named queue recovery preserves the published native dispatch input size");
    alignas(128) unsigned char memory[sizeof(Input) + 256];
    std::memset(memory, 0xA5, sizeof(memory));
    CgsModule::IOBufferStack stack; stack.Construct("TrafficDispatchFixture");
    Check(stack.Prepare(memory, sizeof(memory), 128), "actual IO stack prepares bounded poisoned storage");
    Check(!CgsModule::IOBufferHostZeroFillEnabled(), "factory diagnostic zero-fill stays off");
    Input* previous = nullptr;
    for (unsigned cycle = 0; cycle < 5; ++cycle) {
        Input* input = nullptr;
        Check(stack.CreateIOBuffer(&input, "TrafficDispatch"), "actual CreateIOBuffer invokes the concrete constructor");
        Check(input != nullptr && (reinterpret_cast<uintptr_t>(input) & 127) == 0, "actual factory returns its aligned native input");
        Check(!previous || input == previous, "destruct/pop and factory reuse the same prior tenant storage");
        const auto* objectBytes = reinterpret_cast<const unsigned char*>(input);
        Check(std::count(objectBytes, objectBytes + sizeof(*input), 0xA5) > 32000,
              "constructor preserves untouched queue payload instead of blanket zero-filling");
        input->LockForRead();
        Check(input->GetDispatchFrame() == nullptr && input->GetBlobbyShadowBuffer() == nullptr
              && input->GetCoronaSubmissionInterface() == nullptr && input->GetShadowMap() == nullptr,
              "original four pointer defaults are valid native-width pointers");
        input->UnlockForRead();
        const unsigned beforeUnlocked = assertions;
        ++expectedAssertions;
        input->GetSceneResultQueue();
        Check(assertions == beforeUnlocked + 1, "original mutable queue getter asserts without its write lock");
        input->LockForWrite();
        Queue* queue = input->GetSceneResultQueue();
        Check(queue != nullptr, "factory-produced input exposes its real scene-result queue");
        // Baseline/omission controls stop here instead of reproducing the live AV.
        if (!queue) { input->UnlockForWrite(); break; }
        const bool empty = queue->GetLength() == 0;
        Check(empty, "original queue Construct clears prior tenant/event state on every factory reuse");
        if (!empty) { input->UnlockForWrite(); break; }
        Check(queue->GetMaxLength() == 32768, "original scene-result queue capacity remains 32768 bytes");
        unsigned char oldEvent[16] = {};
        Check(queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(oldEvent), 8, 16), "seed an old frustum result before World's actual fan-out");
        input->UnlockForWrite();

        struct FrustumPayload { u32 words[4]; } payload = {{0x12345678u, cycle, 0x87654321u, 0x10203040u}};
        const CgsModule::Event* lpFrustumTestResult = reinterpret_cast<const CgsModule::Event*>(&payload);
        const s32 liResultType = 4, liResultSize = sizeof(payload);
        Input* lpTrafficDispatchInput = input;
#include "traffic_dispatch_fanout.inc"
        Check(queue->GetLength() == 1, "actual World Clear/AddEvent replaces the stale result once");
        const CgsModule::Event* result = nullptr; s32 resultSize = 0;
        Check(queue->GetFirstEvent(&result, &resultSize) == liResultType && resultSize == liResultSize
              && result && std::memcmp(result, &payload, sizeof(payload)) == 0,
              "real fan-out preserves the frustum type, size and complete payload");
        Check((reinterpret_cast<uintptr_t>(result) & 15) == 0, "real queue aligns its event payload to 16 bytes");
        Check(queue->GetNextEvent(result, &result, &resultSize) == -1 && result == nullptr,
              "cleared stale result is not replayed after the new frustum result");
        input->LockForWrite();
        auto* frame = reinterpret_cast<CgsGraphics::DispatchFrame*>(uintptr_t(0x123456789ABCDE00));
        input->SetDispatchFrame(frame);
        input->UnlockForWrite(); input->LockForRead();
        Check(input->GetDispatchFrame() == frame, "native dispatch pointer keeps all 64 bits");
        input->UnlockForRead(); previous = input;
        Check(stack.DestroyIOBuffer(&input) && input == nullptr && stack.muAllocated == 0,
              "actual Destruct/Free retires and pops the factory-produced input");
    }
    Check(assertions == expectedAssertions, "only intentional unlocked-getter probes assert");
    std::printf("PCTrafficDispatchIo: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
