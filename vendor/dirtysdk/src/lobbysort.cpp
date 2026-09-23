// DirtySDK lobby -- natural merge sort (core/source/util/lobbysort.c).
//
// Sorts an array of element POINTERS by merging the ascending runs already present
// in the input (stable: equal elements keep their order), then permutes the element
// data into place in one cycle-following pass. Small inputs run on stack buffers,
// larger ones allocate their run/pointer/element scratch through DirtyMemAlloc.

#include "lobbysort.h"
#include "dirtymem.h"   // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstring>

// memory module id 'lsor'
#define LOBBYSORT_MEMID (('l' << 24) | ('s' << 16) | ('o' << 8) | 'r')

extern "C" void LobbyMSort(void* refptr, void* base, s32 nmemb, s32 size, LobbySortCompareT* compare)
{
    s32 pass;
    s32* rbuf;
    s32* rget;
    s32* rput;
    s32 rtmp[512 + 1];      // 511 single-element runs plus the two terminators need 513
    char** mbuf[2];
    char** msrc;
    char** mdst;
    char* mtmp[2][512];
    char* xbuf;
    char xtmp[1024];

    if (nmemb < 2)
    {
        return;
    }

    // scratch: run lengths, two pointer arrays, one element
    if (nmemb < 512)
    {
        mbuf[0] = mtmp[0];
        rbuf = rtmp;
        mbuf[1] = mtmp[1];
    }
    else
    {
        rbuf = static_cast<s32*>(DirtyMemAlloc(nmemb * (s32)sizeof(*rbuf), LOBBYSORT_MEMID, DirtyMemGroupQuery()));
        mbuf[0] = static_cast<char**>(DirtyMemAlloc(nmemb * (s32)sizeof(*mbuf[0]), LOBBYSORT_MEMID, DirtyMemGroupQuery()));
        mbuf[1] = static_cast<char**>(DirtyMemAlloc(nmemb * (s32)sizeof(*mbuf[1]), LOBBYSORT_MEMID, DirtyMemGroupQuery()));
    }
    if (size <= (s32)sizeof(xtmp))
    {
        xbuf = xtmp;
    }
    else
    {
        xbuf = static_cast<char*>(DirtyMemAlloc(size, LOBBYSORT_MEMID, DirtyMemGroupQuery()));
    }

    // split the input into ascending runs
    msrc = mbuf[0];
    msrc[0] = static_cast<char*>(base);
    rput = rbuf;
    rput[0] = 1;
    {
        char* p;
        char* q = static_cast<char*>(base) + (nmemb * size);
        for (p = static_cast<char*>(base) + size; p != q; p += size)
        {
            if (compare(refptr, *msrc, p) > 0)
            {
                *++rput = 1;
            }
            else
            {
                *rput += 1;
            }
            *++msrc = p;
        }
    }
    rput[1] = 0;
    rput[2] = 0;

    // merge run pairs until a single run remains
    for (pass = 0; rbuf[1] != 0; ++pass)
    {
        s32 r1;
        s32 r2;
        char** xbeg;
        char** xend;
        char** ybeg;
        char** yend;

        msrc = mbuf[pass & 1];
        mdst = mbuf[~pass & 1];
        rget = rbuf;
        rput = rbuf;
        r1 = *rget++;
        r2 = *rget++;
        xbeg = msrc;
        xend = xbeg + r1;
        while (r1 != 0)
        {
            ybeg = xend;
            yend = ybeg + r2;
            for (;;)
            {
                if (xbeg == xend)
                {
                    if (ybeg == yend)
                    {
                        break;
                    }
                    *mdst++ = *ybeg++;
                }
                else if ((ybeg == yend) || (compare(refptr, *xbeg, *ybeg) <= 0))
                {
                    *mdst++ = *xbeg++;
                }
                else
                {
                    *mdst++ = *ybeg++;
                }
            }
            *rput++ = r1 + r2;
            xbeg = yend;
            r1 = *rget++;
            r2 = *rget++;
            xend = xbeg + r1;
        }
        rput[0] = 0;
        rput[1] = 0;
    }

    // move the elements into sorted order, following each permutation cycle once
    if (pass != 0)
    {
        s32 i;
        s32 j;
        s32 k;

        msrc = mbuf[pass & 1];
        for (i = 0; i < nmemb; ++i)
        {
            if (msrc[i] == NULL)
            {
                continue;
            }
            memcpy(xbuf, msrc[i], size);
            for (j = i, k = (s32)((msrc[j] - static_cast<char*>(base)) / size); k != i;
                 j = k, k = (s32)((msrc[j] - static_cast<char*>(base)) / size))
            {
                memcpy(msrc[j], msrc[k], size);
                msrc[j] = NULL;
            }
            memcpy(msrc[j], xbuf, size);
            msrc[j] = NULL;
        }
    }

    if (rbuf != rtmp)
    {
        DirtyMemFree(rbuf, LOBBYSORT_MEMID, DirtyMemGroupQuery());
        DirtyMemFree(mbuf[0], LOBBYSORT_MEMID, DirtyMemGroupQuery());
        DirtyMemFree(mbuf[1], LOBBYSORT_MEMID, DirtyMemGroupQuery());
    }
    if (xbuf != xtmp)
    {
        DirtyMemFree(xbuf, LOBBYSORT_MEMID, DirtyMemGroupQuery());
    }
}
