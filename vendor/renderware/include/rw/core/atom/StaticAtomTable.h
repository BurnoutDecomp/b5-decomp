#ifndef RW_CORE_ATOM_STATICATOMTABLE_H
#define RW_CORE_ATOM_STATICATOMTABLE_H

#include <cstdint>

// ===========================================================================
// rw::core::atom::StaticAtomTable -- the RenderWare core "atom" string-interning
// table: a fixed-capacity open-addressing (linear-probe) hash table mapping a
// NUL-terminated byte string to a 16-bit atom id, backed by a packed string
// pool and an id->offset side table.
//
// OWNING HOME for the four X360 members (all scalar integer code):
//     rw::core::atom::StaticAtomTable::HashIDArrayIndex  @ 0x82C478A8
//     rw::core::atom::StaticAtomTable::Register          @ 0x82C47900
//     rw::core::atom::StaticAtomTable::FindID            @ 0x82C47A40
//     rw::core::atom::StaticAtomTable::At                @ 0x82C47B88
//
// LAYOUT (pinned from the X360 member offsets; the object is a thin handle that
// holds one pointer to its data block, dereferenced as `*this` at the head of
// every member -- `lwz r11, 0(r3)`):
//
//   StaticAtomTable                 (the `this` handle)
//     +0   Data* mpData
//
//   Data                            (the data block reached via mpData)
//     +0x00  uint8_t*  mpStringPoolBase    ; base of the packed NUL-terminated names
//     +0x04  uint8_t*  mpStringPoolCursor  ; write cursor (== base + used bytes)
//     +0x08  uint32_t* mpIdToOffset        ; id -> byte offset into the string pool
//                                          ;   (indexed by atom id, 4-byte stride)
//     +0x0C  uint16_t* mpHashSlots         ; hash slot -> atom id (2-byte stride);
//                                          ;   0xFFFF marks an empty slot
//     +0x10  uint16_t  muHashSlotCount     ; number of hash slots (the probe modulus)
//     +0x12  uint16_t  muFreeSlots         ; remaining insert capacity (Register --)
//
// All four members are integer/byte code (lwz/lhz/lbz/sth/stb/stw + divwu); no
// VMX. The hash is rw::RwHash32String seeded with the 32-bit FNV-1a offset basis
// (0x811C9DC5), reduced modulo muHashSlotCount to a starting slot.
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

class StaticAtomTable
{
public:
    // The block of arrays/counters the handle points at. Member-by-name access;
    // the X360 offsets are reproduced by field declaration order.
    struct Data
    {
        uint8_t*  mpStringPoolBase;    // +0x00
        uint8_t*  mpStringPoolCursor;  // +0x04
        uint32_t* mpIdToOffset;        // +0x08
        uint16_t* mpHashSlots;         // +0x0C
        uint16_t  muHashSlotCount;     // +0x10
        uint16_t  muFreeSlots;         // +0x12
    };

    // Empty-slot sentinel stored in mpHashSlots (the X360 0xFFFF compare).
    static const uint16_t KU_EMPTY_SLOT = 0xFFFFu;

    // Reduce a string key to its starting hash slot: FNV-1a(key) % slotCount.
    // (X360 @ 0x82C478A8.) The unsigned modulo matches the X360 divwu; the
    // `__twllei(slotCount, 0)` trap-if-zero is a divide-by-zero guard and is not
    // reproduced (it is undefined behaviour to call with an unsized table).
    uint32_t HashIDArrayIndex(const char* lpcKey) const;

    // Intern a key under the explicit atom id `luAtomId`: linear-probe from the
    // hash slot; if the key is already present return its (matching) string ptr,
    // otherwise claim the first empty slot, append the key to the string pool,
    // record its id->offset, and decrement the free-slot counter.
    // (X360 @ 0x82C47900.)
    void Register(const char* lpcKey, uint32_t luAtomId);

    // Look up `lpcKey`: linear-probe from the hash slot. On a hit, write the
    // matching atom id through lpuOutId and return true; on a full-cycle miss or
    // an empty slot, write that slot's content through lpuOutId and return false.
    // (X360 @ 0x82C47A40, returns 1/0.)
    bool FindID(const char* lpcKey, uint16_t* lpuOutId) const;

    // Convenience lookup: return the atom id FindID writes (0xFFFF on an
    // empty-slot miss). (X360 @ 0x82C47B88.)
    uint16_t At(const char* lpcKey) const;

private:
    Data* mpData;  // +0
};

}  // namespace atom
}  // namespace core
}  // namespace rw

#endif  // RW_CORE_ATOM_STATICATOMTABLE_H
