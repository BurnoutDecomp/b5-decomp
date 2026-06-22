#ifndef CGS_SOUND_PLAYBACK_CGSCOMMON_H
#define CGS_SOUND_PLAYBACK_CGSCOMMON_H

#include "types.hpp"

// CgsSound::Playback::Name - the sound-playback name-interning facility.
//
// FLAG: MINIMAL FLAGGED HOME, grown as dependents need it. Modelled so far:
// Name::MakeHash + Name::HashTable::Store (the two boot-trace functions) plus the
// data they touch (Name::mHash for shape, the nested HashNode struct, the
// HashTable static pool), AND the Name(const char*) ctor (added for the AEMS
// param-index static-init TUs -- it is inlined-only on X360, see below). The full
// CgsCommon.h (DWARF CgsCommon.h:25-729) is large: typedef Ident,
// KU32_DEFAULT_ALIGNMENT, the reserved-Ident constants (K_MIN_RESERVED_IDENT /
// K_INIT_SND9_SUBMIX_IDENT = 0xFFFFFFF0), K_NULL_NAME, the remaining Name ctors
// (Name() / Name(uintptr_t) / Name(const Name&)) / operators / GetValue /
// GetCString / IsGenuine / Dump / TraverseHashTable / PointerToName / EndianSwap,
// HashNode::HashNode / Clear / Dump / Traverse, and HashTable::Retrieve / Dump /
// Traverse, plus the whole Playback subsystem (Entity, ContentClass, Slot,
// Factory, ...). All DEFERRED to their own TUs -- add only when a TU needs it.

namespace CgsSound
{
namespace Playback
{

// CgsCommon.h:102. Interned sound-playback name: a 32-bit hash with an interning
// side-table so equal names share one hash and collisions are caught.
struct Name
{
public:
    // CgsCommon.h:126. Construct from a C string: hash + intern it. Constructing a
    // Name from a string interns it (MakeHash -> Store), which is exactly what the
    // AEMS param-index static initializers do. INLINED-ONLY on X360 (no out-of-line
    // address -> no own TU), so it is defined inline here; the body matches the
    // inlined static-init pseudocode (mHash = MakeHash(...)). FLAG: home-grow added
    // for the AEMS param-index TUs (BrnAutoGenAemsParameterIndexes.*).
    explicit Name(const char* lkpacName) { mHash = MakeHash(lkpacName); }

    // CgsCommon.h. The raw interned hash value. FLAG: ADDITIVE home-grow for the
    // CgsSound::Playback::Registry TU (Registry::AddEntity reads an Entity's
    // mName/mTypeName hash words directly -- `*a2` / `a2[1]` in the X360 asm).
    // The DWARF lists Name::GetValue (CgsCommon.h notes); modelled here as the
    // trivial mHash getter so dependents can read the hash by name instead of by
    // raw offset. Shape-only -- not a recon'd standalone TU.
    uintptr_t GetValue() const { return mHash; }

private:
    // CgsCommon.h:249. The interned hash value. NOT touched by MakeHash/Store
    // (those are static and operate on the HashTable pool); future Name ctor TUs
    // assign it from MakeHash's return. Modelled for shape completeness. FLAG.
    uintptr_t mHash;

    // CgsCommon.h:269. One interning-table node (16 bytes on X360: 4x4-byte
    // words -- verified by the `slwi r8,r8,4` 16-byte node stride and the four
    // word stores at +0/+4/+8/+0xC in Store @ 0x82689880).
    struct HashNode
    {
        uintptr_t       mHash;      // CgsCommon.h:284  (+0)  the node's hash
        const char*     mkpacName;  // CgsCommon.h:285  (+4)  interned string ptr
        HashNode*       mpLess;     // CgsCommon.h:286  (+8)  subtree: lower hash
        HashNode*       mpMore;     // CgsCommon.h:287  (+0xC) subtree: higher hash

        // HashNode::Clear() (CgsCommon.h:274) zeroes the four words. In Store it
        // is INLINED (the four `stw r9,...` stores), so it is not called out as a
        // separate function here. The standalone Clear()/ctor/Dump/Traverse are
        // DEFERRED to the HashNode TU. FLAG.
    };

    // CgsCommon.h:292. The global name-interning side-table: 32 BST buckets over
    // a fixed pool of 2048 nodes. All state is static (per-process), defined in
    // the .cpp.
    struct HashTable
    {
    public:
        // CgsCommon.h:297. Intern (hash, name) into the table. STATIC despite the
        // X360 mangling/DWARF (it touches only the static pool, never a `this`).
        // Logically returns void -- the X360 leaves the hash in r3 but MakeHash
        // discards it and re-loads its own. FLAG (see .cpp).
        static void Store(uintptr_t luHash, const char* lkpacName);

    private:
        // CgsCommon.h:312-314. The static pool, defined in CgsCommon.cpp.
        //   sapHashNode   == dword_82FFB958  (bucket heads)
        //   saNodes       == unk_82FFDF20    (node pool)
        //   su32CurrentNode == dword_82FFB9D8 (next-free index)
        static HashNode* sapHashNode[32];
        static HashNode  saNodes[2048];
        static u32       su32CurrentNode;

        // Retrieve / Dump / Traverse (CgsCommon.h:301-309) DEFERRED. FLAG.
    };

    // CgsCommon.h:247. Compute the interning hash of a C string and Store it.
    // STATIC despite the DWARF/mangling (its r3 is the char*, not a `this`). FLAG.
    static uintptr_t MakeHash(const char* lkpacName);
};

}
}

#endif // CGS_SOUND_PLAYBACK_CGSCOMMON_H
