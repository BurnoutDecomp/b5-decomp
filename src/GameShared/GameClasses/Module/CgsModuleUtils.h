#ifndef CGS_MODULE_UTILS_H
#define CGS_MODULE_UTILS_H

// ============================================================================
// GameShared/GameClasses/Module/CgsModuleUtils.h  (original-source home: the
// Feb-2007 tree includes "Module/CgsModuleUtils.h"; the sound-module TU's
// gated-bridge comment cites LockBuffersForIO at CgsModuleUtils.h:259.)
//
// The buffer-pair IO lock helpers the X360 emits as the sub_823B6FE0 /
// sub_823B7060 pair around every cross-module bridge call: the DESTINATION
// buffer is locked for write and the SOURCE buffer for read, then both are
// released in reverse. (WorldModule::Prepare @0x827D53B0 brackets each of its
// failure-path bridges with them.)
// ============================================================================

namespace CgsModule
{
    template <typename TDest, typename TSource>
    inline void LockBuffersForIO( TDest* lpDestBuffer, TSource* lpSourceBuffer )
    {
        lpDestBuffer->LockForWrite();
        lpSourceBuffer->LockForRead();
    }

    template <typename TDest, typename TSource>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSource* lpSourceBuffer )
    {
        lpSourceBuffer->UnlockForRead();
        lpDestBuffer->UnlockForWrite();
    }

    // Multi-source overloads (X360 sub_823B70E0/sub_823B7190 = 2 sources,
    // sub_823B7240/sub_823B7320 = 3 sources): one destination locked for write,
    // each source for read; released in reverse.
    template <typename TDest, typename TSourceA, typename TSourceB>
    inline void LockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA, TSourceB* lpSourceB )
    {
        lpDestBuffer->LockForWrite();
        lpSourceA->LockForRead();
        lpSourceB->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA, TSourceB* lpSourceB )
    {
        lpSourceB->UnlockForRead();
        lpSourceA->UnlockForRead();
        lpDestBuffer->UnlockForWrite();
    }

    // three-source variant
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC>
    inline void LockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                  TSourceB* lpSourceB, TSourceC* lpSourceC )
    {
        lpDestBuffer->LockForWrite();
        lpSourceA->LockForRead();
        lpSourceB->LockForRead();
        lpSourceC->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                    TSourceB* lpSourceB, TSourceC* lpSourceC )
    {
        lpSourceC->UnlockForRead();
        lpSourceB->UnlockForRead();
        lpSourceA->UnlockForRead();
        lpDestBuffer->UnlockForWrite();
    }

    // Single-buffer overloads (the X360 also emits the pair against one module
    // buffer, e.g. WorldModule::Prepare's prop-module Lock/UnlockBuffersForIO).
    template <typename TBuffer>
    inline void LockBuffersForIO( TBuffer* lpBuffer )
    {
        lpBuffer->LockForWrite();
    }

    template <typename TBuffer>
    inline void UnlockBuffersForIO( TBuffer* lpBuffer )
    {
        lpBuffer->UnlockForWrite();
    }
}

#endif // CGS_MODULE_UTILS_H
