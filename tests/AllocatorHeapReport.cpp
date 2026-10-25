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
    // Observe the real producer through its callback; no allocator/OS body is replaced.
    struct TraceRecords
    {
        char records[8][4000]{};
        size_t count = 0;
        unsigned routes = 0;
        bool bad = false;
    };
    TraceRecords* expectedTrace = nullptr;
    void CaptureTrace(const char* record, void* context, unsigned route)
    {
        if (!expectedTrace || context != expectedTrace) { if (expectedTrace) expectedTrace->bad = true; return; }
        TraceRecords& output = *expectedTrace;
        output.routes |= route;
        if (!record || output.count == 8) { output.bad = true; return; }
        size_t length = 0;
        while (length < sizeof(output.records[0]) && record[length]) ++length;
        if (length == sizeof(output.records[0])) { output.bad = true; return; }
        std::memcpy(output.records[output.count++], record, length + 1);
    }
    void TraceA(const char* record, void* context) { CaptureTrace(record, context, 1); }
    void TraceB(const char* record, void* context) { CaptureTrace(record, context, 2); }
    bool TraceAddress(const char* record, const void* data)
    {
        if (std::strncmp(record, "addr: 0x", 8) != 0) return false;
        for (size_t i = 8; i != 24; ++i)
            if (!((record[i] >= '0' && record[i] <= '9') || (record[i] >= 'a' && record[i] <= 'f'))) return false;
        char* end = nullptr;
        const unsigned long long address = std::strtoull(record + 8, &end, 16);
        return end == record + 24 && *end == '\t' &&
            address == static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(data));
    }
    bool TraceRecord(const char* record, const void* data, unsigned size, const char* preview)
    {
        unsigned decimal = 0, hex = 0;
        char delimiter = 0;
        const size_t length = std::strlen(record);
        return TraceAddress(record, data) &&
            std::sscanf(record + 25, "size: %u (%x)%c", &decimal, &hex, &delimiter) == 3 &&
            decimal == size && hex == size && delimiter == '\t' &&
            std::strstr(record, preview) && length && record[length - 1] == '\n' &&
            !std::strstr(record, "attr:");
    }
#if !defined(ALLOCATOR_TRACE_PREIMAGE_CONTROL)
    struct ReportingProbe : Allocator
    {
        using Allocator::DescribeChunk;
        using Allocator::GetDataPreview;
    };
#endif
    void TraceChecks()
    {
        static_assert(sizeof(void*) == 8 && sizeof(wchar_t) == 2, "production Windows x64 reporting ABI");
        {
            Heap heap;
            std::memset(heap.first, 'A', 64);
            std::memset(heap.second, 'B', 208);
            Allocator::SnapshotImage storage{};
            TraceRecords rows;
            expectedTrace = &rows;
            const size_t freeBefore = heap.allocator.GetLargestFreeBlock(false);
            heap.allocator.TraceAllocatedMemory(TraceA, &rows, &storage, sizeof(storage));
            Check(!rows.bad && rows.count == 2 && rows.routes == 1 &&
                  TraceRecord(rows.records[0], heap.first, 64, "\tdata: 41 41 41 ") &&
                  TraceRecord(rows.records[1], heap.second, 208, "\tdata: 42 42 42 ") &&
                  heap.allocator.GetLargestFreeBlock(false) == freeBefore,
                  "trace caller storage reports real live allocations and context");
            rows = TraceRecords{};
            heap.allocator.TraceAllocatedMemory(TraceA, &rows, nullptr, 0);
            Check(!rows.bad && rows.count == 2 &&
                  TraceAddress(rows.records[0], heap.first) && TraceAddress(rows.records[1], heap.second) &&
                  heap.allocator.GetLargestFreeBlock(false) == freeBefore,
                  "trace owned storage hides its context and restores capacity");
            rows = TraceRecords{};
            heap.allocator.SetTraceFunction(TraceA, &rows);
            heap.allocator.TraceAllocatedMemory(nullptr, nullptr, &storage, sizeof(storage));
            const bool installed = !rows.bad && rows.count == 2 && rows.routes == 1;
            rows = TraceRecords{};
            heap.allocator.TraceAllocatedMemory(TraceB, nullptr, &storage, sizeof(storage));
            const bool independentContext = !rows.bad && rows.count == 2 && rows.routes == 2;
            TraceRecords explicitRows;
            expectedTrace = &explicitRows;
            heap.allocator.TraceAllocatedMemory(nullptr, &explicitRows, &storage, sizeof(storage));
            Check(installed && independentContext && !explicitRows.bad && explicitRows.count == 2 && explicitRows.routes == 1,
                  "trace callback and context defaults remain independent");
            heap.allocator.SetTraceFunction(nullptr, nullptr);
            expectedTrace = nullptr;
        }
        {
            Allocator empty;
            Allocator::SnapshotImage storage{};
            TraceRecords rows;
            expectedTrace = &rows;
            empty.TraceAllocatedMemory(TraceA, &rows, &storage, sizeof(storage));
            const bool emptyValid = !rows.bad && rows.count == 0;
            alignas(16) unsigned char core[1024]{};
            Allocator full;
            if (!full.AddCore(core, sizeof(core))) std::abort();
            void* data = full.Malloc(sizeof(core) - 16);
            if (!data) std::abort();
            full.TraceAllocatedMemory(TraceA, &rows, nullptr, 0);
            Check(emptyValid && !rows.bad && rows.count == 0 && full.GetLargestFreeBlock(false) == 0,
                  "trace empty and exhausted heaps make no callbacks or mutation");
            full.Free(data);
        }
        expectedTrace = nullptr;
#if !defined(ALLOCATOR_TRACE_PREIMAGE_CONTROL)
        {
            ReportingProbe allocator;
            alignas(16) unsigned char core[1024]{};
            if (!allocator.AddCore(core, sizeof(core))) std::abort();
            void* data = allocator.Malloc(64);
            if (!data) std::abort();
            const Records actual = Live(allocator, 2);
            if (actual.count != 1 || actual.blocks[0].mpData != data) std::abort();
            const Allocator::Chunk* token = static_cast<const Allocator::Chunk*>(actual.blocks[0].mpBlock);
            char tiny[] = {'L', 'Q', 'R'};
            const bool one = allocator.DescribeChunk(token, tiny + 1, 1, false) == 0 &&
                tiny[0] == 'L' && tiny[1] == 'Q' && tiny[2] == 'R';
            const bool zero = allocator.DescribeChunk(token, tiny + 1, 0, false) == 0 &&
                tiny[0] == 'L' && tiny[1] == 'Q' && tiny[2] == 'R';
            Check(one && zero, "trace description noappend preserves valid one-byte and zero-byte storage");
            const bool appendedOne = allocator.DescribeChunk(token, tiny + 1, 1, true) == 0 &&
                tiny[0] == 'L' && tiny[1] == 0 && tiny[2] == 'R';
            tiny[1] = 'Q';
            const bool appendedZero = allocator.DescribeChunk(token, tiny + 1, 0, true) == 0 &&
                tiny[0] == 'L' && tiny[1] == 'Q' && tiny[2] == 'R';
            char address[32];
            std::memset(address, 'Z', sizeof(address));
            const bool below = allocator.DescribeChunk(token, address, 27, false) == 0;
            bool untouched = true;
            for (char byte : address) untouched &= byte == 'Z';
            const size_t written = allocator.DescribeChunk(token, address, 28, false);
            Check(appendedOne && appendedZero && below && untouched && written == 25 &&
                  TraceAddress(address, data) && address[25] == 0 && address[26] == 'Z' && address[28] == 'Z',
                  "trace description bounds append and native address fields without overwriting canaries");
            const unsigned char sample[] = {0x20, 0x41, 0x22, 0x27, 0x7f, 0x80};
            char narrow[26]; wchar_t wide[26];
            const char expected[] = "20 41 22 27 7f 80\t A.... ";
            const wchar_t expectedWide[] = {L'2',L'0',0x2020,L'4',L'1',0x2020,L'2',L'2',0x2020,
                L'2',L'7',0x2020,L'7',L'f',0x2020,L'8',L'0',L'\t',L' ',L'A',L'.',L'.',0x7f,L'.',0x2020,0};
            static_assert(sizeof(expected) == sizeof(narrow) && sizeof(expectedWide) == sizeof(wide), "literal primary preview witnesses");
            const auto type = allocator.GetDataPreview(sample, sizeof(sample), narrow, wide, 26);
            Check(type == Allocator::kDataTypeBinary && !std::memcmp(narrow, expected, sizeof(narrow)) &&
                  !std::memcmp(wide, expectedWide, sizeof(wide)),
                  "trace preview preserves primary narrow and wide hex printable padding and DEL contracts");
            bool shortValid = true;
            const size_t capacities[] = {0, 1, 4, 5, 6};
            for (size_t capacity : capacities)
            {
                char bytes[8]; wchar_t chars[8];
                std::memset(bytes, 'Z', sizeof(bytes));
                for (wchar_t& value : chars) value = 0x5a5a;
                const size_t dataSize = capacity == 6 ? 0 : sizeof(sample);
                const auto result = allocator.GetDataPreview(sample, dataSize, bytes + 1, chars + 1, capacity);
                shortValid &= result == Allocator::kDataTypeBinary;
                for (size_t i = 0; i != 8; ++i)
                {
                    shortValid &= bytes[i] == (capacity && i == 1 ? 0 : 'Z');
                    shortValid &= chars[i] == (capacity && i == 1 ? 0 : 0x5a5a);
                }
            }
            Check(shortValid, "trace preview zero and short capacities preserve guarded caller storage");
        }
#endif
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
    TraceChecks();
    std::printf("AllocatorHeapReport: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
