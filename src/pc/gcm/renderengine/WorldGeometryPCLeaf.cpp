#include "pc/gcm/renderengine/TextureUploadPCLeaf.h"
#include "pc/gcm/renderengine/GeometryBindingsPCLeaf.h"
#include "pc/gcm/renderengine/PackedNormalPCLeaf.h"
#include "pc/gcm/renderengine/IndexRangePCLeaf.h"
// =============================================================================
// WorldGeometryPCLeaf.cpp  (pc/gcm/renderengine)
//
// FLAG PC-platform leaf: RETAINED D3D9 MIRRORS OF THE STATIC WORLD GEOMETRY.
// The full rationale is in the header banner. In one paragraph: on the console the
// bundle's serialised IndexBuffer/VertexBuffer bytes ARE the GPU's memory and the
// draw binds them directly, so nothing is ever re-processed per draw; on PC D3D9
// cannot bind host memory, and the two Xenos vertex-fetch features the data relies
// on (DEC3N packed normals, primitive reset) have no D3D9 runtime equivalent at all.
// Doing that conversion inside the submit -- which is what the DrawIndexedPrimitiveUP
// path in XenonD3D9Shims.cpp did -- redid it for every one of the ~3,800 draws a
// frame. This TU does it once per (buffer, plan) pair and keeps the result in a
// device buffer.
//
// NOTHING HERE CHANGES WHAT IS DRAWN. The DEC3N expansion preserves the values of
// the per-draw loop it replaces, and the strip re-cut emits exactly the triangles (and
// the winding, and the degenerate-stitch skips) that ExpandStripRunsToList emits.
//
// SCOPE: only the DISPATCH publisher's geometry is mirrored -- the serialised bundle
// buffers bound by shadow::Device::SetMeshBuffersPC, which are static for as long as
// their resource is resident. The Xenon FAST-SET publisher's streams (sky dome,
// immediate mode, particles, MeshHelper) are rewritten every frame and stay on the
// UP path; see the routing in WorldDraw_IndexedUP.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Development/BrnDiagBoundSurfaces.h"  // [diag] BrnDiag::LogBoundSurfaces
#include "pc/gcm/renderengine/WorldGeometryPCLeaf.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/GeometryBufferPoolD3D9PCLeaf.h"
#include "pc/gcm/renderengine/GeometryEntryReferencesPCLeaf.h"
#include "pc/gcm/renderengine/GeometryAssociativeFrontCachePCLeaf.h"
#include "pc/gcm/renderengine/device.h"                    // renderengine::gDevice
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::WriteToLog

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace renderengine
{
namespace
{
    using GeometryPoolType = PCGeometryBufferPool<D3D9GeometryPoolBackend>;
    // FLAG PC-platform leaf: share native buffers across more retained meshes
    // to avoid creation stalls and stream/index binding changes. The 8 MiB
    // default is a measured PC policy; bounded smaller sizes retain A/B controls.
    GeometryPoolType sGeometryPool([] {
        const char* lpcValue = std::getenv("BRN_GEOMETRY_PAGE_MB");
        if (lpcValue && lpcValue[0] && lpcValue[1] == '\0')
        {
            if (lpcValue[0] == '1') return 1024u * 1024u;
            if (lpcValue[0] == '2') return 2u * 1024u * 1024u;
            if (lpcValue[0] == '4') return 4u * 1024u * 1024u;
            if (lpcValue[0] == '8') return 8u * 1024u * 1024u;
        }
        return 8u * 1024u * 1024u;
    }(), [] {
        const char* lpcValue = std::getenv("BRN_GEOMETRY_RANGE_SEARCH");
        return !lpcValue || lpcValue[0] != '0';
    }());
    std::vector<u8> sVertexBakeScratch;
    const bool sbSpecializedIndexRange = [] {
        const char* lpcValue = std::getenv("BRN_GEOMETRY_INDEX_RANGE");
        return !lpcValue || lpcValue[0] != '0';
    }();
    // ---- keys ---------------------------------------------------------------
    // Both keys are compared and hashed as RAW BYTES, so every field -- including
    // the padding a struct with mixed widths carries -- has to be deterministic.
    // MakeKey memsets before filling; nothing else may construct one.

    struct VertexKey
    {
        const void* mpHeader;
        u32         muSourceStride;
        u32         muExpandedStride;
        u32         muDec3nCount;
        u32         muNumVertices;
        u16         mau16Dec3nOffsets[16];
    };

    struct IndexKey
    {
        const void* mpHeader;
        u32         muStartIndex;
        u32         muIndexCount;
        u32         muResetIndex;
        s32         miPrimitiveType;
        u32         muPrimitiveCount;
        u8          mu8Reset32Bit;      // bit 0 = reset enabled, bit 1 = 32-bit indices
        u8          mau8Pad[3];
    };

    // The ONLY two places a key is built, so the byte-wise hash/equality below stays
    // deterministic: memset first, then every named field, and nothing else.
    //
    // The front cache further down depends on that discipline: because every byte these
    // two do not write is zero in EVERY key, comparing a plan against a stored key field
    // by field (KeyMatchesPlan, below) gives the same answer as memcmp-ing this function's
    // output against it -- exactness, without paying for the padding bytes.
    VertexKey MakeVertexKey(const WorldGeometryVertexPlan& lrPlan)
    {
        VertexKey lKey;
        std::memset(&lKey, 0, sizeof(lKey));
        lKey.mpHeader         = lrPlan.mpHeader;
        lKey.muSourceStride   = lrPlan.muSourceStride;
        lKey.muExpandedStride = lrPlan.muExpandedStride;
        lKey.muDec3nCount     = lrPlan.muDec3nCount;
        lKey.muNumVertices    = lrPlan.muNumVertices;
        for (u32 lu = 0; lu < lrPlan.muDec3nCount && lu < 16u; ++lu)
            lKey.mau16Dec3nOffsets[lu] = lrPlan.mau16Dec3nOffsets[lu];
        return lKey;
    }

    IndexKey MakeIndexKey(const WorldGeometryIndexPlan& lrPlan)
    {
        IndexKey lKey;
        std::memset(&lKey, 0, sizeof(lKey));
        lKey.mpHeader         = lrPlan.mpHeader;
        lKey.muStartIndex     = lrPlan.muStartIndex;
        lKey.muIndexCount     = lrPlan.muIndexCount;
        lKey.muResetIndex     = lrPlan.muResetIndex;
        lKey.miPrimitiveType  = lrPlan.miMappedPrimitiveType;
        lKey.muPrimitiveCount = lrPlan.muMappedPrimitiveCount;
        lKey.mu8Reset32Bit    = static_cast<u8>((lrPlan.mbResetEnabled ? 1u : 0u)
                                              | (lrPlan.mb32Bit ? 2u : 0u));
        return lKey;
    }

    // FULL-IDENTITY tests, field for field against MakeVertexKey / MakeIndexKey above --
    // NOT a subset, and not a hash. The front cache's whole safety argument is that a hit
    // means "MakeVertexKey(plan) == stored key", i.e. exactly what the map's RawEqual
    // would have concluded, so the mirrors handed back cannot belong to another plan.
    // The offset loop stops at muDec3nCount for the same reason MakeVertexKey's fill does:
    // both keys already agree on that count, and offsets past it are zero in both.
    inline bool VertexKeyMatchesPlan(const VertexKey& lrKey,
                                     const WorldGeometryVertexPlan& lrPlan)
    {
        if (lrKey.mpHeader != lrPlan.mpHeader
            || lrKey.muSourceStride != lrPlan.muSourceStride
            || lrKey.muExpandedStride != lrPlan.muExpandedStride
            || lrKey.muDec3nCount != lrPlan.muDec3nCount
            || lrKey.muNumVertices != lrPlan.muNumVertices)
            return false;
        for (u32 lu = 0; lu < lrPlan.muDec3nCount && lu < 16u; ++lu)
        {
            if (lrKey.mau16Dec3nOffsets[lu] != lrPlan.mau16Dec3nOffsets[lu])
                return false;
        }
        return true;
    }

    inline bool IndexKeyMatchesPlan(const IndexKey& lrKey,
                                    const WorldGeometryIndexPlan& lrPlan)
    {
        return lrKey.mpHeader == lrPlan.mpHeader
            && lrKey.muStartIndex == lrPlan.muStartIndex
            && lrKey.muIndexCount == lrPlan.muIndexCount
            && lrKey.muResetIndex == lrPlan.muResetIndex
            && lrKey.miPrimitiveType == lrPlan.miMappedPrimitiveType
            && lrKey.muPrimitiveCount == lrPlan.muMappedPrimitiveCount
            && lrKey.mu8Reset32Bit == static_cast<u8>((lrPlan.mbResetEnabled ? 1u : 0u)
                                                    | (lrPlan.mb32Bit ? 2u : 0u));
    }

    // The hash is deliberately CHEAP and narrow: the header pointer already separates
    // almost every key (a header has one or two plans at most), so hashing the leading
    // pointer + first two words as three multiplies costs less than a byte-wise FNV over
    // the whole 48-byte key on a path that runs ~3,800 times a frame. Equality still
    // compares the whole key (RawEqual), so a narrow hash can only cost a bucket collision,
    // never a wrong hit.
    template <typename T>
    struct RawHash
    {
        size_t operator()(const T& lrKey) const
        {
            uintptr_t luWords[3] = { 0, 0, 0 };
            std::memcpy(luWords, &lrKey, sizeof(luWords) < sizeof(T) ? sizeof(luWords) : sizeof(T));
            uintptr_t luHash = luWords[0] * static_cast<uintptr_t>(0x9E3779B97F4A7C15ull);
            luHash ^= (luWords[1] + static_cast<uintptr_t>(0x7F4A7C159E3779B9ull)) * static_cast<uintptr_t>(0xC2B2AE3D27D4EB4Full);
            luHash ^= (luWords[2] + static_cast<uintptr_t>(0x165667B19E3779F9ull)) * static_cast<uintptr_t>(0x27D4EB2F165667C5ull);
            return static_cast<size_t>(luHash ^ (luHash >> 29));
        }
    };

    template <typename T>
    struct RawEqual
    {
        bool operator()(const T& lrLeft, const T& lrRight) const
        {
            return std::memcmp(&lrLeft, &lrRight, sizeof(T)) == 0;
        }
    };

    // ---- retained entries ---------------------------------------------------
    // Each carries the HOST source range it was built from, so the streamer's free
    // of that range can find it again.

    struct RetainedVertexBuffer
    {
        u64 muEvictionToken;
        GeometryPoolType::Allocation mAllocation;
        IDirect3DVertexBuffer9* mpBuffer;
        const u8*               mpSourceBegin;
        const u8*               mpSourceEnd;
        const void*             mpHeader;
        u32                     muStride;
        u32                     muNumVertices;
        u32                     muBytes;
    };

    struct RetainedIndexBuffer
    {
        u64 muEvictionToken;
        GeometryPoolType::Allocation mAllocation;
        u32 muIndexStart;
        IDirect3DIndexBuffer9* mpBuffer;
        const u8*              mpSourceBegin;
        const u8*              mpSourceEnd;
        const void*            mpHeader;
        s32                    miPrimitiveType;
        u32                    muPrimitiveCount;
        u32                    muBytes;
        u32                    mau32FirstIndices[3];
        u32                    muFirstIndexCount;
        u32                    muMinIndex;
        u32                    muMaxIndex;
    };

    typedef std::unordered_map<VertexKey, RetainedVertexBuffer,
                               RawHash<VertexKey>, RawEqual<VertexKey> > VertexMap;
    typedef std::unordered_map<IndexKey, RetainedIndexBuffer,
                               RawHash<IndexKey>, RawEqual<IndexKey> > IndexMap;

    VertexMap sVertexBuffers;
    IndexMap  sIndexBuffers;

    // ---- the eviction index -------------------------------------------------
    // A free notification names a byte range, not a key, so the mirrors touching a
    // range have to be findable without walking the whole map (the pool frees one
    // resource at a time and a track swap frees thousands). Every mirror registers
    // a compact generation-checked reference under each 4 KiB source-address
    // bucket its header and bytes touch. The full-key path remains a control.
    // Dense small headers made 64 KiB buckets repeatedly scan unrelated keys on
    // each resource free. Finer buckets retain the same exact RangeHit test.
    // This lookup granularity is independent of the native GPU page capacity.
    const u32 KU_PAGE_SHIFT = [] {
        const char* lpcValue = std::getenv("BRN_GEOMETRY_SOURCE_PAGE_KB");
        if (lpcValue && std::strcmp(lpcValue, "64") == 0) return 16u;
        if (lpcValue && std::strcmp(lpcValue, "16") == 0) return 14u;
        return 12u;
    }();

    typedef std::unordered_map<uintptr_t, std::vector<VertexKey> > VertexPageIndex;
    typedef std::unordered_map<uintptr_t, std::vector<IndexKey> >  IndexPageIndex;

    VertexPageIndex sVertexPages;
    IndexPageIndex  sIndexPages;
    using TokenPageIndex = std::unordered_map<uintptr_t, std::vector<u64>>;
    TokenPageIndex sVertexTokenPages, sIndexTokenPages;
    GeometryEntryReferencesPC<VertexMap::value_type> sVertexReferences;
    GeometryEntryReferencesPC<IndexMap::value_type> sIndexReferences;
    const bool sbCompactLifetime = [] {
        const char* lpcValue = std::getenv("BRN_GEOMETRY_COMPACT_LIFETIME");
        return !lpcValue || lpcValue[0] != '0';
    }();


    // Reused across creations (never touched on the draw path): the strip re-cut
    // writes into this before the index buffer is sized and filled.
    std::vector<u8> sBakeScratch;

    // ---- the direct-mapped front cache (legacy control) ---------------------
    // WHY: once the mirrors exist, the two unordered_map probes ARE the remaining cost of
    // WorldGeometry_Prepare. Measured on a 17 ms in-world frame at ~3,500-4,000 dispatch
    // draws: Prepare 10.7% inclusive, of which AcquireVertexBuffer self 3.0%,
    // AcquireIndexBuffer self 2.7% and the key memcmp 4.2% -- about 1.7 ms a frame spent
    // hashing, chasing bucket chains across a ~10k-entry vertex map and a ~5k-entry index
    // map (both far past any cache), and memcmp-ing 56/32-byte keys. Almost none of that
    // is new work: the same meshes are drawn again the next frame, so a repeat draw only
    // has to find the mirrors it already found last frame.
    //
    // So a repeat draw goes through a plain fixed array instead: hash four words, load one
    // slot, compare both keys. Direct-mapped on purpose -- no chain to walk, no rehash, no
    // allocation, and a conflicting draw simply misses and takes the old path. The maps
    // remain the sole OWNERS of a mirror; this only remembers where one was found.
    //
    // SIZE: MEASURED 2026-08-15 with 8192 slots: 5.6M hits / 5.9M misses over a driving run,
    // i.e. ~49% -- worse than the ~67% a 3,800-key estimate predicts, because a mesh is
    // looked up under up to THREE different plans a frame (its lit technique, the z-only
    // pre-pass and the shadow cascades bind different declarations / runs), so the live key
    // set is ~10k, a load factor of ~1.2 on 8192. 32768 slots puts it at ~0.3 (~75% hits).
    // The table retains the completed draw as well as its full keys. A hit reads
    // contiguous metadata instead of following two map nodes for the same immutable
    // buffer/range values on every draw. At 160 bytes per entry this is 5 MiB.
    const u32 KU_FRONT_CACHE_ENTRIES = 32768u;
    static_assert((KU_FRONT_CACHE_ENTRIES & (KU_FRONT_CACHE_ENTRIES - 1u)) == 0u,
                  "the slot index is masked, not reduced -- entry count must be a power of two");

    // Field order is deliberate: the generation and the vertex key's discriminating head
    // share the first cache line, so the two ways a probe usually fails (retired cache,
    // different mesh) both resolve off one load.
    struct GeometryFrontCacheEntry
    {
        u64                   muGeneration;   // 0 = never filled; see suGeometryGeneration
        VertexKey             mVertexKey;
        IndexKey              mIndexKey;
        WorldGeometryDraw     mDraw;          // borrowed native buffers, NOT owned
    };

    // POD in BSS: every slot starts at generation 0, which no live generation ever equals,
    // so the cold cache is all misses with no explicit initialisation anywhere.
    GeometryFrontCacheEntry saFrontCache[KU_FRONT_CACHE_ENTRIES];
    GeometryAssociativeFrontCachePC<GeometryFrontCacheEntry, 4096> sAssociativeFrontCache;
    // FLAG PC-platform leaf: four-way tags reduce conflicts and the active table
    // footprint. Full keys are still checked before returning borrowed draw data.
    // BRN_GEOMETRY_ASSOC_CACHE=0 retains the legacy direct-mapped control.
    const bool sbAssociativeFrontCache = [] {
        const char* value = std::getenv("BRN_GEOMETRY_ASSOC_CACHE");
        return !value || value[0] != '0';
    }();


    // THE LIFETIME GUARD. The cached draw borrows the maps' native buffer allocations.
    // Their offsets, topology and ranges are immutable after creation. Both owners'
    // erase paths (OnResourceMemoryFreed and ReleaseAll) retire this generation BEFORE
    // releasing an allocation, so a hit never returns a retired native pointer/range.
    // Map insertions/rehashes do not change the cached allocation or its metadata.
    //
    // Retiring the whole cache is the right trade because evictions are BURSTY: a track-unit
    // swap frees thousands of mirrors in one notification storm and would have invalidated
    // most of the cache anyway, while an ordinary frame erases nothing and pays nothing. The
    // alternative -- a per-node back-index so an erase could clear just its slot -- would put
    // bookkeeping on the create path to save a few hundred refills per track swap.
    u64 suGeometryGeneration = 1;   // starts at 1: 0 is reserved for "slot never filled"

    inline void RetireFrontCache()
    {
        ++suGeometryGeneration;
        if (suGeometryGeneration == 0)
        {
            // A reused epoch must not revive an ancient borrowed draw.
            std::memset(saFrontCache, 0, sizeof(saFrontCache));
            sAssociativeFrontCache.Clear();
            suGeometryGeneration = 1;
        }
    }

    // The words that actually discriminate one draw from another. This picks the SLOT only
    // -- the full keys are still validated on a hit -- so a weak mix can cost a conflict
    // miss and never a wrong mirror. The run's start/count are in here because the pointers
    // alone do not separate draws (one index header serves several runs, slice 0 of an
    // instanced draw among them) -- and so are the VERTEX PLAN's stride/DEC3N words, because
    // one vertex header is bound under several technique plans a frame (lit, z-only,
    // shadow) that share every other word.
    // ⚠ MEASURED 2026-08-15: without the plan words in the slot hash the lit and z-only
    // draws of every mesh mapped to ONE slot and evicted each other every frame -- a flat
    // 50% miss rate that did not move when the table was quadrupled. That is thrash, not
    // capacity, and it made Prepare MORE expensive than the maps alone (12.6% vs 10.7%).
    inline u64 FrontCacheHash(const WorldGeometryVertexPlan& lrVertexPlan,
                              const WorldGeometryIndexPlan& lrIndexPlan)
    {
        u64 luHash = static_cast<u64>(reinterpret_cast<uintptr_t>(lrVertexPlan.mpHeader));
        luHash ^= static_cast<u64>(reinterpret_cast<uintptr_t>(lrIndexPlan.mpHeader))
                  * 0x9E3779B97F4A7C15ull;
        luHash += static_cast<u64>(lrIndexPlan.muStartIndex) * 0xC2B2AE3D27D4EB4Full;
        luHash ^= static_cast<u64>(lrIndexPlan.muIndexCount) * 0x165667B19E3779F9ull;
        luHash += (static_cast<u64>(lrVertexPlan.muExpandedStride & 0xFFFFu)
                   | (static_cast<u64>(lrVertexPlan.muDec3nCount & 0xFFFFu) << 16)
                   | (static_cast<u64>(lrIndexPlan.muMappedPrimitiveCount) << 32))
                  * 0x94D049BB133111EBull;
        luHash *= 0xBF58476D1CE4E5B9ull;
        // Preserve the full mix for associative fingerprints. Both policies take
        // set bits from the upper half, away from aligned pointer low bits.
        return luHash;
    }

    inline u32 FrontCacheSlot(const WorldGeometryVertexPlan& lrVertexPlan,
                              const WorldGeometryIndexPlan& lrIndexPlan)
    {
        return static_cast<u32>(FrontCacheHash(lrVertexPlan, lrIndexPlan) >> 32)
            & (KU_FRONT_CACHE_ENTRIES - 1u);
    }

    // ---- counters -----------------------------------------------------------
    u32 suVertexBuffersCreated  = 0;
    u32 suIndexBuffersCreated   = 0;
    u64 suVertexBytes           = 0;
    u64 suIndexBytes            = 0;
    u32 suVertexBuffersEvicted  = 0;
    u32 suIndexBuffersEvicted   = 0;
    u32 suCreateFailures        = 0;
    // Front-cache hit/miss, so the log line can show whether the fast path is actually
    // carrying the frame. Deliberately NOT part of ReportIfDue's "has anything changed"
    // test -- they move every draw, and a report every draw is exactly what must not happen.
    u64 suFrontCacheHits        = 0;
    u64 suFrontCacheMisses      = 0;
    u32 suReportedVertexCreated = 0xFFFFFFFFu;   // forces the first report
    u32 suReportedIndexCreated  = 0;
    u32 suReportedEvictions     = 0;
    u32 suLastReportTicks       = 0;

    inline IDirect3DDevice9* Dev()
    {
        return renderengine::gDevice;
    }

    D3DPOOL GeometryPool()
    {
        // FLAG PC-platform leaf: controlled allocation A/B for frame-time traces.
        // Geometry, topology and draw parameters are identical on both routes.
        static const bool sbManaged = [] {
            const char* lpValue = std::getenv("BRN_GEOMETRY_MANAGED");
            return lpValue && lpValue[0] == '1';
        }();
        return sbManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
    }

    unsigned VertexPoolAlignment(unsigned luStride)
    {
        // FLAG PC-platform leaf: keep byte offsets divisible by the vertex
        // stride so GeometryBindingsPC::Rebase can retain a zero-offset stream.
        // This avoids D3D9 stream updates without changing vertex addresses.
        static const bool sbAligned = [] {
            const char* lpcValue = std::getenv("BRN_GEOMETRY_ALIGN_STRIDE");
            return !lpcValue || lpcValue[0] != '0';
        }();
        return sbAligned && luStride && !(luStride & 3u) ? luStride : 16u;
    }

    inline uintptr_t PageOf(const void* lpPointer)
    {
        return reinterpret_cast<uintptr_t>(lpPointer) >> KU_PAGE_SHIFT;
    }

    template <typename TKey, typename TIndex>
    void RegisterPages(TIndex& lrIndex, const TKey& lrKey,
                       const void* lpHeader, const u8* lpBegin, const u8* lpEnd)
    {
        lrIndex[PageOf(lpHeader)].push_back(lrKey);
        const uintptr_t luHeaderPage = PageOf(lpHeader);
        const uintptr_t luFirst      = PageOf(lpBegin);
        const uintptr_t luLast       = PageOf(lpEnd != lpBegin ? lpEnd - 1 : lpBegin);
        for (uintptr_t luPage = luFirst; luPage <= luLast; ++luPage)
        {
            if (luPage != luHeaderPage)
                lrIndex[luPage].push_back(lrKey);
        }
    }

    template<class TMap, class TIndex, class TReferences>
    typename TMap::mapped_type& RegisterMirror(TMap& lrMap, TIndex& lrLegacy,
        TokenPageIndex& lrCompact, TReferences& lrReferences,
        const typename TMap::key_type& lrKey, const typename TMap::mapped_type& lrEntry)
    {
        auto lInserted = lrMap.emplace(lrKey, lrEntry);
        auto& lrStored = lInserted.first->second;
        if (lInserted.second)
        {
            if (sbCompactLifetime)
            {
                lrStored.muEvictionToken = lrReferences.Add(&*lInserted.first);
                RegisterPages(lrCompact, lrStored.muEvictionToken, lrStored.mpHeader,
                              lrStored.mpSourceBegin, lrStored.mpSourceEnd);
            }
            else RegisterPages(lrLegacy, lrKey, lrStored.mpHeader,
                               lrStored.mpSourceBegin, lrStored.mpSourceEnd);
        }
        return lrStored;
    }

    // One log line, at most every ten seconds AND only when a counter moved. Called from
    // the front cache's MISS path now (see WorldGeometry_Prepare), which reports the same
    // events at the same times: every creation happens on that path by definition, and
    // every eviction retires the whole front cache, so the frame after one is all misses.
    // It must never allocate and must almost never do work.
    void ReportIfDue()
    {
        const bool lbChanged = (suVertexBuffersCreated != suReportedVertexCreated)
                            || (suIndexBuffersCreated != suReportedIndexCreated)
                            || ((suVertexBuffersEvicted + suIndexBuffersEvicted)
                                    != suReportedEvictions);
        if (!lbChanged)
            return;

        // While a change is pending but not yet due, only every 256th call pays for the
        // clock read -- the state above stays "changed" for the whole 10 s window otherwise.
        static u32 suSampleGate = 0;
        if (suReportedVertexCreated != 0xFFFFFFFFu && ((++suSampleGate) & 0xFFu) != 0u)
            return;

        const u32 luTicks = static_cast<u32>(::GetTickCount());
        if (suReportedVertexCreated != 0xFFFFFFFFu
            && (luTicks - suLastReportTicks) < 10000u)
            return;

        suLastReportTicks       = luTicks;
        suReportedVertexCreated = suVertexBuffersCreated;
        suReportedIndexCreated  = suIndexBuffersCreated;
        suReportedEvictions     = suVertexBuffersEvicted + suIndexBuffersEvicted;

        char lacMsg[320];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[worldgeom] retained VB=%u (%.2f MB) IB=%u (%.2f MB)"
                      " live=%u/%u evicted VB=%u IB=%u createFail=%u"
                      " front=%llu hit/%llu miss\n",
                      suVertexBuffersCreated,
                      static_cast<double>(suVertexBytes) / (1024.0 * 1024.0),
                      suIndexBuffersCreated,
                      static_cast<double>(suIndexBytes) / (1024.0 * 1024.0),
                      static_cast<unsigned>(sVertexBuffers.size()),
                      static_cast<unsigned>(sIndexBuffers.size()),
                      suVertexBuffersEvicted, suIndexBuffersEvicted,
                      suCreateFailures,
                      static_cast<unsigned long long>(suFrontCacheHits),
                      static_cast<unsigned long long>(suFrontCacheMisses));
        CgsDev::Log::WriteToLog(lacMsg);
    }

    // ---- vertex mirror creation ---------------------------------------------
    bool StoreGeometry(IDirect3DDevice9* lpDevice, GeometryBufferKind leKind,
                       const void* lpData, unsigned luBytes,
                       GeometryPoolType::Allocation& lrAllocation, unsigned luAlignment = 16u)
    {
        // Includes allocator search/bookkeeping and the nested native upload.
        FrameProfile::CycleScope lProfile(FrameProfile::GEOMETRY_STORE);
        return sGeometryPool.Store(lpDevice, leKind, lpData, luBytes, lrAllocation, luAlignment);
    }

    // Preserve the original signed10-bit /511 conversion, including the -512
    // clamp and untouched record bytes. Compile separate kernels so the scalar
    // comparison control adds no branch/register pressure inside the lookup loop.
    template<bool TB_USE_LOOKUP>
    __declspec(noinline) void BakePackedVertexData(u8* lpDestination, const WorldGeometryVertexPlan& lrPlan)
    {
        const u8* const lpSource = static_cast<const u8*>(lrPlan.mpData);
        const f32* const lpfNormalTable = PackedNormalPC::gDecodeTable.mafValues;
        for (u32 luVertex = 0; luVertex < lrPlan.muNumVertices; ++luVertex)
        {
            const u8* lpSourceVertex = lpSource + luVertex * lrPlan.muSourceStride;
            u8* lpDestinationVertex  = lpDestination + luVertex * lrPlan.muExpandedStride;
            u32 luSourceCursor      = 0;
            u32 luDestinationCursor = 0;
            for (u32 luPacked = 0; luPacked < lrPlan.muDec3nCount; ++luPacked)
            {
                const u32 luOffset = lrPlan.mau16Dec3nOffsets[luPacked];
                const u32 luPrefixBytes = luOffset - luSourceCursor;
                std::memcpy(lpDestinationVertex + luDestinationCursor,
                            lpSourceVertex + luSourceCursor, luPrefixBytes);
                luDestinationCursor += luPrefixBytes;

                u32 luValue;
                std::memcpy(&luValue, lpSourceVertex + luOffset, sizeof(luValue));
                f32 lafNormal[3];
                if constexpr (TB_USE_LOOKUP)
                {
                    lafNormal[0] = lpfNormalTable[luValue & 1023u];
                    lafNormal[1] = lpfNormalTable[(luValue >> 10) & 1023u];
                    lafNormal[2] = lpfNormalTable[(luValue >> 20) & 1023u];
                }
                else
                {
                    for (u32 luComponent = 0; luComponent < 3; ++luComponent)
                    {
                        int liComponent = static_cast<int>(
                            (luValue >> (10u * luComponent)) & 0x3FFu);
                        if ((liComponent & 0x200) != 0)
                            liComponent -= 0x400;
                        lafNormal[luComponent] = liComponent <= -512
                            ? -1.0f : static_cast<f32>(liComponent) / 511.0f;
                    }
                }
                std::memcpy(lpDestinationVertex + luDestinationCursor,
                            lafNormal, sizeof(lafNormal));
                luSourceCursor = luOffset + 4u;
                luDestinationCursor += sizeof(lafNormal);
            }
            const u32 luTailBytes = lrPlan.muSourceStride - luSourceCursor;
            std::memcpy(lpDestinationVertex + luDestinationCursor,
                        lpSourceVertex + luSourceCursor, luTailBytes);
        }
    }

    void BakeVertexData(u8* lpDestination, const WorldGeometryVertexPlan& lrPlan)
    {
        FrameProfile::Scope lConvertProfile(FrameProfile::GEOMETRY_CONVERT);
        if (lrPlan.muDec3nCount == 0)
        {
            std::memcpy(lpDestination, lrPlan.mpData,
                        static_cast<size_t>(lrPlan.muNumVertices) * lrPlan.muSourceStride);
        }
        else if (PackedNormalPC::Enabled())
        {
            BakePackedVertexData<true>(lpDestination, lrPlan);
        }
        else
        {
            BakePackedVertexData<false>(lpDestination, lrPlan);
        }
    }

    // lrKey is MakeVertexKey(lrPlan), built by the caller so the front-cache miss path can
    // reuse it for the slot it is about to fill.
    RetainedVertexBuffer* AcquireVertexBuffer(const WorldGeometryVertexPlan& lrPlan,
                                              const VertexKey& lrKey)
    {
        VertexMap::iterator lIt = sVertexBuffers.find(lrKey);
        if (lIt != sVertexBuffers.end())
            return lIt->second.mpBuffer != nullptr ? &lIt->second : nullptr;

        FrameProfile::Scope lPrepareProfile(FrameProfile::GEOMETRY_PREPARE);

        IDirect3DDevice9* const lpDevice = Dev();
        const u32 luBytes = lrPlan.muNumVertices * lrPlan.muExpandedStride;
        RetainedVertexBuffer lEntry;
        std::memset(&lEntry, 0, sizeof(lEntry));
        lEntry.mpSourceBegin = static_cast<const u8*>(lrPlan.mpData);
        lEntry.mpSourceEnd   = lEntry.mpSourceBegin
                             + static_cast<size_t>(lrPlan.muNumVertices) * lrPlan.muSourceStride;
        lEntry.mpHeader      = lrPlan.mpHeader;
        lEntry.muStride      = lrPlan.muExpandedStride;
        lEntry.muNumVertices = lrPlan.muNumVertices;
        lEntry.muBytes       = luBytes;

        if (lpDevice != nullptr && luBytes != 0)
        {
            const void* lpPayload = lrPlan.mpData;
            if (lrPlan.muDec3nCount)
            {
                // Convert in ordinary CPU memory, then copy once into the GPU's
                // write-combined mapping. Per-component writes into that mapping
                // made cold camera views disproportionately expensive.
                sVertexBakeScratch.resize(luBytes);
                BakeVertexData(sVertexBakeScratch.data(), lrPlan);
                lpPayload = sVertexBakeScratch.data();
            }
            if (GeometryPool() == D3DPOOL_DEFAULT && StoreGeometry(lpDevice,
                    GeometryBufferKind::Vertex, lpPayload, luBytes, lEntry.mAllocation,
                    VertexPoolAlignment(lrPlan.muExpandedStride)))
                lEntry.mpBuffer = static_cast<IDirect3DVertexBuffer9*>(lEntry.mAllocation.GetBuffer());
            else
            {
                // Unsupported hardware or an allocation failure retains the old
                // individual managed-buffer path, with identical payload bytes.
                HRESULT lhCreate;
                {
                    FrameProfile::CycleScope lCreateProfile(FrameProfile::GEOMETRY_CREATE);
                    lhCreate = lpDevice->CreateVertexBuffer(luBytes, D3DUSAGE_WRITEONLY, 0,
                        TextureUploadPC::StaticBufferPool(lpDevice), &lEntry.mpBuffer, nullptr);
                }
                if (SUCCEEDED(lhCreate))
                {
                    FrameProfile::NativeBuffer();
                    void* lpLocked = nullptr;
                    if (SUCCEEDED(lEntry.mpBuffer->Lock(0, luBytes, &lpLocked, 0)) && lpLocked)
                    {
                        std::memcpy(lpLocked, lpPayload, luBytes);
                        if (FAILED(lEntry.mpBuffer->Unlock()))
                        { lEntry.mpBuffer->Release(); lEntry.mpBuffer = nullptr; }
                    }
                    else { lEntry.mpBuffer->Release(); lEntry.mpBuffer = nullptr; }
                }
            }
        }

        if (lEntry.mpBuffer == nullptr)
            ++suCreateFailures;
        else
        {
            ++suVertexBuffersCreated;
            FrameProfile::Geometry(true, luBytes);
            suVertexBytes += luBytes;
        }

        // A failed creation is cached too (as a null mirror) so a mesh that cannot be
        // retained does not retry -- and re-fail -- on every single draw.
        FrameProfile::CycleScope lRegisterProfile(FrameProfile::GEOMETRY_REGISTER);
        RetainedVertexBuffer& lrStored = RegisterMirror(sVertexBuffers, sVertexPages,
            sVertexTokenPages, sVertexReferences, lrKey, lEntry);
        return lrStored.mpBuffer != nullptr ? &lrStored : nullptr;
    }

    // ---- index mirror creation ----------------------------------------------
    // Identical output to XenonD3D9Shims.cpp's ExpandStripRunsToList: runs are cut at
    // every reset index, degenerate stitch triangles are dropped, and a strip's odd
    // triangles have their first two indices swapped back so the winding survives the
    // conversion to a list. The only difference is that this writes into a sized
    // buffer instead of insert()ing three indices at a time.
    template <typename T>
    u32 BakeStripRunsToList(const T* lpIndices, u32 luCount, T ltReset, T* lpOut)
    {
        u32 luTriangles = 0;
        u32 luRunStart  = 0;
        for (u32 lu = 0; lu <= luCount; ++lu)
        {
            if (lu != luCount && lpIndices[lu] != ltReset)
                continue;
            for (u32 lv = luRunStart; lv + 2u < lu; ++lv)
            {
                const T lt0 = lpIndices[lv];
                const T lt1 = lpIndices[lv + 1];
                const T lt2 = lpIndices[lv + 2];
                if (lt0 == lt1 || lt1 == lt2 || lt0 == lt2)
                    continue;
                const bool lbOdd = (((lv - luRunStart) & 1u) != 0u);
                lpOut[luTriangles * 3u + 0u] = lbOdd ? lt1 : lt0;
                lpOut[luTriangles * 3u + 1u] = lbOdd ? lt0 : lt1;
                lpOut[luTriangles * 3u + 2u] = lt2;
                ++luTriangles;
            }
            luRunStart = lu + 1u;
        }
        return luTriangles;
    }

    // lrKey is MakeIndexKey(lrPlan); same reason as AcquireVertexBuffer above.
    RetainedIndexBuffer* AcquireIndexBuffer(const WorldGeometryIndexPlan& lrPlan,
                                            const IndexKey& lrKey,
                                            bool* lpbSkip)
    {
        *lpbSkip = false;

        IndexMap::iterator lIt = sIndexBuffers.find(lrKey);
        if (lIt != sIndexBuffers.end())
        {
            if (lIt->second.mpBuffer == nullptr)
            {
                // muPrimitiveCount 0 with no buffer is the "expanded to nothing" case,
                // which is a SKIP, not a failure to retain.
                *lpbSkip = (lIt->second.muPrimitiveCount == 0
                            && lIt->second.muFirstIndexCount == 0xFFFFFFFFu);
                return nullptr;
            }
            return &lIt->second;
        }

        FrameProfile::Scope lPrepareProfile(FrameProfile::GEOMETRY_PREPARE);
        const u32 luIndexSize = lrPlan.mb32Bit ? 4u : 2u;
        const u8* const lpRun = static_cast<const u8*>(lrPlan.mpRun);

        RetainedIndexBuffer lEntry;
        std::memset(&lEntry, 0, sizeof(lEntry));
        lEntry.mpHeader         = lrPlan.mpHeader;
        lEntry.mpSourceBegin    = lpRun;
        lEntry.mpSourceEnd      = lpRun + static_cast<size_t>(lrPlan.muIndexCount) * luIndexSize;
        lEntry.miPrimitiveType  = lrPlan.miMappedPrimitiveType;
        lEntry.muPrimitiveCount = lrPlan.muMappedPrimitiveCount;

        const bool lbExpandStrip =
            lrPlan.mbResetEnabled
            && lrPlan.miMappedPrimitiveType == static_cast<s32>(D3DPT_TRIANGLESTRIP);

        const u8* lpPayload = lpRun;
        u32       luPayloadBytes = lrPlan.muIndexCount * luIndexSize;

        if (lbExpandStrip)
        {
            FrameProfile::CycleScope lIndexProfile(FrameProfile::GEOMETRY_INDEX_BUILD);
            // Upper bound: a strip of N indices can yield at most N triangles.
            sBakeScratch.clear();
            sBakeScratch.resize(static_cast<size_t>(lrPlan.muIndexCount) * 3u * luIndexSize);
            u32 luTriangles;
            if (lrPlan.mb32Bit)
            {
                luTriangles = BakeStripRunsToList<u32>(
                    reinterpret_cast<const u32*>(lpRun), lrPlan.muIndexCount,
                    lrPlan.muResetIndex, reinterpret_cast<u32*>(&sBakeScratch[0]));
            }
            else
            {
                luTriangles = BakeStripRunsToList<u16>(
                    reinterpret_cast<const u16*>(lpRun), lrPlan.muIndexCount,
                    static_cast<u16>(lrPlan.muResetIndex),
                    reinterpret_cast<u16*>(&sBakeScratch[0]));
            }

            {
                // [DIAG one-shot] the first mesh whose strips were re-cut (the same
                // witness the per-draw path used to print).
                static bool sbDiag = false;
                if (!sbDiag)
                {
                    sbDiag = true;
                    char lacMsg[192];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                                  "[worldgeom] primitive reset honoured: %u strip indices"
                                  " -> %u list triangles (reset index 0x%X)\n",
                                  lrPlan.muIndexCount, luTriangles, lrPlan.muResetIndex);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }

            if (luTriangles == 0)
            {
                lEntry.muPrimitiveCount = 0;
                lEntry.muFirstIndexCount = 0xFFFFFFFFu;   // the SKIP marker (see above)
                FrameProfile::CycleScope lRegisterProfile(FrameProfile::GEOMETRY_REGISTER);
                RegisterMirror(sIndexBuffers, sIndexPages, sIndexTokenPages,
                               sIndexReferences, lrKey, lEntry);
                *lpbSkip = true;
                return nullptr;
            }

            lEntry.miPrimitiveType  = static_cast<s32>(D3DPT_TRIANGLELIST);
            lEntry.muPrimitiveCount = luTriangles;
            lpPayload      = &sBakeScratch[0];
            luPayloadBytes = luTriangles * 3u * luIndexSize;
        }

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice != nullptr && luPayloadBytes != 0)
        {
            const GeometryBufferKind leKind = lrPlan.mb32Bit
                ? GeometryBufferKind::Index32 : GeometryBufferKind::Index16;
            if (GeometryPool() == D3DPOOL_DEFAULT && StoreGeometry(lpDevice,
                    leKind, lpPayload, luPayloadBytes, lEntry.mAllocation))
            {
                lEntry.mpBuffer = static_cast<IDirect3DIndexBuffer9*>(lEntry.mAllocation.GetBuffer());
                lEntry.muIndexStart = lEntry.mAllocation.muOffset / luIndexSize;
            }
            else if ([&] {
                FrameProfile::CycleScope lCreateProfile(FrameProfile::GEOMETRY_CREATE);
                return SUCCEEDED(lpDevice->CreateIndexBuffer(luPayloadBytes, D3DUSAGE_WRITEONLY,
                    lrPlan.mb32Bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                    TextureUploadPC::StaticBufferPool(lpDevice), &lEntry.mpBuffer, nullptr));
            }())
            {
                FrameProfile::NativeBuffer();
                void* lpLocked = nullptr;
                if (SUCCEEDED(lEntry.mpBuffer->Lock(0, luPayloadBytes, &lpLocked, 0)) && lpLocked)
                {
                    std::memcpy(lpLocked, lpPayload, luPayloadBytes);
                    if (FAILED(lEntry.mpBuffer->Unlock()))
                    { lEntry.mpBuffer->Release(); lEntry.mpBuffer = nullptr; }
                }
                else { lEntry.mpBuffer->Release(); lEntry.mpBuffer = nullptr; }
            }
        }

        if (lEntry.mpBuffer == nullptr)
            ++suCreateFailures;
        else
        {
            ++suIndexBuffersCreated;
            FrameProfile::Geometry(false, luPayloadBytes);
            suIndexBytes += luPayloadBytes;
            lEntry.muBytes = luPayloadBytes;

            FrameProfile::CycleScope lIndexProfile(FrameProfile::GEOMETRY_INDEX_BUILD);

            // The first three SUBMITTED index values, kept so the caller's clip-space
            // and wheel probes can sample the same vertices they used to read straight
            // out of the scratch run.
            const u32 luAvailable = luPayloadBytes / luIndexSize;
            lEntry.muFirstIndexCount = luAvailable < 3u ? luAvailable : 3u;
            for (u32 lu = 0; lu < lEntry.muFirstIndexCount; ++lu)
            {
                lEntry.mau32FirstIndices[lu] = lrPlan.mb32Bit
                    ? reinterpret_cast<const u32*>(lpPayload)[lu]
                    : reinterpret_cast<const u16*>(lpPayload)[lu];
            }
            // D3D9 consumes a vertex range as well as an index run. Compute the
            // exact range once from the final (possibly reset-expanded) payload,
            // instead of advertising the whole shared vertex buffer every draw.
            // Lists can contain an incomplete trailing primitive which D3D never
            // reads. Its padding/reset sentinel must not widen the native range.
            u64 luConsumed = luAvailable;
            switch (lEntry.miPrimitiveType)
            {
            case D3DPT_POINTLIST: luConsumed = lEntry.muPrimitiveCount; break;
            case D3DPT_LINELIST: luConsumed = u64(lEntry.muPrimitiveCount) * 2u; break;
            case D3DPT_LINESTRIP: luConsumed = u64(lEntry.muPrimitiveCount) + 1u; break;
            case D3DPT_TRIANGLELIST: luConsumed = u64(lEntry.muPrimitiveCount) * 3u; break;
            case D3DPT_TRIANGLESTRIP:
            case D3DPT_TRIANGLEFAN: luConsumed = u64(lEntry.muPrimitiveCount) + 2u; break;
            }
            if (luConsumed > luAvailable) luConsumed = luAvailable;
            if (sbSpecializedIndexRange)
            {
                const IndexRangePC::Range lRange = lrPlan.mb32Bit
                    ? IndexRangePC::Scan<u32>(lpPayload, static_cast<size_t>(luConsumed))
                    : IndexRangePC::Scan<u16>(lpPayload, static_cast<size_t>(luConsumed));
                lEntry.muMinIndex = lRange.muMin;
                lEntry.muMaxIndex = lRange.muMax;
            }
            else
            {
                lEntry.muMinIndex = luConsumed ? 0xffffffffu : 0u;
                for (u32 lu = 0; lu < luConsumed; ++lu)
                {
                    const u32 luIndex = lrPlan.mb32Bit
                        ? reinterpret_cast<const u32*>(lpPayload)[lu]
                        : reinterpret_cast<const u16*>(lpPayload)[lu];
                    if (luIndex < lEntry.muMinIndex) lEntry.muMinIndex = luIndex;
                    if (luIndex > lEntry.muMaxIndex) lEntry.muMaxIndex = luIndex;
                }
            }
        }

        FrameProfile::CycleScope lRegisterProfile(FrameProfile::GEOMETRY_REGISTER);
        RetainedIndexBuffer& lrStored = RegisterMirror(sIndexBuffers, sIndexPages,
            sIndexTokenPages, sIndexReferences, lrKey, lEntry);
        return lrStored.mpBuffer != nullptr ? &lrStored : nullptr;
    }

    // The only place a WorldGeometryDraw is composed, so the front-cache hit path and the
    // map path cannot drift apart in what they hand the caller.
    inline void FillDraw(const RetainedVertexBuffer& lrVertex,
                         const RetainedIndexBuffer& lrIndex,
                         WorldGeometryDraw* lpOutDraw)
    {
        lpOutDraw->mpVertexBuffer   = lrVertex.mpBuffer;
        lpOutDraw->mpIndexBuffer    = lrIndex.mpBuffer;
        lpOutDraw->muExpandedStride = lrVertex.muStride;
        lpOutDraw->muNumVertices    = lrVertex.muNumVertices;
        lpOutDraw->muVertexOffset   = lrVertex.mAllocation.muOffset;
        lpOutDraw->muIndexStart     = lrIndex.muIndexStart;
        lpOutDraw->muMinIndex       = lrIndex.muMinIndex;
        lpOutDraw->muMaxIndex       = lrIndex.muMaxIndex;
        lpOutDraw->miPrimitiveType  = lrIndex.miPrimitiveType;
        lpOutDraw->muPrimitiveCount = lrIndex.muPrimitiveCount;
        lpOutDraw->muFirstIndexCount = lrIndex.muFirstIndexCount;
        lpOutDraw->mau32FirstIndices[0] = lrIndex.mau32FirstIndices[0];
        lpOutDraw->mau32FirstIndices[1] = lrIndex.mau32FirstIndices[1];
        lpOutDraw->mau32FirstIndices[2] = lrIndex.mau32FirstIndices[2];
    }

    inline bool RangeHit(const void* lpHeader, const u8* lpBegin, const u8* lpEnd,
                         const u8* lpFreeBegin, const u8* lpFreeEnd)
    {
        const u8* const lpHeaderBytes = static_cast<const u8*>(lpHeader);
        if (lpHeaderBytes >= lpFreeBegin && lpHeaderBytes < lpFreeEnd)
            return true;
        return lpBegin < lpFreeEnd && lpEnd > lpFreeBegin;
    }

    template<class TMap, class TReferences>
    void SweepTokenPages(TokenPageIndex& lrPages, TMap& lrMap, TReferences& lrReferences,
                         const u8* lpBegin, const u8* lpEnd, u64& luBytes, u32& luEvicted)
    {
        const uintptr_t luLast = PageOf(lpEnd - 1);
        for (uintptr_t luPage = PageOf(lpBegin); luPage <= luLast; ++luPage)
        {
            auto lPage = lrPages.find(luPage);
            if (lPage == lrPages.end()) continue;
            auto& lrTokens = lPage->second;
            size_t luKept = 0;
            for (u64 luToken : lrTokens)
            {
                auto* lpNode = lrReferences.Find(luToken);
                if (!lpNode) continue;
                auto& lrEntry = lpNode->second;
                if (!RangeHit(lrEntry.mpHeader, lrEntry.mpSourceBegin,
                              lrEntry.mpSourceEnd, lpBegin, lpEnd))
                {
                    lrTokens[luKept++] = luToken;
                    continue;
                }
                const auto lKey = lpNode->first;
                RetireFrontCache();
                lrReferences.Retire(luToken);
                if (lrEntry.mpBuffer)
                {
                    if (lrEntry.mAllocation) sGeometryPool.Retire(lrEntry.mAllocation);
                    else lrEntry.mpBuffer->Release();
                    luBytes -= lrEntry.muBytes;
                    ++luEvicted;
                    FrameProfile::Retire();
                }
                lrMap.erase(lKey);
            }
            lrTokens.resize(luKept);
            if (lrTokens.empty()) lrPages.erase(lPage);
        }
    }
}

EWorldGeometryPrepare WorldGeometry_Prepare(const WorldGeometryVertexPlan& lrVertexPlan,
                                            const WorldGeometryIndexPlan& lrIndexPlan,
                                            WorldGeometryDraw* lpOutDraw)
{
    renderengine::FrameProfile::DetailScope lDetailProfile(renderengine::FrameProfile::GEOMETRY_LOOKUP);
    if (Dev() == nullptr || lrVertexPlan.mpData == nullptr || lrIndexPlan.mpRun == nullptr
        || lrVertexPlan.muNumVertices == 0 || lrVertexPlan.muExpandedStride == 0
        || lrIndexPlan.muIndexCount == 0)
        return E_WORLDGEOMETRY_UNAVAILABLE;

    // ---- fast path: the same mesh, drawn again ---------------------------------------
    // Steady state is a repeat draw, so check cached candidates before going
    // near either map. The hit is taken ONLY when both conditions hold:
    //   * the slot was filled at the CURRENT generation -- nothing has been erased since,
    //     so its borrowed buffer allocations are still live (see suGeometryGeneration);
    //   * BOTH stored keys match these plans in full -- the same identity RawEqual would
    //     have tested, so the mirrors returned are exactly the ones the map lookups would
    //     have found, under this plan's stride/primitive type and no other's.
    // Anything else -- retired candidates, colliding identities, a first-ever
    // draw -- is a miss and falls through to the maps unchanged.
    const u64 luHash = FrontCacheHash(lrVertexPlan, lrIndexPlan);
    GeometryFrontCacheEntry* lpLegacySlot = nullptr;
    const GeometryFrontCacheEntry* lpCached = nullptr;
    const auto lMatches = [&](const GeometryFrontCacheEntry& lrEntry) {
        return VertexKeyMatchesPlan(lrEntry.mVertexKey, lrVertexPlan)
            && IndexKeyMatchesPlan(lrEntry.mIndexKey, lrIndexPlan);
    };
    if (sbAssociativeFrontCache)
        lpCached = sAssociativeFrontCache.Find(luHash, suGeometryGeneration, lMatches);
    else
    {
        lpLegacySlot = &saFrontCache[static_cast<u32>(luHash >> 32) & (KU_FRONT_CACHE_ENTRIES - 1u)];
        if (lpLegacySlot->muGeneration == suGeometryGeneration && lMatches(*lpLegacySlot))
            lpCached = lpLegacySlot;
    }
    if (lpCached)
    {
        ++suFrontCacheHits;
        *lpOutDraw = lpCached->mDraw;
        return E_WORLDGEOMETRY_READY;
    }
    ++suFrontCacheMisses;

    // ---- slow path: the maps, exactly as before --------------------------------------
    // Sized once for the measured live set (~10k VB / ~5k IB mirrors while driving) so
    // the find walks short bucket chains from the start instead of rehashing its way up
    // through a dozen doublings. It sits here rather than above the fast path because a
    // slot can only be filled by this path, so nothing can hit the cache before the first
    // insert -- and the fast path is left with no statics to test.
    static bool sbReserved = false;
    if (!sbReserved)
    {
        sbReserved = true;
        sVertexBuffers.reserve(16384u);
        sIndexBuffers.reserve(8192u);
    }

    const VertexKey lVertexKey = MakeVertexKey(lrVertexPlan);
    RetainedVertexBuffer* const lpVertex = AcquireVertexBuffer(lrVertexPlan, lVertexKey);
    if (lpVertex == nullptr)
        return E_WORLDGEOMETRY_UNAVAILABLE;

    const IndexKey lIndexKey = MakeIndexKey(lrIndexPlan);
    bool lbSkip = false;
    RetainedIndexBuffer* const lpIndex = AcquireIndexBuffer(lrIndexPlan, lIndexKey, &lbSkip);
    if (lpIndex == nullptr)
        return lbSkip ? E_WORLDGEOMETRY_SKIP : E_WORLDGEOMETRY_UNAVAILABLE;

    // Only a READY outcome takes a slot. The cached-failure and SKIP outcomes are rare,
    // already answered by a single map probe, and leaving them out is what lets a filled
    // slot mean one unambiguous thing -- "these two mirrors, submit them" -- with no
    // outcome field for the hit path to test.
    //
    // The generation is read AFTER the acquires on purpose: it is the generation these two
    // allocations are live under. Nothing on the acquire path can erase a node (creation only
    // inserts, and D3D9's own managed-pool eviction does not run our free hook), so it
    // cannot have moved between the lookup and this store.
    FillDraw(*lpVertex, *lpIndex, lpOutDraw);
    const GeometryFrontCacheEntry lEntry{suGeometryGeneration, lVertexKey, lIndexKey, *lpOutDraw};
    if (sbAssociativeFrontCache)
        sAssociativeFrontCache.Store(luHash, suGeometryGeneration, lEntry);
    else
        *lpLegacySlot = lEntry;

    ReportIfDue();
    return E_WORLDGEOMETRY_READY;
}

s32 WorldGeometry_Submit(const WorldGeometryDraw& lrDraw, u32 luBaseVertexIndex)
{
    FrameProfile::Scope lSubmitProfile(FrameProfile::GEOMETRY_SUBMIT);
    FrameProfile::Draw();
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lrDraw.mpVertexBuffer == nullptr
        || lrDraw.mpIndexBuffer == nullptr)
        return static_cast<s32>(E_FAIL);

    const GeometryBindingsPC::VertexWindow lWindow = GeometryBindingsPC::Rebase(
        lrDraw.muVertexOffset, lrDraw.muExpandedStride, static_cast<INT>(luBaseVertexIndex));
    if (lWindow.muOffset != lrDraw.muVertexOffset) FrameProfile::RebasedDraw();
    HRESULT lhBind = GeometryBindingsPC::gCache.BindVertex(lpDevice,
        static_cast<IDirect3DVertexBuffer9*>(lrDraw.mpVertexBuffer), lWindow.muOffset, lrDraw.muExpandedStride);
    if (FAILED(lhBind)) return static_cast<s32>(lhBind);
    lhBind = GeometryBindingsPC::gCache.BindIndex(lpDevice,
        static_cast<IDirect3DIndexBuffer9*>(lrDraw.mpIndexBuffer));
    if (FAILED(lhBind)) return static_cast<s32>(lhBind);

    // [DIAG] THE WORLD PASS'S OWN DEPTH STATE, at the moment a world mesh actually draws.
    // The tyre mark is rejected by the depth test against whatever this pass left in the
    // buffer, and a comparison needs BOTH sides. Inert unless BRN_RT_PROBE is set and the
    // first tyre-mark segment has been laid; see BrnDiagBoundSurfaces.h. DELETE-WHEN-STABLE.
    BrnDiag::LogBoundSurfaces("world-draw", 397u, true);

    // Equivalent to the UP call this replaces: that one offset the vertex POINTER by
    // baseVertex * stride and passed base 0, so the run's index values are relative to
    // the base vertex either way.
    const HRESULT lhr = lpDevice->DrawIndexedPrimitive(
        static_cast<D3DPRIMITIVETYPE>(lrDraw.miPrimitiveType),
        lWindow.miBase,
        lrDraw.muMinIndex,
        lrDraw.muMaxIndex - lrDraw.muMinIndex + 1u,
        lrDraw.muIndexStart,
        lrDraw.muPrimitiveCount);
    return static_cast<s32>(lhr);
}

// The streamer is about to hand a heap block back, so every mirror built out of that
// block has to go with it -- otherwise the next track unit loaded over those bytes
// would be drawn with the previous one's geometry.
void WorldGeometry_OnResourceMemoryFreed(const void* lpBase, size_t luSize)
{
    WorldVd32_OnResourceMemoryFreed(lpBase, luSize);
    // No "maps are empty" early-out here: a resource's header and its buffer bytes live in
    // DIFFERENT memtype blocks (renderable descriptor slot 0 = the 0x28-byte header, slot 2
    // = the vertex/index bytes), so one resource produces two notifications, and the page
    // buckets registered under the SECOND block still have to be swept even when the first
    // one already erased the last live mirror -- otherwise those buckets leak keys forever.
    if (lpBase == nullptr || luSize == 0)
        return;

    const u8* const lpFreeBegin = static_cast<const u8*>(lpBase);
    const u8* const lpFreeEnd   = lpFreeBegin + luSize;
    if (sbCompactLifetime)
    {
        SweepTokenPages(sVertexTokenPages, sVertexBuffers, sVertexReferences,
                        lpFreeBegin, lpFreeEnd, suVertexBytes, suVertexBuffersEvicted);
        SweepTokenPages(sIndexTokenPages, sIndexBuffers, sIndexReferences,
                        lpFreeBegin, lpFreeEnd, suIndexBytes, suIndexBuffersEvicted);
        return;
    }
    const uintptr_t luFirstPage = PageOf(lpFreeBegin);
    const uintptr_t luLastPage  = PageOf(lpFreeEnd - 1);

    for (uintptr_t luPage = luFirstPage; luPage <= luLastPage; ++luPage)
    {
        VertexPageIndex::iterator lVertexPage = sVertexPages.find(luPage);
        if (lVertexPage != sVertexPages.end())
        {
            std::vector<VertexKey>& lrKeys = lVertexPage->second;
            size_t luKept = 0;
            for (size_t lu = 0; lu < lrKeys.size(); ++lu)
            {
                VertexMap::iterator lIt = sVertexBuffers.find(lrKeys[lu]);
                if (lIt == sVertexBuffers.end())
                    continue;                       // already evicted through another page
                if (!RangeHit(lIt->second.mpHeader, lIt->second.mpSourceBegin,
                              lIt->second.mpSourceEnd, lpFreeBegin, lpFreeEnd))
                {
                    lrKeys[luKept++] = lrKeys[lu];  // still live; keep the reference
                    continue;
                }
                // BEFORE the Release AND the erase, never after: a front-cache slot may
                // hand out this allocation's mpBuffer/range, so the cache is retired
                // first and no window exists in which a slot could still be believed. One
                // increment, adjacent to the free, so the ordering cannot be lost in a later
                // edit. (Bumping per eviction rather than per sweep costs nothing -- the
                // effect is the same whole-cache retire.)
                RetireFrontCache();
                if (lIt->second.mpBuffer != nullptr)
                {
                    if (lIt->second.mAllocation) sGeometryPool.Retire(lIt->second.mAllocation);
                    else lIt->second.mpBuffer->Release();
                    suVertexBytes -= lIt->second.muBytes;
                    ++suVertexBuffersEvicted;
                    FrameProfile::Retire();
                }
                sVertexBuffers.erase(lIt);
            }
            lrKeys.resize(luKept);
            if (lrKeys.empty())
                sVertexPages.erase(lVertexPage);
        }

        IndexPageIndex::iterator lIndexPage = sIndexPages.find(luPage);
        if (lIndexPage != sIndexPages.end())
        {
            std::vector<IndexKey>& lrKeys = lIndexPage->second;
            size_t luKept = 0;
            for (size_t lu = 0; lu < lrKeys.size(); ++lu)
            {
                IndexMap::iterator lIt = sIndexBuffers.find(lrKeys[lu]);
                if (lIt == sIndexBuffers.end())
                    continue;
                if (!RangeHit(lIt->second.mpHeader, lIt->second.mpSourceBegin,
                              lIt->second.mpSourceEnd, lpFreeBegin, lpFreeEnd))
                {
                    lrKeys[luKept++] = lrKeys[lu];
                    continue;
                }
                RetireFrontCache();     // before the Release and the erase; see the vertex sweep
                if (lIt->second.mpBuffer != nullptr)
                {
                    if (lIt->second.mAllocation) sGeometryPool.Retire(lIt->second.mAllocation);
                    else lIt->second.mpBuffer->Release();
                    suIndexBytes -= lIt->second.muBytes;
                    ++suIndexBuffersEvicted;
                    FrameProfile::Retire();
                }
                sIndexBuffers.erase(lIt);
            }
            lrKeys.resize(luKept);
            if (lrKeys.empty())
                sIndexPages.erase(lIndexPage);
        }
    }
}

void WorldGeometry_BeginFrame()
{
    sGeometryPool.BeginFrame();
}

void WorldGeometry_ReleaseAll()
{
    GeometryBindingsPC::gCache.Invalidate();
    WorldVd32_ReleaseAll();
    // First, before a single allocation dies: front-cache draws borrow those buffers,
    // so retire them all up front (unconditionally here -- this erases
    // everything by definition).
    RetireFrontCache();
    suFrontCacheHits   = 0;
    suFrontCacheMisses = 0;

    for (VertexMap::iterator lIt = sVertexBuffers.begin(); lIt != sVertexBuffers.end(); ++lIt)
    {
        sVertexReferences.Retire(lIt->second.muEvictionToken);
        if (lIt->second.mpBuffer != nullptr && !lIt->second.mAllocation)
            lIt->second.mpBuffer->Release();
    }
    for (IndexMap::iterator lIt = sIndexBuffers.begin(); lIt != sIndexBuffers.end(); ++lIt)
    {
        sIndexReferences.Retire(lIt->second.muEvictionToken);
        if (lIt->second.mpBuffer != nullptr && !lIt->second.mAllocation)
            lIt->second.mpBuffer->Release();
    }
    sGeometryPool.ReleaseAll();
    sVertexBuffers.clear();
    sIndexBuffers.clear();
    sVertexPages.clear();
    sIndexPages.clear();
    sVertexTokenPages.clear();
    sIndexTokenPages.clear();
    suVertexBytes = 0;
    suIndexBytes  = 0;
}

}   // namespace renderengine
