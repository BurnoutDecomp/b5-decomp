// =====================================================================================
// rw::core::filesys -- async-op list (intrusive) + GetSize accessor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs.
//
//   rw::core::filesys::AsyncOpList::InsertAfter @0x82BBDE70  (template <AsyncOp> inst)
//   rw::core::filesys::AsyncOpList::Remove      @0x82BBDED0  (template <AsyncOp> inst)
//   rw::core::filesys::GetSize                  @0x82BBD700
//
// The list head is { mpHead@+0, mpTail@+4, muCount@+8 }; nodes link forward via
// AsyncOp::mpNext (node +0). The bodies are store-for-store ports of the pseudocode,
// with the raw _DWORD word-indices resolved to named members:
//   result/a1 -> AsyncOpList*   result[0]==mpHead  result[1]==mpTail  result[2]==muCount
//   a3 -> the node being inserted               a2 -> the predecessor / target node
// =====================================================================================

#include "asyncop.h"

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // GetSize @0x82BBD700:  ld r3, 0x18(r3); blr -- return the 64-bit size at +0x18.
            u64 GetSize(const AsyncOp* lpOp)
            {
                return lpOp->mu64Size;
            }

            // InsertAfter @0x82BBDE70.
            //   if (a2) { if (!a2->next) tail = a3; a3->next = a2->next; a2->next = a3; ++count; }
            //   else    { a3->next = head; v3 = count; head = a3; count = v3 + 1;
            //             if (!a3->next) tail = a3; }
            AsyncOpList* AsyncOpList::InsertAfter(AsyncOp* lpAfter, AsyncOp* lpNode)
            {
                if (lpAfter)
                {
                    if (!lpAfter->mpNext)
                        mpTail = lpNode;
                    lpNode->mpNext = lpAfter->mpNext;
                    lpAfter->mpNext = lpNode;
                    ++muCount;
                }
                else
                {
                    lpNode->mpNext = mpHead;
                    u32 luPrevCount = muCount;
                    mpHead = lpNode;
                    muCount = luPrevCount + 1;
                    if (!lpNode->mpNext)
                        mpTail = lpNode;
                }
                return this;
            }

            // Remove @0x82BBDED0.
            // lpFrom (X360 r5/a3) is a PREDECESSOR NODE cursor, not a pointer-to-link: the
            // walk follows lpFrom->mpNext (the asm's `*(r5)`) looking for lpNode. The head
            // case (lpNode == mpHead) is handled separately against the head/tail fields.
            //
            //   r10 = mpHead;
            //   if (lpNode != mpHead) {                       // 0x82BBDF1C
            //       if (mpHead) {
            //           if (!lpFrom) lpFrom = mpHead;          // start scan at head node
            //           if (lpFrom->mpNext) {
            //               while (lpFrom->mpNext && lpFrom->mpNext != lpNode)  // DF3C
            //                   lpFrom = lpFrom->mpNext;
            //               if (lpFrom->mpNext == lpNode) {    // DF58/DF64
            //                   --muCount; result = 1;
            //                   lpFrom->mpNext = lpNode->mpNext;   // DF7C/DF80
            //                   if (lpNode == mpTail) mpTail = lpFrom;  // DF84/DF90
            //               }
            //           }
            //       }
            //   } else {                                      // lpNode == mpHead
            //       result = 1; --muCount;
            //       if (lpNode == mpTail) { mpTail = 0; mpHead = 0; }   // DF04/DF08
            //       else mpHead = lpNode->mpNext;              // DF10/DF14
            //   }
            //   if (result) lpNode->mpNext = 0;               // DF9C
            int AsyncOpList::Remove(AsyncOp* lpNode, AsyncOp* lpFrom)
            {
                int liResult = 0;

                if (lpNode != mpHead)
                {
                    if (mpHead)
                    {
                        if (!lpFrom)
                            lpFrom = mpHead;
                        if (lpFrom->mpNext)
                        {
                            while (lpFrom->mpNext && lpFrom->mpNext != lpNode)
                                lpFrom = lpFrom->mpNext;

                            if (lpFrom->mpNext && lpFrom->mpNext == lpNode)
                            {
                                liResult = 1;
                                --muCount;
                                lpFrom->mpNext = lpNode->mpNext;
                                if (lpNode == mpTail)
                                    mpTail = lpFrom;
                            }
                        }
                    }
                }
                else
                {
                    liResult = 1;
                    AsyncOp* lpTail = mpTail;
                    --muCount;
                    if (lpNode == lpTail)
                    {
                        mpTail = nullptr;
                        mpHead = nullptr;
                    }
                    else
                    {
                        mpHead = lpNode->mpNext;
                    }
                }

                if (liResult)
                    lpNode->mpNext = nullptr;

                return liResult;
            }
        }
    }
}
