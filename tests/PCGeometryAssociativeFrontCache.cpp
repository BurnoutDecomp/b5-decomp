#include "geometry_assoc_cache.inc"
#include <cstdio>

struct Entry { unsigned key, stride, flags, value; };
using Cache = renderengine::GeometryAssociativeFrontCachePC<Entry, 4>;
static unsigned checks, failures;
static void Check(bool value, const char* name) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
static const Entry* Find(const Cache& cache, unsigned long long hash,
                         unsigned long long epoch, unsigned key, unsigned stride=32, unsigned flags=0) {
    return cache.Find(hash, epoch, [=](const Entry& entry) {
        return entry.key == key && entry.stride == stride && entry.flags == flags;
    });
}
int main() {
    Cache cache;
    unsigned predicates=0;
    Check(!cache.Find(0, 0, [&](const Entry&) { ++predicates; return true; }) && !predicates,
          "empty fingerprint never exposes zero-initialized borrowed metadata");
    // Every entry deliberately has the same fingerprint AND set.
    for (unsigned i=0; i<4; ++i) cache.Store(2, 1, Entry{i,32,0,100+i});
    for (unsigned i=0; i<4; ++i) {
        const auto* entry=Find(cache,2,1,i);
        Check(entry && entry->value==100+i, "all four colliding identities remain distinguishable");
    }
    Check(!Find(cache,2,1,4), "fingerprint collision is not an identity match");
    Check(!Find(cache,2,1,0,40), "different vertex stride rejects same fingerprint");
    Check(!Find(cache,2,1,0,32,1), "different index flags reject same fingerprint");
    cache.Store(2,1,Entry{4,32,0,104});
    Check(!Find(cache,2,1,0), "fifth colliding insertion replaces one oldest way");
    for (unsigned i=1; i<=4; ++i) {
        const auto* entry=Find(cache,2,1,i);
        Check(entry && entry->value==100+i, "replacement preserves the other three ways");
    }
    cache.Store((1ull<<32)|2,1,Entry{9,32,0,109});
    Check(Find(cache,(1ull<<32)|2,1,9) && !Find(cache,2,1,9), "equal fingerprints in separate sets do not alias");
    Check(!Find(cache,2,2,1), "retirement epoch rejects previously borrowed buffers");
    cache.Store(2,2,Entry{1,32,0,999});
    const auto* revived=Find(cache,2,2,1);
    Check(revived && revived->value==999, "reused identity publishes the new allocation metadata");
    Check(!Find(cache,2,2,2), "new epoch does not revive other ways from old epoch");
    Check(!Find(cache,2,1,1), "older epoch cannot observe replacement allocation");
    Check(!Find(cache,(1ull<<32)|2,2,9), "unvisited set also rejects previous epoch");
    cache.Store(0,3,Entry{8,32,0,108});
    Check(Find(cache,0,3,8)!=nullptr, "all-zero hash remains a usable identity");
    Check(!Find(cache,1,3,7), "reserved-empty encoding still requires full equality");
    cache.Store(0x80000000u,3,Entry{7,32,0,107});
    Check(Find(cache,0x80000000u,3,7)!=nullptr, "high-bit fingerprints compare as full bit patterns");
    cache.Clear();
    Check(!Find(cache,0,3,8) && !Find(cache,0x80000000u,3,7), "explicit clear invalidates every borrowed value");
    cache.Store(2,1,Entry{1,32,0,200});
    Check(Find(cache,2,1,1)->value==200, "generation reuse after clear cannot recover old allocation");
    std::printf("PCGeometryAssociativeFrontCache: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
