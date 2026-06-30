#pragma once

// ===========================================================================
// EATech Apt -- AptDisplayListState: the per-frame display list of a movie clip.
//
// A depth-sorted doubly-linked list of the AptCIH scene nodes currently placed in
// a movie clip. The list links live in the AptCIHs themselves (mpDisplayList
// Previous/Next/Parent); this object is just the head pointer + the list ops
// (insert by depth, remove, find by depth/name, depth swap). As the list mutates
// it notifies the render-tree manager so the render tree tracks the change.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (19AptDisplayListState). LAYOUT: a
// single AptCIH* head (4 bytes; the ctor zeroes it). The CIH list links are at
// AptCIH [5]/[6]/[7] (prev/next/parent), which the render spine already defines.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"

struct AptCharacter;
class AptValue;
class  EAStringC;

struct AptDisplayListState
{
    AptCIH* mpFirst;   // head of the depth-sorted CIH list

    AptDisplayListState() : mpFirst(0) {}   // @0x7E3E7C

    AptCIH* GetFirstItem() const { return mpFirst; }   // @0x7DEFC8
    int     getLength() const;                          // @0x7E6890
    AptCIH* getValue(int nIndex) const;                 // @0x7E68B8

    // Insert pItem after pAfter (head-insert when pAfter is null), ref-counting it
    // + notifying the render-tree manager. @0x7FA508
    AptCIH* insert(AptCIH* pAfter, AptCIH* pItem);
    // Insert pItem at depth nDepth (found via findInst), then stamp its depth. @0x7FA650
    AptCIH* insert(int nDepth, AptCIH* pItem);

    // Unlink pItem from the list + notify the manager. @0x7FA8C4 / @0x7FA9CC
    AptCIH* removeItem(AptCIH* pItem);
    AptCIH* remove(AptCIH* pItem);

    // ChangeDepth @0x82AEE7E0 -- re-position pItem to nDepth (unlink + depth-sorted
    // re-insert + render-tree notify + stamp the render item's depth). Returns pItem.
    AptCIH* ChangeDepth(int16_t nDepth, AptCIH* pItem);
    // swapDepths @0x82AEE560 -- exchange two nodes' list positions AND render-item depths.
    void    swapDepths(AptCIH* pA, AptCIH* pB);

    // Locate by name (then by depth): outPrev = the insert-after node, outMatch =
    // the node at that name/depth (or null). @0x7EE8A0
    void findInst(int nDepth, const EAStringC* pName, AptCIH** ppOutPrev, AptCIH** ppOutMatch) const;

    // GC mark each listed CIH. @0x7E68FC
    void RegisterReferences(const AptValue* pOwner) const;

    // AddToDelayReleaseList @0x82AFD... (X360-attested behavioural follow-on; body
    // in its own TU) -- unlink pItem from the live list and queue it on the
    // delayed-release list (so a node removed mid-tick is released safely after the
    // walk). Declared so AptDisplayList::removeObject can call it by name.
    AptCIH* AddToDelayReleaseList(AptCIH* pItem, bool bDelay);
};
