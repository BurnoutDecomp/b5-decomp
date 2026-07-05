#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribedittable.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribeditspecifier.h" // Attrib::EditSpecifier / EditSpecifierLess

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"   // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::Malloc

// The two external rbtree primitives EditRecord threads together. Both are their own
// (unrecovered) X360 ledger functions; declared opaque here so the insert compiles
// (declared-not-defined -- fine at link time). EditRecordAllocNode == sub_8280EB08
// (allocate+construct a new EditSpecifier rbtree node for `key` in `tree`);
// eastl::RBTreeInsert splices the node under a parent hint and rebalances.
namespace Attrib { void* EditRecordAllocNode(void* lpTree, const EditSpecifier* lpKey); }
namespace eastl  { void  RBTreeInsert(void* lpNode, void* lpParentHint, void* lpHeader, int liInsertLeft); }

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::EditTable::operator new @ 0x82805A80
//
// The X360 body asserts sbHasLinearAllocator (the AttribSys memory manager has been
// Prepare'd; CgsAttribSysMemoryManager.h:211) and then calls
// AttribSysPackageAllocator::Malloc(&dword_83011B94, lnBytes, 0) -- i.e. it Mallocs the
// requested bytes from the static AttribSys package allocator (&dword_83011B94 ==
// AttribSysMemoryManager::sAttribSysAllocator) with flags 0. GetAttribSysAllocator()
// runs that exact sbHasLinearAllocator assert and returns &sAttribSysAllocator, so the
// allocator lookup is routed through it (member-by-name; the asm reads the private
// static directly).
namespace Attrib
{
    void* EditTable::operator new(size_t lnBytes)
    {
        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        return lpAllocator->Malloc(lnBytes, 0);
    }

    // @ 0x8280F120 -- insert one edit record keyed by *lpKey into the EditTable's
    // sorted (red-black) EditSpecifier tree. A LEFT insertion (liInsertLeft=1) is
    // chosen ONLY when the caller did not force a side (lcForce==0), the hint is not
    // the end sentinel (lpPos != tree+4), AND the comparator reports the new key is
    // NOT less than the hint's key (i.e. less(*key, *(pos+0x10)) == false). Any of
    // force / end-hint / (new key < hint key) selects a RIGHT insertion (=0). A fresh
    // node is allocated for the key, spliced under the hint against the header sentinel
    // (tree+4), the tree's size counter (tree+0x14) is bumped, and the new node is
    // handed back through *lpResult. (EditRecordAllocNode == sub_8280EB08 /
    // eastl::RBTreeInsert are their own unrecovered ledger functions; reached here as
    // opaque forward-declared primitives.)
    void** EditRecord(void** lpResult, void* lpTree, void* lpPos,
                      const EditSpecifier* lpKey, char lcForce)
    {
        u8* lpTreeBytes = reinterpret_cast<u8*>(lpTree);
        void* lpHeaderSentinel = lpTreeBytes + 4;

        int liInsertLeft = 0;
        if (!lcForce
            && lpPos != lpHeaderSentinel
            && !Attrib::EditSpecifierLess()(
                   *lpKey,
                   *reinterpret_cast<const EditSpecifier*>(
                       reinterpret_cast<u8*>(lpPos) + 0x10)))
        {
            liInsertLeft = 1;
        }

        void* lpNode = Attrib::EditRecordAllocNode(lpTree, lpKey);
        eastl::RBTreeInsert(lpNode, lpPos, lpHeaderSentinel, liInsertLeft);

        ++*reinterpret_cast<u32*>(lpTreeBytes + 0x14);   // the tree's live-record count
        *lpResult = lpNode;
        return lpResult;
    }

    // @ 0x828071D0 -- find the EditTable tree node whose EditSpecifier equals *lpKey
    // (an rbtree lower_bound followed by the exact-match check; == eastl::rbtree::find).
    // Nodes carry their two children @+0x00 and +0x04 and their EditSpecifier @+0x10;
    // the tree keeps its root @ tree+0xC and its header/end sentinel is tree+4. The
    // compare is EditSpecifierLess's lexicographic (mClassKey,mCollectionKey,mAttribKey:
    // u64; mIndex: u32) order. Returns the found node through *lpResult, or the end
    // sentinel (tree+4) when the key is absent.
    void** EditRecordFind(void** lpResult, void* lpTree, const EditSpecifier* lpKey)
    {
        u8* lpTreeBytes = reinterpret_cast<u8*>(lpTree);
        void* lpEnd = lpTreeBytes + 4;
        void* lpNode = *reinterpret_cast<void**>(lpTreeBytes + 0xC);
        void* lpCandidate = lpEnd;

        const EditSpecifierLess lLess;

        while (lpNode != NULL)
        {
            const EditSpecifier& lrNodeKey =
                *reinterpret_cast<const EditSpecifier*>(reinterpret_cast<u8*>(lpNode) + 0x10);

            if (lLess(lrNodeKey, *lpKey))   // node < search -> descend *(node+0x00)
            {
                lpNode = *reinterpret_cast<void**>(lpNode);            // +0x00
            }
            else                            // search <= node -> record + descend *(node+0x04)
            {
                lpCandidate = lpNode;
                lpNode = *reinterpret_cast<void**>(reinterpret_cast<u8*>(lpNode) + 4); // +0x04
            }
        }

        if (lpCandidate == lpEnd)
        {
            *lpResult = lpEnd;
        }
        else
        {
            const EditSpecifier& lrCandKey =
                *reinterpret_cast<const EditSpecifier*>(reinterpret_cast<u8*>(lpCandidate) + 0x10);
            // exact-match: if the lower-bound candidate is strictly greater than the
            // search key, the key is absent -> report end.
            *lpResult = lLess(*lpKey, lrCandKey) ? lpEnd : lpCandidate;
        }
        return lpResult;
    }
}
