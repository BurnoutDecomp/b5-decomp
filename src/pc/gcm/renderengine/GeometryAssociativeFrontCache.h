#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <emmintrin.h>
#include <intrin.h>

// FLAG PC-platform leaf: a bounded front cache for retained native geometry.
// Fingerprints only select candidates. The caller must validate the complete
// identity before returning borrowed buffers, and advance the generation before
// retiring them. A generation value may be reused only after Clear().
namespace renderengine {
template<class Entry, std::size_t SetCount> class GeometryAssociativeFrontCachePC {
    static_assert(SetCount && !(SetCount & (SetCount - 1)), "power-of-two set count");
    static_assert(std::is_trivially_copyable_v<Entry>, "borrowed metadata only");
    struct alignas(16) Set {
        std::uint64_t muGeneration = 0;
        unsigned muNext = 0;
        alignas(16) std::uint32_t mauTags[4]{};
    };
    static_assert(sizeof(Set) == 32, "compact metadata");
    std::array<Set, SetCount> maSets{};
    std::array<Entry, SetCount * 4> maEntries{};
    static std::size_t Index(std::uint64_t luHash) { return (luHash >> 32) & (SetCount - 1); }
    static std::uint32_t Tag(std::uint64_t luHash) { return static_cast<std::uint32_t>(luHash) | 1u; }
    static unsigned Matches(const Set& lrSet, std::uint32_t luTag) {
        const __m128i lValues = _mm_load_si128(reinterpret_cast<const __m128i*>(lrSet.mauTags));
        return static_cast<unsigned>(_mm_movemask_ps(_mm_castsi128_ps(
            _mm_cmpeq_epi32(lValues, _mm_set1_epi32(static_cast<int>(luTag))))));
    }
public:
    template<class Equal> const Entry* Find(std::uint64_t luHash, std::uint64_t luGeneration, Equal lEqual) const {
        const auto luIndex = Index(luHash);
        const auto& lrSet = maSets[luIndex];
        if (lrSet.muGeneration != luGeneration) return nullptr;
        unsigned luCandidates = Matches(lrSet, Tag(luHash));
        while (luCandidates) {
            unsigned long luWay;
            _BitScanForward(&luWay, luCandidates);
            const auto& lrEntry = maEntries[luIndex * 4 + luWay];
            if (lEqual(lrEntry)) return &lrEntry;
            luCandidates &= luCandidates - 1;
        }
        return nullptr;
    }
    void Store(std::uint64_t luHash, std::uint64_t luGeneration, const Entry& lrEntry) {
        const auto luIndex = Index(luHash);
        auto& lrSet = maSets[luIndex];
        if (lrSet.muGeneration != luGeneration) {
            lrSet.muGeneration = luGeneration;
            lrSet.muNext = 0;
            _mm_store_si128(reinterpret_cast<__m128i*>(lrSet.mauTags), _mm_setzero_si128());
        }
        const unsigned luEmpty = Matches(lrSet, 0);
        unsigned long luWay;
        if (luEmpty) _BitScanForward(&luWay, luEmpty);
        else { luWay = lrSet.muNext; lrSet.muNext = (lrSet.muNext + 1) & 3u; }
        maEntries[luIndex * 4 + luWay] = lrEntry;
        lrSet.mauTags[luWay] = Tag(luHash);
    }
    // Values are borrowed metadata; clearing never releases a native resource.
    void Clear() { maSets = {}; }
};
}
