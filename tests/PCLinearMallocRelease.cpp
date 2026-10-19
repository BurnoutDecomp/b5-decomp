#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"

// Private state is injected only to cover the attested defensive release/Stop
// branches. This does not claim an original owning producer or active RW caller.
using CgsMemory::LinearMalloc;
static int checks, failures, assertions, begins, ends, assertionLine;
static const char* assertionText;
static int goodFrees, wrongFrees;
static LinearMalloc* observedAllocator;
static void* expectedOwned;
static bool releaseOrdering;
namespace CgsDev { namespace Assert {
int BeginAssert() { ++begins; return 0; }
int FireAssert(const char* expression, const char*, int line) {
    ++assertions; assertionText = expression; assertionLine = line; return 0;
}
void* EndAssert() { ++ends; return nullptr; }
} }
static void Check(bool ok, const char* description) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", description); }
}
static void* RecordedMalloc(size_t size) { return std::malloc(size); }
static void* RecordedRealloc(void* pointer, size_t size) { return std::realloc(pointer, size); }
static void RestoredFree(void* pointer) {
    ++goodFrees;
    releaseOrdering = observedAllocator && LinearMalloc::mpActiveRWAllocator == nullptr
        && observedAllocator->mpAlloc == expectedOwned
        && observedAllocator->mbCreated && pointer == expectedOwned;
    std::free(pointer);
}
static void WrongFree(void* pointer) { ++wrongFrees; std::free(pointer); }
static bool Same(const RwMemoryFunctions& a, const RwMemoryFunctions& b) {
    return a.rwmalloc == b.rwmalloc && a.rwrealloc == b.rwrealloc && a.rwfree == b.rwfree;
}
static bool IsCRT() {
    return rwMemoryFunctions.rwmalloc == std::malloc
        && rwMemoryFunctions.rwrealloc == std::realloc
        && rwMemoryFunctions.rwfree == std::free;
}

int main() {
    const RwMemoryFunctions lazy = rwMemoryFunctions;
    Check(lazy.rwmalloc == RwMallocMemStub && lazy.rwrealloc == RwReallocMemStub
          && lazy.rwfree == RwFreeMemStub,
          "canonical global object initially contains all three real lazy entries");
    void* memory = rwMemoryFunctions.rwmalloc(64);
    Check(memory != nullptr, "lazy malloc returns real CRT backing");
    Check(IsCRT(), "malloc entry installs the complete CRT table");
    if (memory) std::memset(memory, 0x6B, 64);
    rwMemoryFunctions = lazy;
    void* resized = rwMemoryFunctions.rwrealloc(memory, 128);
    Check(resized != nullptr, "lazy realloc forwards the real pointer and new size");
    bool prefix = resized != nullptr;
    if (resized) for (int i = 0; i < 64; ++i)
        prefix &= static_cast<unsigned char*>(resized)[i] == 0x6B;
    Check(prefix, "real realloc retains the previous allocation bytes");
    Check(IsCRT(), "realloc entry installs all three CRT slots");
    rwMemoryFunctions = lazy;
    rwMemoryFunctions.rwfree(resized ? resized : memory);
    Check(IsCRT(), "free entry releases real backing and installs all three CRT slots");

    const RwMemoryFunctions restored = {RecordedMalloc, RecordedRealloc, RestoredFree};
    const RwMemoryFunctions wrong = {RecordedMalloc, RecordedRealloc, WrongFree};
    alignas(128) unsigned char backing[128];
    std::memset(backing, 0xA5, sizeof(backing));
    LinearMalloc borrowed = {}, other = {};
    borrowed.Construct(); other.Construct();
    borrowed.Create(backing, sizeof(backing)); borrowed.SetAlignment(64);
    void* last = borrowed.Malloc(8);
    rwMemoryFunctions = wrong;
    LinearMalloc::mpActiveRWAllocator = &other;
    borrowed.Destruct();
    Check(goodFrees == 0 && wrongFrees == 0 && backing[0] == 0xA5 && backing[127] == 0xA5,
          "borrowed destruction releases no external backing");
    Check(LinearMalloc::mpActiveRWAllocator == &other && Same(rwMemoryFunctions, wrong),
          "destroying an inactive allocator preserves another association and table");
    Check(!borrowed.mbCreated && borrowed.GetStartAddress() == nullptr
          && borrowed.GetSize() == 0 && borrowed.GetUsage() == 0
          && borrowed.GetAlignment() == 64 && borrowed.mpLastAlloc == last,
          "borrowed destruction clears region/created while preserving alignment and last");

    LinearMalloc owned = {};
    owned.Construct();
    void* allocation = std::malloc(64);
    if (!allocation) return 2;
    owned.Create(allocation, 64); owned.SetAlignment(64);
    owned.mpLastAlloc = backing;
    owned.mpAlloc = allocation; // Fixture-only defensive-branch state, not a producer.
    observedAllocator = &owned; expectedOwned = allocation;
    LinearMalloc::mCachedMemoryFuncs = restored;
    LinearMalloc::mpActiveRWAllocator = &owned;
    rwMemoryFunctions = wrong;
    owned.Destruct();
    Check(releaseOrdering && goodFrees == 1 && wrongFrees == 0,
          "Destruct stops before reading current rwfree and clears owned only after release");
    Check(LinearMalloc::mpActiveRWAllocator == nullptr && Same(rwMemoryFunctions, restored),
          "Destruct restores every cached slot before releasing owned backing");
    Check(owned.mpAlloc == nullptr, "owned pointer is cleared after its single release");
    Check(!owned.mbCreated && owned.GetStartAddress() == nullptr
          && owned.GetSize() == 0 && owned.GetUsage() == 0,
          "owned destruction clears created and complete region state");
    Check(owned.GetAlignment() == 64 && owned.mpLastAlloc == backing,
          "owned destruction preserves original alignment and last-allocation fields");
    owned.Destruct();
    Check(goodFrees == 1 && wrongFrees == 0, "repeated destruction never releases backing twice");

    LinearMalloc::mpActiveRWAllocator = &other;
    LinearMalloc::mCachedMemoryFuncs = restored;
    rwMemoryFunctions = wrong;
    borrowed.StopRWAllocation();
    Check(assertions == 1 && std::strcmp(assertionText, "IsRWAllocator()") == 0
          && assertionLine == 245 && begins == ends,
          "mismatched Stop emits the exact original non-aborting assertion");
    Check(LinearMalloc::mpActiveRWAllocator == nullptr && Same(rwMemoryFunctions, restored),
          "mismatched Stop continues into complete cached-table restoration");
    LinearMalloc::mCachedMemoryFuncs = {};
    rwMemoryFunctions = wrong;
    borrowed.StopRWAllocation();
    Check(assertions == 2 && begins == ends
          && LinearMalloc::mpActiveRWAllocator == nullptr
          && !rwMemoryFunctions.rwmalloc && !rwMemoryFunctions.rwrealloc && !rwMemoryFunctions.rwfree,
          "zero cache is restored verbatim without an invented fallback");
    rwMemoryFunctions = lazy;
    borrowed.Create(backing, sizeof(backing));
    Check(assertions == 2 && borrowed.GetStartAddress() == backing && borrowed.mpAlloc == nullptr,
          "real borrowed lifecycle can adopt again after original Destruct clearing");
    std::printf("PCLinearMallocRelease: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
