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
    static __forceinline void AssertIOBuffer(const void* lpBuffer,
                                                     const char* lpcExpression, int liLine)
    {
        if (!lpBuffer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcExpression,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", liLine);
            CgsDev::Assert::EndAssert();
        }
    }

    // ARTIST 823B6FE0 / 823B7060
    template <typename TDest, typename TSourceA>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 259);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 260);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
    }

    template <typename TDest, typename TSourceA>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 272);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 273);

        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B70E0 / 823B7190
    template <typename TDest, typename TSourceA, typename TSourceB>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 286);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 287);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 288);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 302);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 303);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 304);

        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7240 / 823B7320
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0, TSourceB* lpOutputBuffer1,
        TSourceC* lpOutputBuffer2)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 319);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 320);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 321);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 322);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0, TSourceB* lpOutputBuffer1,
        TSourceC* lpOutputBuffer2)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 338);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 339);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 340);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 341);

        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7400 / 823B7510
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC, typename TSourceD>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0, TSourceB* lpOutputBuffer1,
        TSourceC* lpOutputBuffer2, TSourceD* lpOutputBuffer3)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 358);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 359);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 360);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 361);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 362);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC, typename TSourceD>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0, TSourceB* lpOutputBuffer1,
        TSourceC* lpOutputBuffer2, TSourceD* lpOutputBuffer3)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 380);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 381);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 382);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 383);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 384);

        lpOutputBuffer3->UnlockForRead();
        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7620 / 823B7760 checks every buffer before locking or unlocking.
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1, TSourceC* lpOutputBuffer2,
        TSourceD* lpOutputBuffer3, TSourceE* lpOutputBuffer4)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 403);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 404);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 405);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 406);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 407);
        AssertIOBuffer(lpOutputBuffer4, "lpOutputBuffer4", 408);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
        lpOutputBuffer4->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1, TSourceC* lpOutputBuffer2,
        TSourceD* lpOutputBuffer3, TSourceE* lpOutputBuffer4)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 428);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 429);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 430);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 431);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 432);
        AssertIOBuffer(lpOutputBuffer4, "lpOutputBuffer4", 433);

        lpOutputBuffer4->UnlockForRead();
        lpOutputBuffer3->UnlockForRead();
        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B78A0 / 823B7A10 guards the six effects inputs and their destination.
    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE, typename TSourceF>
    inline void LockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1, TSourceC* lpOutputBuffer2,
        TSourceD* lpOutputBuffer3, TSourceE* lpOutputBuffer4, TSourceF* lpOutputBuffer5)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 454);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 455);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 456);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 457);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 458);
        AssertIOBuffer(lpOutputBuffer4, "lpOutputBuffer4", 459);
        AssertIOBuffer(lpOutputBuffer5, "lpOutputBuffer5", 460);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
        lpOutputBuffer4->LockForRead();
        lpOutputBuffer5->LockForRead();
    }

    template <typename TDest, typename TSourceA, typename TSourceB, typename TSourceC,
              typename TSourceD, typename TSourceE, typename TSourceF>
    inline void UnlockBuffersForIO(TDest* lpInputBuffer, TSourceA* lpOutputBuffer0,
        TSourceB* lpOutputBuffer1, TSourceC* lpOutputBuffer2,
        TSourceD* lpOutputBuffer3, TSourceE* lpOutputBuffer4, TSourceF* lpOutputBuffer5)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 482);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 483);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 484);
        AssertIOBuffer(lpOutputBuffer2, "lpOutputBuffer2", 485);
        AssertIOBuffer(lpOutputBuffer3, "lpOutputBuffer3", 486);
        AssertIOBuffer(lpOutputBuffer4, "lpOutputBuffer4", 487);
        AssertIOBuffer(lpOutputBuffer5, "lpOutputBuffer5", 488);

        lpOutputBuffer5->UnlockForRead();
        lpOutputBuffer4->UnlockForRead();
        lpOutputBuffer3->UnlockForRead();
        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
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
