#include "pc/gcm/renderengine/WorldGeometry.cpp"

namespace renderengine {
    IDirect3DDevice9* gDevice = nullptr;
    void WorldVd32_OnResourceMemoryFreed(const void*, size_t) {}
    void WorldVd32_ReleaseAll() {}
}
namespace BrnDiag { void LogBoundSurfaces(const char*, u32, bool) {} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }

static unsigned checks, failures;
static void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}

int main()
{
    using namespace renderengine;
    constexpr size_t bytes = 256 * 1024;
    auto* headers = static_cast<u8*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* data = static_cast<u8*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!headers || !data) return 2;
    const auto add = [&](unsigned h, unsigned d) {
        WorldGeometryVertexPlan plan{};
        plan.mpHeader = headers + h; plan.mpData = data + d;
        plan.muNumVertices = 2; plan.muSourceStride = plan.muExpandedStride = 16;
        const auto key = MakeVertexKey(plan);
        RetainedVertexBuffer entry{}; // failed native allocation still needs lifetime tracking
        entry.mpHeader = plan.mpHeader;
        entry.mpSourceBegin = data + d; entry.mpSourceEnd = data + d + 32;
        return RegisterMirror(sVertexBuffers, sVertexPages, sVertexTokenPages,
                              sVertexReferences, key, entry).muEvictionToken;
    };
    const auto old = add(0, 0);
    Check(sVertexBuffers.size() == 1, "failed-allocation record is registered for later invalidation");
    WorldGeometry_OnResourceMemoryFreed(headers, 1);
    Check(sVertexBuffers.empty(), "header-only notification evicts its cached failure");
    const auto current = add(128, 8192);
    Check(!sbCompactLifetime || (old != current && !sVertexReferences.Find(old)),
          "slot reuse leaves the previous data-page identity dead");
    WorldGeometry_OnResourceMemoryFreed(data, 32);
    Check(sVertexBuffers.size() == 1, "late notification for old data preserves the new resource");
    Check(sbCompactLifetime ? sVertexTokenPages.find(PageOf(data)) == sVertexTokenPages.end()
                            : sVertexPages.find(PageOf(data)) == sVertexPages.end(),
          "late old-data notification removes stale page references completely");
    add(256, 8192 + 128);
    WorldGeometry_OnResourceMemoryFreed(data + 8192 + 8, 1);
    Check(sVertexBuffers.size() == 1, "partial-page free preserves an unrelated resource on the same page");
    WorldGeometry_OnResourceMemoryFreed(headers + 256, 1);
    WorldGeometry_OnResourceMemoryFreed(data + 8192, 4096);
    Check(sVertexBuffers.empty() && sVertexPages.empty() && sVertexTokenPages.empty(),
          "separate header and payload frees eventually remove every reverse reference");

    WorldGeometryIndexPlan plan{};
    plan.mpHeader = headers + 512; plan.mpRun = data + 16384;
    plan.muIndexCount = 3;
    RetainedIndexBuffer skip{};
    skip.mpHeader = plan.mpHeader; skip.mpSourceBegin = data + 16384; skip.mpSourceEnd = data + 16390;
    skip.muFirstIndexCount = UINT32_MAX;
    RegisterMirror(sIndexBuffers, sIndexPages, sIndexTokenPages, sIndexReferences, MakeIndexKey(plan), skip);
    WorldGeometry_OnResourceMemoryFreed(data + 16386, 1);
    Check(sIndexBuffers.empty(), "empty-strip marker is invalidated by an overlapping payload free");
    WorldGeometry_OnResourceMemoryFreed(headers + 512, 1);
    Check(sIndexPages.empty() && sIndexTokenPages.empty(), "empty-strip header leaves no reverse references");

    for (unsigned i = 0; i < 1000; ++i) add(8192 + i * 128, 32768 + i * 64);
    sVertexBuffers.rehash(5000);
    Check(sVertexBuffers.size() == 1000, "large owner rehash keeps the registered population");
    for (unsigned i = 0; i < 1000; ++i) WorldGeometry_OnResourceMemoryFreed(headers + 8192 + i * 128, 1);
    Check(sVertexBuffers.empty(), "all owner nodes remain reachable after rehash");
    WorldGeometry_OnResourceMemoryFreed(data, bytes);
    Check(sVertexPages.empty() && sVertexTokenPages.empty(), "all delayed data-page records can be swept");
    const auto last = add(0, 0);
    WorldGeometry_ReleaseAll();
    Check(!sbCompactLifetime || !sVertexReferences.Find(last), "full release retires live token identities");
    const auto next = add(0, 0);
    Check(!sbCompactLifetime || (next != last && !sVertexReferences.Find(last)
                                && sVertexReferences.StorageSlots() == 1000),
          "full-release reuse remains bounded and cannot revive the old identity");
    WorldGeometry_ReleaseAll();
    VirtualFree(headers, 0, MEM_RELEASE); VirtualFree(data, 0, MEM_RELEASE);
    std::printf("PCGeometryLifetime: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
