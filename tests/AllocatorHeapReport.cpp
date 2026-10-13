// Exercise the complete production allocator and its reporting lifecycle.
#include "ppmalloc/EAGeneralAllocator.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using Allocator = EA::Allocator::GeneralAllocator;
using Block = Allocator::BlockInfo;

namespace
{
    int checks = 0;
    int failures = 0;
    void Check(bool value, const char* label)
    {
        ++checks;
        if (!value) ++failures;
        std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
    }
    struct Records
    {
        Block blocks[64]{};
        size_t count = 0;
        bool keepGoing = true;
    };
    bool Collect(const Block* block, void* context)
    {
        Records& result = *static_cast<Records*>(context);
        if (result.count == 64) std::abort();
        result.blocks[result.count++] = *block;
        return result.keepGoing;
    }
    struct Heap
    {
        alignas(16) unsigned char core[16384]{};
        Allocator allocator;
        void* first;
        void* second;
        Heap()
        {
            if (!allocator.AddCore(core, sizeof(core))) std::abort();
            first = allocator.Malloc(64);
            second = allocator.Malloc(208);
            if (!first || !second) std::abort();
        }
    };
    Records Live(Allocator& allocator, int mask)
    {
        Allocator::SnapshotImage storage{};
        Records result;
        if (!allocator.ReportHeap(Collect, &result, mask, false, &storage, sizeof(storage)))
            std::abort();
        return result;
    }
}

int main()
{
    {
        Allocator empty;
        Allocator::SnapshotImage storage{};
        Records rows;
        Check(!empty.ReportHeap(nullptr, &rows, 31, false, &storage, sizeof(storage)),
              "null callback rejects report");
        Check(empty.ReportHeap(Collect, &rows, 31, false, &storage, sizeof(storage)) && rows.count == 0,
              "valid empty live report succeeds without callbacks");
    }
    {
        Heap heap;
        const Records rows = Live(heap.allocator, 31);
        bool valid = rows.count == 3;
        size_t span = 0;
        const unsigned char* cursor = heap.core;
        for (size_t i = 0; i < rows.count; ++i)
        {
            const Block& block = rows.blocks[i];
            valid &= block.mpCore == heap.core && block.mpBlock == cursor && !block.mbMemoryMapped;
            valid &= block.mpData == cursor + 16 && block.mnDataSize + 16 == block.mnBlockSize;
            valid &= block.mBlockType == (i < 2 ? 2 : 4);
            cursor += block.mnBlockSize;
            span += block.mnBlockSize;
        }
        Check(valid && span == sizeof(heap.core) && cursor == heap.core + sizeof(heap.core) &&
              rows.blocks[0].mpData == heap.first && rows.blocks[0].mnDataSize == 64 &&
              rows.blocks[1].mpData == heap.second && rows.blocks[1].mnDataSize == 208,
              "live records describe actual ordered allocations and the complete adopted core");
        const Records used = Live(heap.allocator, 2);
        Check(used.count == 2 && used.blocks[0].mpData == heap.first && used.blocks[1].mpData == heap.second,
              "allocated filter excludes free blocks");
        const Records free = Live(heap.allocator, 4);
        Check(free.count == 1 && free.blocks[0].mBlockType == 4 &&
              free.blocks[0].mnDataSize == heap.allocator.GetLargestFreeBlock(false),
              "free record capacity agrees with allocatable free memory");
        Check(Live(heap.allocator, 8).count == 0, "live walk does not synthesize copied-only core records");
    }
    {
        Heap heap;
        Allocator::SnapshotImage storage{};
        const void* report = heap.allocator.ReportBegin(nullptr, 2, false, &storage, sizeof(storage));
        const Block* rejected = heap.allocator.ReportNext(report, 4);
        Check(report && rejected == nullptr, "live report intersects captured and per-call masks");
        heap.allocator.ReportEnd(report);
    }
    {
        Heap heap;
        alignas(16) unsigned char storage[4096]{};
        Records rows;
        const bool ok = heap.allocator.ReportHeap(Collect, &rows, 31, true, storage, sizeof(storage));
        Check(ok && rows.count == 4 && rows.blocks[0].mBlockType == 8 &&
              rows.blocks[0].mpCore == heap.core && rows.blocks[0].mpBlock == heap.core &&
              rows.blocks[0].mnBlockSize == sizeof(heap.core) && rows.blocks[0].mpData == nullptr &&
              rows.blocks[0].mnDataSize == 0 && rows.blocks[1].mpData == heap.first &&
              rows.blocks[2].mpData == heap.second && rows.blocks[3].mBlockType == 4,
              "copied report emits its real core before ordered chunk records");
    }
    {
        Heap heap;
        alignas(16) unsigned char storage[4096];
        std::memset(storage, 0xA5, sizeof(storage));
        void* snapshot = heap.allocator.TakeSnapshot(31, true, storage, sizeof(storage));
        if (!snapshot) std::abort();
        heap.allocator.Free(heap.first);
        const void* report = heap.allocator.ReportBegin(snapshot, 31, true, nullptr, 0);
        const Block* beforeFree = heap.allocator.ReportNext(report, 2);
        bool stable = beforeFree && beforeFree->mpData == heap.first && beforeFree->mBlockType == 2;
        heap.allocator.ReportEnd(report);
        const Block* firstAgain = heap.allocator.ReportNext(snapshot, 31);
        Check(stable && firstAgain && firstAgain->mBlockType == 8 &&
              Live(heap.allocator, 2).count == 1 && storage[4095] == 0xA5,
              "copied snapshot survives heap changes and borrowed report end resets its index");
        heap.allocator.FreeSnapshot(snapshot);
        Check(storage[4095] == 0xA5 && heap.allocator.ValidateHeap(Allocator::kHeapValidationLevelFull),
              "user snapshot storage remains caller-owned");
    }
    {
        Heap heap;
        const size_t freeBefore = heap.allocator.GetLargestFreeBlock(false);
        Records rows;
        const bool ok = heap.allocator.ReportHeap(Collect, &rows, 2, false, nullptr, 0);
        Check(ok && rows.count == 2 && heap.allocator.GetLargestFreeBlock(false) == freeBefore,
              "owned live context is hidden from allocated-only reports and freed afterwards");
        const void* report = heap.allocator.ReportBegin(nullptr, 18, false, nullptr, 0);
        bool sawContext = false;
        size_t count = 0;
        for (const Block* block = heap.allocator.ReportNext(report, 18); block;
             block = heap.allocator.ReportNext(report, 18))
        {
            ++count;
            if (block->mpData == report)
                sawContext = block->mBlockType == 16 && block->mnDataSize >= sizeof(Allocator::Snapshot);
        }
        heap.allocator.ReportEnd(report);
        Check(sawContext && count == 3 && heap.allocator.GetLargestFreeBlock(false) == freeBefore,
              "internal-inclusive report exposes the actual owned context allocation");
        rows = Records{};
        Check(heap.allocator.ReportHeap(Collect, &rows, 16, false, nullptr, 0) && rows.count == 0,
              "internal bit alone does not select allocations");
        heap.allocator.Free(heap.first);
        heap.allocator.Free(heap.second);
        Check(heap.allocator.GetLargestFreeBlock(false) == sizeof(heap.core) - 16,
              "report cleanup preserves existing free coalescing");
    }
    {
        Heap heap;
        alignas(16) unsigned char small[16];
        std::memset(small, 0x37, sizeof(small));
        const size_t freeBefore = heap.allocator.GetLargestFreeBlock(false);
        const void* report = heap.allocator.ReportBegin(nullptr, 31, false, small, sizeof(small));
        bool untouched = true;
        for (unsigned char byte : small) untouched &= byte == 0x37;
        const bool fallback = report && report != small && untouched;
        heap.allocator.ReportEnd(report);
        Check(fallback && heap.allocator.GetLargestFreeBlock(false) == freeBefore,
              "undersized supplied storage falls back to owned context without touching the buffer");
    }
    {
        Heap heap;
        alignas(16) unsigned char storage[4096]{};
        Records rows;
        rows.keepGoing = false;
        const bool ok = heap.allocator.ReportHeap(Collect, &rows, 31, true, storage, sizeof(storage));
        const Allocator::Snapshot* snapshot = reinterpret_cast<const Allocator::Snapshot*>(storage);
        Check(!ok && rows.count == 1 && snapshot->mnBlockInfoIndex == 2,
              "false callback stops callbacks but retains ARTIST's extra iterator advance");
    }
    {
        alignas(16) unsigned char core[1024]{};
        Allocator full;
        if (!full.AddCore(core, sizeof(core))) std::abort();
        void* data = full.Malloc(sizeof(core) - 16);
        if (!data) std::abort();
        Records rows;
        Check(!full.ReportHeap(Collect, &rows, 31, false, nullptr, 0) && rows.count == 0 &&
              full.GetLargestFreeBlock(false) == 0,
              "context allocation failure rejects report without callbacks or heap mutation");
        full.Free(data);
    }
    {
        Allocator allocator;
        Allocator::SnapshotImage invalid{};
        Check(!allocator.ReportNext(nullptr, 31) && !allocator.ReportNext(&invalid, 31),
              "iterator rejects null or invalid context");
    }
    std::printf("AllocatorHeapReport: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
