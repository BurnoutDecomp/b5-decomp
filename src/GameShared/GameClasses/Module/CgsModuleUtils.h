#ifndef CGS_MODULE_UTILS_H
#define CGS_MODULE_UTILS_H

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"

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
        if (!lpDestBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 259);
            CgsDev::Assert::EndAssert();
        }
        if (!lpSourceBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 260);
            CgsDev::Assert::EndAssert();
        }
        lpDestBuffer->LockForWrite();
        lpSourceBuffer->LockForRead();
    }

    template <typename TDest, typename TSource>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSource* lpSourceBuffer )
    {
        if (!lpDestBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 272);
            CgsDev::Assert::EndAssert();
        }
        if (!lpSourceBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 273);
            CgsDev::Assert::EndAssert();
        }
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

    // four-source variant (X360 sub_823B7400 / sub_823B7510)
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD>
    inline void LockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                  TSourceB* lpSourceB, TSourceC* lpSourceC,
                                  TSourceD* lpSourceD )
    {
        lpDestBuffer->LockForWrite();
        lpSourceA->LockForRead();
        lpSourceB->LockForRead();
        lpSourceC->LockForRead();
        lpSourceD->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                    TSourceB* lpSourceB, TSourceC* lpSourceC,
                                    TSourceD* lpSourceD )
    {
        lpSourceD->UnlockForRead();
        lpSourceC->UnlockForRead();
        lpSourceB->UnlockForRead();
        lpSourceA->UnlockForRead();
        lpDestBuffer->UnlockForWrite();
    }

    // ARTIST823B7620; DecFIGS CgsModuleUtils.h:401.
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer,
                                   const IOBuffer* lpOutputBuffer0,
                                   const IOBuffer* lpOutputBuffer1,
                                   const IOBuffer* lpOutputBuffer2,
                                   const IOBuffer* lpOutputBuffer3,
                                   const IOBuffer* lpOutputBuffer4)
    {
        if (!lpInputBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 403);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 404);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 405);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer2)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 406);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 407);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer4)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 408);
            CgsDev::Assert::EndAssert();
        }
        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
        lpOutputBuffer4->LockForRead();
    }

    // ARTIST823B7760; DecFIGS CgsModuleUtils.h:426.
    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer,
                                   const IOBuffer* lpOutputBuffer0,
                                   const IOBuffer* lpOutputBuffer1,
                                   const IOBuffer* lpOutputBuffer2,
                                   const IOBuffer* lpOutputBuffer3,
                                   const IOBuffer* lpOutputBuffer4)
    {
        if (!lpInputBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 428);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 429);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 430);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer2)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 431);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 432);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer4)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 433);
            CgsDev::Assert::EndAssert();
        }
        lpOutputBuffer4->UnlockForRead();
        lpOutputBuffer3->UnlockForRead();
        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // Single-buffer overloads (the X360 also emits the pair against one module
    // buffer, e.g. WorldModule::Prepare's prop-module Lock/UnlockBuffersForIO).
    template <typename TBuffer>
    inline void LockBuffersForIO( TBuffer* lpBuffer )
    {
        if (!lpBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 238);
            CgsDev::Assert::EndAssert();
        }
        lpBuffer->LockForWrite();
    }

    template <typename TBuffer>
    inline void UnlockBuffersForIO( TBuffer* lpBuffer )
    {
        if (!lpBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 248);
            CgsDev::Assert::EndAssert();
        }
        lpBuffer->UnlockForWrite();
    }
}

#endif // CGS_MODULE_UTILS_H
