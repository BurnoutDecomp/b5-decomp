#ifndef CGS_MODULE_UTILS_H
#define CGS_MODULE_UTILS_H

#include "GameShared/GameClasses/Core/CgsAssert.h"

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

    // five-source variant (X360 sub_823B7620 / sub_823B7760) -- the widest set the
    // world drive uses (WorldModule::Update @0x827D63E8's entity-modules -> scene
    // staging locks the scene input plus five module outputs).
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE>
    inline void LockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                  TSourceB* lpSourceB, TSourceC* lpSourceC,
                                  TSourceD* lpSourceD, TSourceE* lpSourceE )
    {
        lpDestBuffer->LockForWrite();
        lpSourceA->LockForRead();
        lpSourceB->LockForRead();
        lpSourceC->LockForRead();
        lpSourceD->LockForRead();
        lpSourceE->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE>
    inline void UnlockBuffersForIO( TDest* lpDestBuffer, TSourceA* lpSourceA,
                                    TSourceB* lpSourceB, TSourceC* lpSourceC,
                                    TSourceD* lpSourceD, TSourceE* lpSourceE )
    {
        lpSourceE->UnlockForRead();
        lpSourceD->UnlockForRead();
        lpSourceC->UnlockForRead();
        lpSourceB->UnlockForRead();
        lpSourceA->UnlockForRead();
        lpDestBuffer->UnlockForWrite();
    }

    // ARTIST 823B7B80 / 823B7D20: resource input with seven producer outputs.
    template <typename TDest, typename TA, typename TB, typename TC, typename TD,
              typename TE, typename TF, typename TG>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TA* lpOutputBuffer0,
        TB* lpOutputBuffer1, TC* lpOutputBuffer2, TD* lpOutputBuffer3,
        TE* lpOutputBuffer4, TF* lpOutputBuffer5, TG* lpOutputBuffer6)
    {
        if (!lpInputBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 511);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 512);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 513);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer2)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 514);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 515);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer4)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 516);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer5)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer5",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 517);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer6)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer6",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 518);
            CgsDev::Assert::EndAssert();
        }
        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
        lpOutputBuffer4->LockForRead();
        lpOutputBuffer5->LockForRead();
        lpOutputBuffer6->LockForRead();
    }

    // ARTIST 823B7B80 / 823B7D20: resource input with seven producer outputs.
    template <typename TDest, typename TA, typename TB, typename TC, typename TD,
              typename TE, typename TF, typename TG>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TA* lpOutputBuffer0,
        TB* lpOutputBuffer1, TC* lpOutputBuffer2, TD* lpOutputBuffer3,
        TE* lpOutputBuffer4, TF* lpOutputBuffer5, TG* lpOutputBuffer6)
    {
        if (!lpInputBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 542);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer0",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 543);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 544);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer2)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer2",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 545);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer3",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 546);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer4)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer4",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 547);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer5)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer5",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 548);
            CgsDev::Assert::EndAssert();
        }
        if (!lpOutputBuffer6)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutputBuffer6",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 549);
            CgsDev::Assert::EndAssert();
        }
        lpOutputBuffer6->UnlockForRead();
        lpOutputBuffer5->UnlockForRead();
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
        lpBuffer->LockForWrite();
    }

    template <typename TBuffer>
    inline void UnlockBuffersForIO( TBuffer* lpBuffer )
    {
        lpBuffer->UnlockForWrite();
    }
}

#endif // CGS_MODULE_UTILS_H
