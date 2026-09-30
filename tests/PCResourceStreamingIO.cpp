#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

static int assertions, checks, failures;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::printf("ASSERT %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}
static void Check(bool ok, const char* message) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", message); } }

namespace CgsResource {
// The callee boundary captures every argument without dereferencing synthetic pointers.
// The production decoder itself is extracted unchanged apart from its owning class name.
struct RequestDecoder {
    int miAllocateRequestEventId = -1;
    u64 id = 0; s32 pool = -1, count = -1;
    const void* entries = nullptr; bool* needs = nullptr; void* resources = nullptr;
    bool live = false, allow = false;
    bool AllocateResourceList(u64 a, s32 b, const void* c, s32 d, bool* e, void* f, bool g, bool h) {
        id=a; pool=b; entries=c; count=d; needs=e; resources=f; live=g; allow=h; return true;
    }
    void DoAllocateResourceListRequest(const void*);
};
#include "pc_resource_streaming_io.inc"
}

int main() {
    using namespace CgsResource;
    using namespace BundleLoaderIO;
    InputBuffer_Update input;
    OutputBuffer output;
    InputBuffer_Record record;
    input.Construct(); output.Construct(); record.Construct();
    auto& inRead = static_cast<const InputBuffer_Update&>(input);
    auto& outRead = static_cast<const OutputBuffer&>(output);
    auto& recordRead = static_cast<const InputBuffer_Record&>(record);

    input.LockForWrite(); output.LockForWrite();
    for (int i=0; i<256; ++i) {
        Events::LoadBundleRequest load = {};
        load.mpUser = reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x123456780000ull+i*256);
        load.miEventId = 700+i; load.miPoolId = 3; load.mbAllowFailiure = (i&1)!=0;
        std::snprintf(load.macFileName, sizeof(load.macFileName), "zone_%d.bundle", i);
        input.GetLoadBundleRequestQueue()->AddEvent(load);
        Events::UnloadBundleRequest unload = {};
        static_cast<Events::BundleLoaderEvent&>(unload) = load;
        input.GetUnloadBundleRequestQueue()->AddEvent(unload);
        Events::LoadBundleResponse response = {};
        static_cast<Events::BundleLoaderEvent&>(response) = load;
        output.GetLoadBundleResponseQueue()->AddEvent(response);
        Events::UnloadBundleResponse unloaded = {};
        static_cast<Events::BundleLoaderEvent&>(unloaded) = unload;
        output.GetUnloadBundleResponseQueue()->AddEvent(unloaded);
    }
    Check(input.GetLoadBundleRequestQueue()->GetLength()==256 && input.GetUnloadBundleRequestQueue()->GetLength()==256,
          "both input queues hold all 256 native records independently");
    Events::LoadBundleRequest overflow = {};
    Check(!input.GetLoadBundleRequestQueue()->AddEventSafe(overflow), "full queue refuses safe append without overwriting adjacent queue");
    input.UnlockForWrite(); output.UnlockForWrite(); input.LockForRead(); output.LockForRead();
    bool ordered = true;
    for (int i=0; i<256; ++i) {
        const auto& a=inRead.GetLoadBundleRequestQueue()->GetEvent(i);
        const auto& b=inRead.GetUnloadBundleRequestQueue()->GetEvent(i);
        const auto& c=outRead.GetLoadBundleResponseQueue()->GetEvent(i);
        const auto& d=outRead.GetUnloadBundleResponseQueue()->GetEvent(i);
        ordered &= a.miEventId==700+i && b.miEventId==a.miEventId && c.mpUser==a.mpUser && d.mpUser==a.mpUser;
        ordered &= reinterpret_cast<uintptr_t>(a.mpUser)==0x123456780000ull+i*256 && a.mbAllowFailiure==((i&1)!=0);
        ordered &= std::strcmp(a.macFileName, d.macFileName)==0;
    }
    Check(ordered, "request/response order, flags, names and 64-bit reply pointers survive all queue slots");
    Check(inRead.GetEventQueue()==inRead.GetLoadBundleRequestQueue(), "legacy input alias names same typed queue");
    input.UnlockForRead(); output.UnlockForRead();

    Events::AllocateResourceListRequest request = {};
    request.mpUser = reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x112233445560ull);
    request.miEventId = 73; request.miPoolId = 19; request.mListId.SetHash(0x80000000F1234567ull);
    request.mpEntries = reinterpret_cast<const BundleV2::ResourceEntry*>(0x123400009ABCull);
    request.mpcDebugData = reinterpret_cast<char*>(0xABCD00007890ull);
    request.miNumEntries = 37; request.mpNeeds = reinterpret_cast<bool*>(0x23450000ABCDull);
    request.mpResources = reinterpret_cast<SmallResource*>(0x34560000BCDEull);
    request.mbLiveUpdateReplace = true; request.mbAllowFailiure = true; request.mbCompressedBundle = false;
    output.LockForWrite();
    output.GetPoolSendQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&request), 16, sizeof(request));
    Events::OpenReadStreamRequest stream = {};
    stream.SetBuffer(reinterpret_cast<void*>(0x45670000CDEFul)); stream.SetBufferSize(512*1024); stream.SetNumBlocks(8);
    output.GetStreamRequestQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&stream), 16, sizeof(stream));
    output.UnlockForWrite(); output.LockForRead(); record.LockForWrite();
    const CgsModule::Event* payload = nullptr; s32 size=0;
    int tag=outRead.GetPoolSendQueue()->GetFirstEvent(&payload, &size);
    Check(tag==16 && size==sizeof(request), "pool send uses native request size");
    record.GetPoolReceiveQueue()->AddEvent(payload, tag, size);
    tag=outRead.GetStreamRequestQueue()->GetFirstEvent(&payload, &size);
    const auto* streamed = reinterpret_cast<const Events::OpenReadStreamRequest*>(payload);
    Check(tag==16 && size==sizeof(stream) && streamed->GetBuffer()==stream.GetBuffer()
          && streamed->GetBufferSize()==512*1024 && streamed->GetNumBlocks()==8, "stream request retains native buffer pointer and ring dimensions");
    output.UnlockForRead(); record.UnlockForWrite(); record.LockForRead();
    tag=recordRead.GetPoolReceiveQueue()->GetFirstEvent(&payload, &size);
    const auto* received = reinterpret_cast<const Events::AllocateResourceListRequest*>(payload);
    Check(tag==16 && size==sizeof(request) && std::memcmp(received,&request,sizeof(request))==0, "pool handoff preserves complete allocation record");
    RequestDecoder decoder;
    decoder.DoAllocateResourceListRequest(received);
    Check(decoder.id==request.mListId.GetHash(), "decoder keeps 64-bit list ID");
    Check(decoder.pool==request.miPoolId, "decoder keeps pool ID separate from reply target");
    Check(decoder.entries==request.mpEntries && decoder.count==request.miNumEntries, "decoder preserves entry pointer and count");
    Check(decoder.needs==request.mpNeeds && decoder.resources==request.mpResources, "decoder preserves native output pointers");
    Check(decoder.live && decoder.allow && decoder.miAllocateRequestEventId==73, "decoder preserves flags and response event ID");
    record.UnlockForRead();
    input.Destruct(); output.Destruct(); record.Destruct();
    input.Construct(); output.Construct(); record.Construct();
    input.LockForRead(); output.LockForRead(); record.LockForRead();
    Check(inRead.GetLoadBundleRequestQueue()->GetLength()==0 && inRead.GetUnloadBundleRequestQueue()->GetLength()==0
        && outRead.GetPoolSendQueue()->GetLength()==0 && outRead.GetLoadBundleResponseQueue()->GetLength()==0
        && outRead.GetUnloadBundleResponseQueue()->GetLength()==0 && outRead.GetStreamRequestQueue()->GetLength()==0
        && recordRead.GetPoolReceiveQueue()->GetLength()==0, "reconstruction clears every queue and rebinds typed storage");
    input.UnlockForRead(); output.UnlockForRead(); record.UnlockForRead();
    input.Destruct(); output.Destruct(); record.Destruct();
    Check(assertions==0, "all access follows original IO lock contract");
    std::printf("PCResourceStreamingIO: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
