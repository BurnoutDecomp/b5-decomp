#pragma once

#include "types.hpp"

// rw::core::filesys::detail::ListSingle<T> -- the intrusive singly-linked list the filesys
// Manager keeps its registered Devices in and each Device keeps its queued AsyncOps in.
// Nodes link forward through their own `mpNext` member (node +0x00). The console carries one
// out-of-line copy of InsertAfter / Remove shared by both instantiations (0x82BBDE70 /
// 0x82BBDED0); Append is inlined into Manager::RegisterDevice.

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            namespace detail
            {
                template <class T>
                struct ListSingle
                {
                    T*  mpHead;    // +0x00
                    T*  mpTail;    // +0x04
                    u32 muCount;   // +0x08

                    // Link lpNode at the tail. The empty-list arm comes first (rwcore
                    // list_inlines.h; ARTIST Manager::RegisterDevice 0x82BBE388 falls
                    // through to `mpHead = lpNode`).
                    void Append(T* lpNode)
                    {
                        T* lpTail = mpTail;
                        lpNode->mpNext = 0;
                        mpTail = lpNode;
                        ++muCount;
                        if (lpTail == 0)
                            mpHead = lpNode;
                        else
                            lpTail->mpNext = lpNode;
                    }

                    // Link lpNode at the head, keeping mpTail current.
                    void Push(T* lpNode)
                    {
                        lpNode->mpNext = mpHead;
                        mpHead = lpNode;
                        ++muCount;
                        if (lpNode->mpNext == 0)
                            mpTail = lpNode;
                    }

                    // Insert lpNode after lpAfter (lpAfter == null -> push front), keeping
                    // mpTail current and bumping the count. ARTIST 0x82BBDE70 lays the
                    // push-front arm first, as rwcore list_inlines.h orders it.
                    void InsertAfter(T* lpAfter, T* lpNode)
                    {
                        if (lpAfter == 0)
                        {
                            Push(lpNode);
                        }
                        else
                        {
                            if (lpAfter->mpNext == 0)
                                mpTail = lpNode;
                            lpNode->mpNext = lpAfter->mpNext;
                            lpAfter->mpNext = lpNode;
                            ++muCount;
                        }
                    }

                    // Unlink lpNode. lpPrev is an optional predecessor node to start the
                    // forward scan from (null -> scan from mpHead). True if lpNode was
                    // removed (its link is then cleared). ARTIST 0x82BBDED0 tests the
                    // head case first, as rwcore list_inlines.h orders it.
                    bool Remove(T* lpNode, T* lpPrev)
                    {
                        bool lbRemoved = false;

                        if (lpNode == mpHead)
                        {
                            lbRemoved = true;
                            --muCount;
                            if (lpNode == mpTail)
                            {
                                mpTail = 0;
                                mpHead = 0;
                            }
                            else
                            {
                                mpHead = lpNode->mpNext;
                            }
                        }
                        else if (mpHead != 0)
                        {
                            if (lpPrev == 0)
                                lpPrev = mpHead;

                            while (lpPrev->mpNext != 0 && lpPrev->mpNext != lpNode)
                                lpPrev = lpPrev->mpNext;

                            if (lpPrev->mpNext != 0 && lpPrev->mpNext == lpNode)
                            {
                                lbRemoved = true;
                                --muCount;
                                lpPrev->mpNext = lpNode->mpNext;
                                if (lpNode == mpTail)
                                    mpTail = lpPrev;
                            }
                        }

                        if (lbRemoved)
                            lpNode->mpNext = 0;

                        return lbRemoved;
                    }
                };
            }
        }
    }
}
