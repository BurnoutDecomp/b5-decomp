#include <map>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
using u8 = uint8_t;
using u32 = uint32_t;
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace renderengine {
// The cache owns one COM reference. The fixture records releases without a GPU.
struct Declaration { u32 muReleases = 0; void Release() { ++muReleases; } };
struct Vd32Cached { Declaration* mpDeclaration; char macElements[224] = {}; };
std::map<uintptr_t, Vd32Cached> sVdCache;
std::unordered_map<const void*, std::vector<u8>> sVdSourceWitness;
const char* spLastDeclElements = "";
}
#include "restored_methods.inc"
int main()
{
    using namespace renderengine;
    u32 luChecks = 0;
    auto Check = [&](bool lbPass, const char* lpcLabel) {
        ++luChecks;
        if (!lbPass) { std::fprintf(stderr, "FAIL: %s\n", lpcLabel); std::exit(1); }
    };
    u8 laAllocation[256] = {};
    const uintptr_t luBase = reinterpret_cast<uintptr_t>(laAllocation);
    Declaration lBefore, lFirst, lLast, lEnd, lReplacement;
    sVdCache[luBase + 15] = {&lBefore};
    sVdCache[luBase + 16] = {&lFirst};
    sVdCache[luBase + 63] = {&lLast};
    sVdCache[luBase + 64] = {&lEnd};
    sVdSourceWitness[laAllocation + 16] = {1, 2};
    sVdSourceWitness[laAllocation + 64] = {3, 4};
    spLastDeclElements = sVdCache[luBase + 16].macElements;
    WorldVd32_OnResourceMemoryFreed(nullptr, 100);
    WorldVd32_OnResourceMemoryFreed(laAllocation, 0);
    Check(sVdCache.size() == 4, "null and empty notifications preserve live declarations");
    WorldVd32_OnResourceMemoryFreed(laAllocation + 128, 64);
    Check(sVdCache.size() == 4, "unrelated allocation preserves declarations");
    WorldVd32_OnResourceMemoryFreed(laAllocation + 16, 48);
    Check(sVdCache.size() == 2, "only declarations inside the freed resource retire");
    Check(lFirst.muReleases == 1 && lLast.muReleases == 1, "inclusive first and last byte release owned references");
    Check(lBefore.muReleases == 0 && lEnd.muReleases == 0, "preceding byte and exclusive end remain alive");
    Check(sVdSourceWitness.count(laAllocation + 16) == 0 && sVdSourceWitness.count(laAllocation + 64) == 1,
          "diagnostic snapshots follow the same lifetime");
    Check(*spLastDeclElements == '\0', "published diagnostic pointer does not dangle");
    WorldVd32_OnResourceMemoryFreed(laAllocation + 16, 48);
    Check(lFirst.muReleases == 1 && lLast.muReleases == 1, "duplicate notification cannot double-release");
    Check(sVdCache.find(luBase + 16) == sVdCache.end(), "reused descriptor address must rebuild its vertex format");
    sVdCache[luBase + 16] = {&lReplacement};
    spLastDeclElements = sVdCache[luBase + 64].macElements;
    const char* lpcLiveElements = spLastDeclElements;
    WorldVd32_OnResourceMemoryFreed(laAllocation + 16, 1);
    Check(lReplacement.muReleases == 1 && lFirst.muReleases == 1, "replacement owns an independent cache reference");
    Check(spLastDeclElements == lpcLiveElements, "retiring another resource preserves live diagnostic pointer");
    sVdCache[luBase + 100] = {nullptr};
    WorldVd32_OnResourceMemoryFreed(laAllocation + 100, 1);
    Check(sVdCache.count(luBase + 100) == 0, "cached declaration creation failures also retire");
    Declaration lHigh;
    const uintptr_t luHigh = std::numeric_limits<uintptr_t>::max() - 8;
    sVdCache[luHigh] = {&lHigh};
    WorldVd32_OnResourceMemoryFreed(reinterpret_cast<const void*>(luHigh - 8), 32);
    Check(lHigh.muReleases == 1, "range comparison does not wrap at the address-space end");
    WorldVd32_ReleaseAll();
    Check(sVdCache.empty() && sVdSourceWitness.empty() && *spLastDeclElements == '\0', "full teardown clears all cache state");
    Check(lBefore.muReleases == 1 && lEnd.muReleases == 1, "full teardown releases remaining owned references");
    WorldVd32_ReleaseAll();
    Check(lBefore.muReleases == 1 && lEnd.muReleases == 1, "full teardown is idempotent");
    std::printf("PASS: %u vertex declaration lifetime checks\n", luChecks);
}
