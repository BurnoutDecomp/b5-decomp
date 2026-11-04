#ifndef CGS_MODULE_UTILS_H
#define CGS_MODULE_UTILS_H

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"

// ARTIST 823B6F30 through 823B7EBC uses shared overloads for one destination and
// zero to seven read-only sources. Check the buffers before taking or releasing locks.
namespace CgsModule
{
    __forceinline void AssertIOBuffer(const void* lpBuffer,
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
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 259);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 260);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 272);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 273);

        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B70E0 / 823B7190
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 286);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 287);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 288);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 302);
        AssertIOBuffer(lpOutputBuffer0, "lpOutputBuffer0", 303);
        AssertIOBuffer(lpOutputBuffer1, "lpOutputBuffer1", 304);

        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7240 / 823B7320
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2)
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

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2)
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
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3)
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

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3)
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
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2,
        const IOBuffer* lpOutputBuffer3, const IOBuffer* lpOutputBuffer4)
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

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2,
        const IOBuffer* lpOutputBuffer3, const IOBuffer* lpOutputBuffer4)
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
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2,
        const IOBuffer* lpOutputBuffer3, const IOBuffer* lpOutputBuffer4, const IOBuffer* lpOutputBuffer5)
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

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2,
        const IOBuffer* lpOutputBuffer3, const IOBuffer* lpOutputBuffer4, const IOBuffer* lpOutputBuffer5)
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
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3,
        const IOBuffer* lpOutputBuffer4, const IOBuffer* lpOutputBuffer5, const IOBuffer* lpOutputBuffer6)
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
    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1, const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3,
        const IOBuffer* lpOutputBuffer4, const IOBuffer* lpOutputBuffer5, const IOBuffer* lpOutputBuffer6)
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

    // ARTIST 823B6F30 / 823B6F88
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 238);
        lpInputBuffer->LockForWrite();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer)
    {
        AssertIOBuffer(lpInputBuffer, "lpInputBuffer", 248);
        lpInputBuffer->UnlockForWrite();
    }
}

#endif // CGS_MODULE_UTILS_H
