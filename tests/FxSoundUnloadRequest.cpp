// ARTIST 82695710 copies an optional handle and queues a bundle unload.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
using u32 = uint32_t;
static int handleQueries, invalidHandleQueries;
struct Handle {
    const void* source = nullptr;
    const void* target = nullptr;
    uint64_t GetResourceId() const {
        ++handleQueries;
        if (!source) ++invalidHandleQueries;
        return 123;
    }
};
struct Queue {
    void* buffer = nullptr;
    int capacity = 0, alignment = 0, constructs = 0;
    void Construct(void* b, int c, int a) { buffer = b; capacity = c; alignment = a; ++constructs; }
};
namespace BrnSound { namespace Logic {
struct ResourceRegistrar {
    struct RequestedResource {
        Handle mResourceHandle;
        int miResourceType = 0, miId12C = 0, miId130 = 0;
        char macBundleName[64] = {};
    };
    struct QueuedResource {
        Handle mResourceHandle;
        uint64_t mResourceId;
        u32 muResourceNameHash;
        int miPad12C, mState, mResourceClass, mBundleId;
        bool mbRelinquished;
        char maPad141[3], macBundleName[64], macReceiverBuffer[192];
        void* mpRequester;
        Queue mReceiverQueue;
        QueuedResource(const RequestedResource& source);
    };
};
#include "fx_sound_unload_request.inc"
}}
static int checks, failures;
static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
int main() {
    using R = BrnSound::Logic::ResourceRegistrar;
    for (bool named : {false, true}) {
        R::RequestedResource source;
        if (named) source.mResourceHandle = {&checks, &failures};
        source.miResourceType = named ? 0x12345678 : 0;
        source.miId12C = 4;
        source.miId130 = 6;
        std::strcpy(source.macBundleName, "sound\\aems\\Boost_Patch_Bank.bundle");
        handleQueries = invalidHandleQueries = 0;
        R::QueuedResource queued(source);
        Check(invalidHandleQueries == 0, "bundle-only unload never dereferences a missing resource");
        Check(handleQueries == 0, "unload does not resolve an individual resource ID");
        Check(queued.mResourceHandle.source == source.mResourceHandle.source && queued.mResourceHandle.target == source.mResourceHandle.target,
              "optional handle is preserved");
        Check(queued.muResourceNameHash == static_cast<u32>(source.miResourceType), "name hash survives enqueue");
        Check(queued.mState == 5 && !queued.mbRelinquished && queued.mpRequester == nullptr,
              "request enters unload state without a requester");
        Check(queued.mResourceClass == 4 && queued.mBundleId == 6, "resource class and pool survive enqueue");
        Check(std::strcmp(queued.macBundleName, source.macBundleName) == 0, "bundle path survives enqueue");
        Check(queued.mReceiverQueue.buffer == queued.macReceiverBuffer && queued.mReceiverQueue.capacity == 192 &&
              queued.mReceiverQueue.alignment == 16 && queued.mReceiverQueue.constructs == 1, "receiver binds its own aligned queue");
    }
    std::printf("FxSoundUnloadRequest: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
