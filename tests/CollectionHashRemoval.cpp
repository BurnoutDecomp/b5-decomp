// Collision chains must survive removal of their middle, first and last entries.
// The runner extracts the production map methods; only allocation/assertion are mocked.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"
#include <cstdio>
#include <cstdlib>

static int gAllocations = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int)
{
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}
void* EndAssert() { return nullptr; }
} }
namespace CgsAttribSys {
bool AttribSysMemoryManager::HasMemoryBuffer() { return true; }
AttribSysPackageAllocator* AttribSysMemoryManager::GetAttribSysAllocator()
{
    static AttribSysPackageAllocator allocator;
    return &allocator;
}
void* AttribSysPackageAllocator::Malloc(size_t size, int)
{
    ++gAllocations;
    return std::malloc(size);
}
void AttribSysPackageAllocator::Free(void* block, size_t)
{
    --gAllocations;
    std::free(block);
}
}
namespace Attrib {
u32 gTablePolicyFreedBytes = 0;
void* TableFreeFunc(void* block, size_t size)
{
    CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Free(block, size);
    return block;
}
}
static void Check(bool condition)
{
    if (!condition) std::abort();
}
int main()
{
    const int order[] = {3, 0, 7, 2, 5, 1, 6, 4};
    for (int cycle = 0; cycle < 32; ++cycle)
    {
        Attrib::CollectionHashMap map = {};
        map.RebuildTable(17);
        u64 keys[8];
        char objects[8]; // Opaque identities; the map never dereferences collection values.
        bool present[8] = {};
        for (int i = 0; i < 8; ++i)
        {
            // Same low-word home (16), wrapping past bucket zero, with distinct high words.
            keys[i] = (static_cast<u64>(i + cycle + 1) << 32) | (16 + 17 * i);
            Check(map.Add(keys[i], reinterpret_cast<Attrib::Collection*>(&objects[i])));
            present[i] = true;
        }
        for (int victim : order)
        {
            Check(map.RemoveIndex(map.FindIndex(keys[victim])) ==
                  reinterpret_cast<Attrib::Collection*>(&objects[victim]));
            present[victim] = false;
            for (int i = 0; i < 8; ++i)
                Check(map.Find(keys[i]) == (present[i]
                    ? reinterpret_cast<Attrib::Collection*>(&objects[i]) : nullptr));
            Check(map.RemoveIndex(map.FindIndex(keys[victim])) == nullptr);
        }
        map.RebuildTable(0);
    }
    Check(gAllocations == 0);
    std::puts("Collection hash removal regression: PASS");
}
