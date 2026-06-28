#pragma once

// ===========================================================================
// EATech Apt -- AptDisplayList: a movie-clip's child display list.
//
// The depth-ordered list of AptCIH scene nodes a sprite/movie-clip instance has
// placed (placeObject / instantiateCharacter / AddToDisplayList add to it; tick /
// GeneralisedProcess / GetBoundingRect walk it). Unlike the lightweight
// AptDisplayListState (which IS the head pointer, embedded directly), AptDisplayList
// owns a single pointer to a small pool-allocated HEAD NODE; the actual entries
// (AptCIH*) chain off that node. It is embedded BY VALUE inside the sprite character
// instances (AptCharacterSpriteInstBase at +0x1C), so this header pins its 1-pointer
// layout for those owners; the large behavioural surface (tick/placeObject/...) is
// reconstructed by its own follow-on TUs and extends this header.
//
// SHAPE + the ctor/dtor/PreDestroy/clear BODIES from the X360 ARTIST.XEX (the
// authoritative spine):
//     AptDisplayList::AptDisplayList   @ 0x82AE4850  (allocate + zero the 4-byte head node)
//     AptDisplayList::~AptDisplayList  @ 0x82AFE9F0  (clear, then free the head node)
//     AptDisplayList::PreDestroy       @ 0x82AFD2B8  (clear + free + null the head)
//     AptDisplayList::clear            @ 0x82AFD1F8  (walk the entries, release each)
//
// LAYOUT (4 bytes / 1 dword; the ctor's `*this = Allocate(pool, 4)` pins it):
//   +0x00  mpHead   the pool-allocated head node (its +0 dword points at the first
//                    listed AptCIH; null until the pool alloc, or after PreDestroy).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptCIH;

// The 4-byte pool-allocated head/sentinel node: its single dword is the first
// listed entry (the AptCIH chain is then linked through the CIH display-list links).
struct AptDisplayListNode
{
    AptCIH* mpFirst;   // +0x00 first listed scene node (null when empty)
};

struct AptDisplayList
{
    AptDisplayListNode* mpHead;   // +0x00 pool-allocated head node (4 bytes)

    // ctor @0x82AE4850 -- pool-allocate a zeroed 4-byte head node (or null on
    // allocation failure) and store it. Body in AptDisplayList.cpp (its own TU).
    AptDisplayList();

    // dtor @0x82AFE9F0 -- clear() the listed entries, then return the head node to
    // the pool (4 bytes). Body in AptDisplayList.cpp (its own TU).
    ~AptDisplayList();

    // PreDestroy @0x82AFD2B8 -- when a head node exists: clear() the entries, free
    // the head node (4 bytes), then null mpHead. Body in AptDisplayList.cpp.
    void PreDestroy();

    // clear @0x82AFD1F8 -- walk the listed AptCIH entries releasing each (optionally
    // dropping each one's GC-root + ClearCIH when bClearGCRoots). Body in
    // AptDisplayList.cpp (its own TU).
    void clear(bool bClearGCRoots);
};
