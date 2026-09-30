// Production loader with deterministic file/heap/fixup boundaries. No GPU or game runtime.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"

#undef CGS_ASSERT
#define CGS_ASSERT(c, m) do { if (!(c)) { std::printf("ASSERT %s\n", m); std::abort(); } } while (0)
namespace CgsDev {
struct Message { static const int gxMessageFilterFlags = 0; };
struct TestLog { template<class T> TestLog& operator<<(const T&) { return *this; } };
namespace Log { static TestLog sink; static TestLog* gpDebugPrint = &sink; }
}

using namespace CgsResource;
static std::map<std::string, std::vector<char>> files;
static int reads;
static std::vector<char> fixups;
static std::string Key(const char* name) {
    std::string key(name);
    for (char& c : key) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return key;
}

struct FixturePool : Pool {
    Entry entries[64] = {};
    s16 refs[64] = {};
    u8 statuses[64] = {};
    int allocations = 0, failAt = -1;
    ECreateResult failure = CREATERESULT_OUTOFMEMORY;
    explicit FixturePool(int capacity = 64) {
        mpResourceEntries = entries; mpiResourceRefCounts = refs; mpx8ResourceStatuses = statuses;
        muMaxResources = static_cast<u16>(capacity); miNumDependencies = 0; mbIsValid = true;
    }
    ~FixturePool() { Reset(); }
    void Reset() {
        for (u32 i = 0; i < muMaxResources; ++i) {
            for (void*& ptr : entries[i].mResource.m_baseResources) { std::free(ptr); ptr = nullptr; }
            statuses[i] = 0; refs[i] = 0;
        }
    }
    int Used() const { int count = 0; for (u32 i = 0; i < muMaxResources; ++i) count += statuses[i] != 0; return count; }
    int Slot(u64 hash) { ID id; id.SetHash(hash); return FindResourceIndex(id, true, 3); }
    int Refs(u64 hash) { int slot = Slot(hash); return slot < 0 ? 0 : refs[slot]; }
};

namespace CgsResource {
static char* ReadBundleFile(const char* name, long* size) {
    ++reads;
    auto found = files.find(Key(name));
    if (found == files.end()) return nullptr;
    *size = static_cast<long>(found->second.size());
    char* result = static_cast<char*>(std::malloc(found->second.size()));
    std::memcpy(result, found->second.data(), found->second.size());
    return result;
}
u32 Pool::GetHeapAlignment(s32) const { return 16; }
s32 Pool::FindResourceIndex(ID id, bool allowUnreferenced, u16 mask) {
    for (u32 i = 0; i < muMaxResources; ++i)
        if ((mpx8ResourceStatuses[i] & mask) && mpResourceEntries[i].mID == id
            && (allowUnreferenced || mpiResourceRefCounts[i] > 0)) return i;
    return -1;
}
Pool::ECreateResult Pool::CreateEntry(const NewResource* resource, Entry** entry, s32* slot, bool allocate) {
    auto* fixture = static_cast<FixturePool*>(this);
    CGS_ASSERT(allocate, "fixture requires allocated entries");
    if (++fixture->allocations == fixture->failAt) return fixture->failure;
    for (u32 i = 0; i < muMaxResources; ++i) {
        if (mpx8ResourceStatuses[i]) continue;
        Entry& out = mpResourceEntries[i];
        out.mID = resource->mID; out.mpResourceType = resource->mpResourceType;
        out.mResourceDescriptor = resource->mResourceDescriptor;
        out.muImportTableOffset = resource->muImportTableOffset;
        for (int t = 0; t < 3; ++t) {
            u32 size = resource->mResourceDescriptor.m_baseResourceDescriptors[t].m_size;
            out.mResource.m_baseResources[t] = size ? std::calloc(1, size) : nullptr;
        }
        mpx8ResourceStatuses[i] = 1; mpiResourceRefCounts[i] = 1;
        *entry = &out; *slot = i;
        return CREATERESULT_OK;
    }
    return CREATERESULT_OUTOFENTRIES;
}
bool Pool::RemoveReference(u32 slot) {
    CGS_ASSERT(slot < muMaxResources && mpx8ResourceStatuses[slot], "release live slot");
    if (--mpiResourceRefCounts[slot] > 0) return false;
    for (void*& ptr : mpResourceEntries[slot].mResource.m_baseResources) { std::free(ptr); ptr = nullptr; }
    mpx8ResourceStatuses[slot] = 0;
    return true;
}
void Pool::FixUpEntry(Entry*) { fixups.push_back('F'); }
bool Pool::ResolveImportsForEntry(s32) { fixups.push_back('I'); return true; }
void Pool::PostFixUpEntry(Entry*) { fixups.push_back('P'); }
}
#include "pc_bundle_ownership.inc"

struct MemberType : Type { u32 GetTypeID() const override { return 123; } };
static MemberType memberType;
static const Type* Resolve(u32) { return &memberType; }
static u64 ListId(const char* name) {
    return static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(name))) | 0x8000000000000000ull;
}
static void File(const char* name, std::initializer_list<u64> ids) {
    const size_t bytes = sizeof(BundleV2) + ids.size() * sizeof(BundleV2::ResourceEntry) + ids.size() * 48;
    std::vector<char>& file = files[Key(name)]; file.assign(bytes, 0);
    auto* header = reinterpret_cast<BundleV2*>(file.data());
    header->muVersion = BundleV2::KU_VERSION; header->muPlatform = BundleV2::KU_PLATFORM;
    header->muResourceEntriesCount = static_cast<u32>(ids.size()); header->muResourceEntriesOffset = sizeof(BundleV2);
    auto* entries = reinterpret_cast<BundleV2::ResourceEntry*>(file.data() + sizeof(BundleV2));
    u32 i = 0;
    for (u64 id : ids) {
        entries[i].mResourceId.SetHash(id); entries[i].muResourceTypeId = 123;
        for (u32 t = 0; t < 3; ++t) {
            header->mauResourceDataOffset[t] = static_cast<u32>(sizeof(BundleV2) + ids.size() * sizeof(*entries) + t * ids.size() * 16);
            entries[i].mauUncompressedSizeAndAlignment[t] = 0x40000010;
            entries[i].mauDiskOffset[t] = i * 16;
            std::memset(file.data() + header->mauResourceDataOffset[t] + i * 16, static_cast<int>(id + t), 16);
        }
        ++i;
    }
}
static int checks, failures;
static void Check(bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }

int main() {
    BundleLoader loader;
    {
        FixturePool pool;
        File("a.bundle", {101, 102}); fixups.clear();
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) == 2, "load two members");
        Check(pool.Used() == 3 && pool.Refs(ListId("a.bundle")) == 1, "pool owns a type-29 list");
        Check(std::string(fixups.begin(), fixups.end()) == "FFIIPP", "all fixups precede imports and post-fixups");
        int slot = pool.Slot(101);
        Check(slot >= 0 && *static_cast<u8*>(pool.entries[slot].mResource.m_baseResources[2]) == 103, "all memory types copied");
        int listSlot = pool.Slot(ListId("a.bundle"));
        Check(listSlot >= 0 && pool.entries[listSlot].mpResourceType->GetCachedId() == 29, "list type cache initialized");
        files.erase("a.bundle"); int before = reads;
        Check(loader.UnloadBundle("A.BUNDLE", &pool) == 2, "unload succeeds after file disappears, case insensitive");
        Check(reads == before && pool.Used() == 0, "unload performs no IO and releases its list last");
        Check(loader.UnloadBundle("a.bundle", &pool) < 0 && pool.Used() == 0, "double unload cannot release another resource");
    }
    {
        FixturePool pool;
        File("a.bundle", {101, 102}); File("b.bundle", {102, 103});
        loader.LoadBundle("a.bundle", &pool, Resolve); fixups.clear();
        Check(loader.LoadBundle("A.BUNDLE", &pool, Resolve) == 0 && fixups.empty(), "duplicate low-level load only references existing members");
        Check(pool.Refs(101) == 2 && pool.Refs(ListId("a.bundle")) == 2, "duplicate owns matching member and list references");
        Check(loader.LoadBundle("b.bundle", &pool, Resolve) == 1 && pool.Refs(102) == 3, "overlapping bundles share member");
        Check(loader.UnloadBundle("a.bundle", &pool) == 0 && pool.Refs(101) == 1, "first duplicate unload retains members");
        Check(loader.UnloadBundle("a.bundle", &pool) == 1 && pool.Refs(102) == 1, "final duplicate unload retains other bundle's member");
        Check(loader.UnloadBundle("b.bundle", &pool) == 2 && pool.Used() == 0, "last bundle frees remaining members and metadata");
    }
    {
        FixturePool dependency, pool;
        pool.miNumDependencies = 1; pool.mapDependencies[0] = &dependency;
        File("a.bundle", {201}); loader.LoadBundle("a.bundle", &dependency, Resolve);
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) == 0, "dependency supplies member");
        Check(pool.Used() == 1 && dependency.Refs(201) == 2, "each pool owns its own list, members use dependency refs");
        Check(loader.UnloadBundle("a.bundle", &pool) == 0 && dependency.Refs(201) == 1 && pool.Used() == 0, "unload releases dependency ref without freeing parent");
        File("unknown.bundle", {201}); int before = reads;
        Check(loader.UnloadBundle("unknown.bundle", &dependency) < 0 && dependency.Refs(201) == 1, "on-disk unrelated bundle cannot release a resident member");
        Check(reads == before, "unknown unload does not read file");
        loader.UnloadBundle("a.bundle", &dependency);
    }
    for (int failureKind = 0; failureKind < 2; ++failureKind) {
        FixturePool dependency, pool;
        pool.miNumDependencies = 1; pool.mapDependencies[0] = &dependency;
        File("base.bundle", {301, 302}); loader.LoadBundle("base.bundle", &dependency, Resolve);
        dependency.refs[dependency.Slot(302)] = -2; // revivable entry: rollback must restore exactly
        File("fail.bundle", {301, 302, 303, 304});
        pool.failure = failureKind ? Pool::CREATERESULT_OUTOFENTRIES : Pool::CREATERESULT_OUTOFMEMORY;
        pool.failAt = 3; fixups.clear(); // list and first new member succeed, next fails
        Check(loader.LoadBundle("fail.bundle", &pool, Resolve) < 0, "partial allocation reports failure");
        Check(pool.Used() == 0 && dependency.Refs(301) == 1 && dependency.Refs(302) == -2, "failure rolls back new entries and exact dependency counts");
        Check(fixups.empty(), "failed allocation runs no fixups");
        int before = reads;
        Check(loader.UnloadBundle("fail.bundle", &pool) < 0 && reads == before && dependency.Refs(301) == 1, "failed bundle has no releasable ownership");
        pool.failAt = -1;
        Check(loader.LoadBundle("fail.bundle", &pool, Resolve) == 2 && dependency.Refs(302) == 1, "retry revives and loads normally");
        Check(loader.UnloadBundle("fail.bundle", &pool) == 3 && pool.Used() == 0, "retry owns exactly one acquisition");
    }
    {
        FixturePool dependency, pool;
        pool.miNumDependencies = 1; pool.mapDependencies[0] = &dependency;
        File("base.bundle", {351}); loader.LoadBundle("base.bundle", &dependency, Resolve);
        File("listfail.bundle", {351}); pool.failAt = 1;
        Check(loader.LoadBundle("listfail.bundle", &pool, Resolve) < 0 && pool.Used() == 0
              && dependency.Refs(351) == 1, "list allocation failure leaves dependencies unchanged");
        pool.failAt = -1; File("untyped.bundle", {352}); fixups.clear();
        Check(loader.LoadBundle("untyped.bundle", &pool, nullptr) == 1 && fixups.empty(), "unknown type still copies without fixups");
        Check(loader.UnloadBundle("untyped.bundle", &pool) == 1 && pool.Used() == 0, "unknown type still has complete unload ownership");
    }
    {
        FixturePool pool(1); File("capacity.bundle", {401});
        Check(loader.LoadBundle("capacity.bundle", &pool, Resolve) < 0 && pool.Used() == 0, "list slot cannot turn capacity failure into partial success");
        File("empty.bundle", {});
        Check(loader.LoadBundle("empty.bundle", &pool, Resolve) == 0 && pool.Used() == 1, "empty bundle owns an empty list");
        Check(loader.UnloadBundle("empty.bundle", &pool) == 0 && pool.Used() == 0, "empty bundle unload releases list");
    }
    {
        FixturePool pool; File("a.bundle", {501}); loader.LoadBundle("a.bundle", &pool, Resolve);
        File("a.bundle", {501, 502});
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) < 0 && pool.Refs(501) == 1, "changed resident member count rejected without mutations");
        File("a.bundle", {502});
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) < 0 && pool.Refs(501) == 1 && pool.Refs(502) == 0, "changed resident member list rejected without mutations");
        Check(loader.UnloadBundle("a.bundle", &pool) == 1 && pool.Used() == 0, "unload uses original IDs after file changes");
        loader.LoadBundle("a.bundle", &pool, Resolve); pool.Reset();
        Check(loader.UnloadBundle("a.bundle", &pool) < 0, "pool reset has no stale external metadata");
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) == 1 && pool.Refs(502) == 1, "pool reuse starts fresh ownership");
    }
    {
        FixturePool pool; File("collision.bundle", {ListId("a.bundle")});
        loader.LoadBundle("collision.bundle", &pool, Resolve); File("a.bundle", {551});
        Check(loader.LoadBundle("a.bundle", &pool, Resolve) < 0 && loader.UnloadBundle("a.bundle", &pool) < 0
              && pool.Refs(ListId("a.bundle")) == 1, "wrong-type ID collision cannot become bundle ownership");
    }
    {
        FixturePool pool; File("bad.bundle", {601});
#ifndef PC_BUNDLE_SKIP_MALFORMED
        auto* header = reinterpret_cast<BundleV2*>(files["bad.bundle"].data());
        header->muResourceEntriesCount = 0xFFFFFFFFu;
        Check(loader.LoadBundle("bad.bundle", &pool, Resolve) < 0 && pool.Used() == 0, "invalid entry count rejected before mutation");
        File("bad.bundle", {601});
        header = reinterpret_cast<BundleV2*>(files["bad.bundle"].data()); header->mauResourceDataOffset[0] = 0xFFFFFFF0u;
        Check(loader.LoadBundle("bad.bundle", &pool, Resolve) < 0 && pool.Used() == 0, "invalid copy range rejected before mutation");
#endif
        Check(loader.LoadBundle("missing.bundle", &pool, Resolve) == BundleLoader::KI_LOAD_FILE_MISSING, "missing-file return preserved");
    }
    std::printf("PCBundleOwnership: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
