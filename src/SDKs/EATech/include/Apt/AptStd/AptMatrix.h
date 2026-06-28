#pragma once

// ===========================================================================
// EATech Apt -- AptMatrix (leak AptStd/AptMatrix.h, verbatim). The 2D affine
// transform applied to Apt display objects: a/b/c/d is the 2x2 and tx/ty the
// translation. Pool-allocated like the other non-GC Apt types.
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"       // DOGMA_PoolManager::Allocate/Deallocate

struct AptMatrix
{
    void AptMatrixCopy(const AptMatrix* pMatrix)
    {
        if (pMatrix != 0)
        {
            a  = pMatrix->a;
            b  = pMatrix->b;
            c  = pMatrix->c;
            d  = pMatrix->d;
            tx = pMatrix->tx;
            ty = pMatrix->ty;
        }
    }

    float a, b, c, d;
    float tx, ty;

    static void* operator new(size_t size)             { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size) { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)           { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)            { AptNonGCFreeSavedSize(p); }
};
