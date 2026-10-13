#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/Memory/CgsIOStackLinearMalloc.h"

// The runner exposes private storage only to observe the original ownership and
// last-allocation contracts. All allocation/lifecycle operations use real methods.
static int checks, failures, assertions, begins, ends;
static const char* lastAssertion;
namespace CgsDev { namespace Assert {
int BeginAssert() { ++begins; return 0; }
int FireAssert(const char* expression, const char*, int) {
    ++assertions; lastAssertion = expression; return 0;
}
void* EndAssert() { ++ends; return nullptr; }
} }
static void Check(bool ok, const char* description) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", description); }
}

int main() {
    alignas(128) unsigned char backing[512];
    alignas(128) unsigned char replacement[256];
    std::memset(backing, 0xA7, sizeof(backing));
    CgsMemory::LinearMalloc allocator;
    allocator.mpLastAlloc = replacement;
    allocator.Construct();
    Check(allocator.mpLastAlloc == replacement && allocator.mpAlloc == nullptr,
          "Construct preserves the last-allocation slot while clearing ownership");
    Check(!allocator.mbCreated && allocator.GetStartAddress() == nullptr
          && allocator.GetUsage() == 0 && allocator.GetSize() == 0
          && allocator.GetAlignment() == 4,
          "Construct leaves an empty uncreated region with original alignment");
    allocator.Create(backing + 16, 256);
    Check(assertions == 0 && allocator.mbCreated
          && allocator.GetStartAddress() == backing + 16 && allocator.GetSize() == 256
          && allocator.mpAlloc == nullptr && allocator.mpLastAlloc == replacement,
          "borrowed adoption publishes the region without owning or clearing it");
    allocator.SetAlignment(64);
    auto* first = static_cast<unsigned char*>(allocator.Malloc(65));
    auto* second = static_cast<unsigned char*>(allocator.Malloc(31));
    Check(first == backing + 64 && second == backing + 192
          && allocator.GetUsage() == 207,
          "real allocations align within the adopted region");
    if (first == backing + 64 && second == backing + 192) {
        std::memset(first, 0x31, 65);
        std::memset(second, 0x72, 31);
    }
    bool guards = true;
    for (int i = 0; i < 16; ++i) guards &= backing[i] == 0xA7;
    for (int i = 272; i < 512; ++i) guards &= backing[i] == 0xA7;
    Check(guards && backing[63] == 0xA7 && backing[129] == 0xA7,
          "caller writes preserve external guards and alignment gaps");
    allocator.FreeAll();
    Check(allocator.GetUsage() == 0 && allocator.GetStartAddress() == backing + 16
          && allocator.mpLastAlloc == second && allocator.mpAlloc == nullptr,
          "rewinding keeps the borrowed base and last-allocation state");
    const int beforeRepeat = assertions;
    allocator.Create(replacement, sizeof(replacement));
    Check(assertions == beforeRepeat + 1
          && std::strcmp(lastAssertion, "mbCreated==false") == 0
          && begins == ends && allocator.GetStartAddress() == replacement
          && allocator.GetAlignment() == 4 && allocator.mpAlloc == nullptr
          && allocator.mpLastAlloc == second,
          "repeated Create asserts only created state and continues into new adoption");
    allocator.Construct();
    const int beforeNull = assertions;
    allocator.Create(nullptr, 16);
    Check(assertions == beforeNull + 1 && std::strcmp(lastAssertion, "lpBuffer") == 0
          && begins == ends && allocator.mbCreated
          && allocator.GetStartAddress() == nullptr && allocator.GetSize() == 16,
          "null backing assertion preserves original continuation without dereference");

    CgsMemory::IOStackLinearMalloc<256> io;
    const int beforeIO = assertions;
    io.Construct();
    io.Prepare();
    Check(assertions == beforeIO && io.GetMalloc()->mpAlloc == nullptr
          && io.GetMalloc()->GetStartAddress() == io.maData
          && io.GetMalloc()->Malloc(256) == io.maData,
          "real IO stack consumer borrows its complete embedded arena");
    Check(io.GetMalloc()->Malloc(1) == nullptr
          && io.GetMalloc()->GetStartAddress() == io.maData,
          "overflow leaves the borrowed base available to consumers");
    std::printf("PCLinearMallocBorrowed: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
