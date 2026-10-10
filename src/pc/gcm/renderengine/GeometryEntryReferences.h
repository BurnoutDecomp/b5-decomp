#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace renderengine
{
    // FLAG PC-platform leaf: compact references from source-memory pages to
    // retained geometry entries. Unordered-map nodes survive rehash; retired
    // nodes must be removed here before their owning map erases them. Generations
    // make page references harmless after slot or source-address reuse.
    // Serialized by the same frame/resource boundary as the geometry maps.
    template<class TEntry, std::uint32_t KU_MAX_GENERATION = UINT32_MAX>
    class GeometryEntryReferencesPC
    {
    public:
        using Token = std::uint64_t;
        static_assert(KU_MAX_GENERATION > 0, "nonzero generation");

        Token Add(TEntry* lpEntry)
        {
            if (!lpEntry) return 0;
            std::uint32_t luIndex = muFree;
            if (luIndex == KU_NO_SLOT)
            {
                if (mSlots.size() >= KU_NO_SLOT)
                    throw std::length_error("geometry reference capacity");
                luIndex = static_cast<std::uint32_t>(mSlots.size());
                mSlots.emplace_back();
            }
            else muFree = mSlots[luIndex].muNext;
            Slot& lrSlot = mSlots[luIndex];
            lrSlot.mpEntry = lpEntry;
            return (static_cast<Token>(lrSlot.muGeneration) << 32) | luIndex;
        }

        TEntry* Find(Token luToken) const
        {
            const std::uint32_t luIndex = static_cast<std::uint32_t>(luToken);
            if (luIndex >= mSlots.size()) return nullptr;
            const Slot& lrSlot = mSlots[luIndex];
            return lrSlot.muGeneration == (luToken >> 32) ? lrSlot.mpEntry : nullptr;
        }

        bool Retire(Token luToken)
        {
            if (!Find(luToken)) return false;
            const std::uint32_t luIndex = static_cast<std::uint32_t>(luToken);
            Slot& lrSlot = mSlots[luIndex];
            lrSlot.mpEntry = nullptr;
            // Never wrap into an earlier identity. Exhausted slots stay dead.
            if (lrSlot.muGeneration != KU_MAX_GENERATION)
            {
                ++lrSlot.muGeneration;
                lrSlot.muNext = muFree;
                muFree = luIndex;
            }
            return true;
        }

        std::size_t StorageSlots() const { return mSlots.size(); }

    private:
        static constexpr std::uint32_t KU_NO_SLOT = UINT32_MAX;
        struct Slot
        {
            TEntry* mpEntry = nullptr;
            std::uint32_t muGeneration = 1, muNext = KU_NO_SLOT;
        };
        std::vector<Slot> mSlots;
        std::uint32_t muFree = KU_NO_SLOT;
    };
}
