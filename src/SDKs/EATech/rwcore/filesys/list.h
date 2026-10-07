#pragma once

#include "types.hpp"

// rw::core::filesys::detail::ListSingle<T> -- the intrusive singly-linked list the filesys
// Manager keeps its registered Devices in and each Device keeps its queued AsyncOps in.
// Nodes link forward through their own `mpNext` member (node +0x00). The console carries one
// out-of-line copy of InsertAfter / Remove shared by both instantiations; Append is inlined
// into Manager::RegisterDevice.

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

                    // Link lpNode at the tail.
                    void Append(T* lpNode)
                    {
                        T* lpTail = mpTail;
                        lpNode->mpNext = nullptr;
                        mpTail = lpNode;
                        ++muCount;
                        if (lpTail)
                            lpTail->mpNext = lpNode;
                        else
                            mpHead = lpNode;
                    }

                    // Insert lpNode after lpAfter (lpAfter == null -> push front), keeping
                    // mpTail current and bumping the count.
                    void InsertAfter(T* lpAfter, T* lpNode)
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
                    }

                    // Unlink lpNode. lpFrom is an optional predecessor node to start the
                    // forward scan from (null -> scan from mpHead). True if lpNode was
                    // removed (its link is then cleared).
                    bool Remove(T* lpNode, T* lpFrom)
                    {
                        bool lbRemoved = false;

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
                                        lbRemoved = true;
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
                            lbRemoved = true;
                            T* lpTail = mpTail;
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

                        if (lbRemoved)
                            lpNode->mpNext = nullptr;

                        return lbRemoved;
                    }
                };
            }
        }
    }
}
