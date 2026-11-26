#ifndef CGS_MODULE_UTILS_H
#define CGS_MODULE_UTILS_H
#define CGS_ARTIST_CGSMODULEUTILS_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"

// ARTIST 823B6F30 through 823B7EBC uses shared overloads for one destination and
// zero to seven read-only sources. Check the buffers before taking or releasing locks.
namespace CgsModule
{
    // ARTIST 823B6FE0 / 823B7060
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 259);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 260);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 272);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 273);

        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B70E0 / 823B7190
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 286);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 287);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 288);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0,
        const IOBuffer* lpOutputBuffer1)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 302);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 303);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 304);

        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7240 / 823B7320
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 319);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 320);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 321);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 322);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 338);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 339);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 340);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 341);

        lpOutputBuffer2->UnlockForRead();
        lpOutputBuffer1->UnlockForRead();
        lpOutputBuffer0->UnlockForRead();
        lpInputBuffer->UnlockForWrite();
    }

    // ARTIST 823B7400 / 823B7510
    inline void LockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 358);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 359);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 360);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 361);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 362);

        lpInputBuffer->LockForWrite();
        lpOutputBuffer0->LockForRead();
        lpOutputBuffer1->LockForRead();
        lpOutputBuffer2->LockForRead();
        lpOutputBuffer3->LockForRead();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer, const IOBuffer* lpOutputBuffer0, const IOBuffer* lpOutputBuffer1,
        const IOBuffer* lpOutputBuffer2, const IOBuffer* lpOutputBuffer3)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 380);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 381);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 382);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 383);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 384);

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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 403);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 404);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 405);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 406);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 407);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 408);

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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 428);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 429);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 430);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 431);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 432);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 433);

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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 454);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 455);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 456);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 457);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 458);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 459);
        CGS_ASSERT_AT(lpOutputBuffer5, "lpOutputBuffer5", CGS_ARTIST_CGSMODULEUTILS_FILE, 460);

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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 482);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 483);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 484);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 485);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 486);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 487);
        CGS_ASSERT_AT(lpOutputBuffer5, "lpOutputBuffer5", CGS_ARTIST_CGSMODULEUTILS_FILE, 488);

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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 511);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 512);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 513);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 514);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 515);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 516);
        CGS_ASSERT_AT(lpOutputBuffer5, "lpOutputBuffer5", CGS_ARTIST_CGSMODULEUTILS_FILE, 517);
        CGS_ASSERT_AT(lpOutputBuffer6, "lpOutputBuffer6", CGS_ARTIST_CGSMODULEUTILS_FILE, 518);
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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 542);
        CGS_ASSERT_AT(lpOutputBuffer0, "lpOutputBuffer0", CGS_ARTIST_CGSMODULEUTILS_FILE, 543);
        CGS_ASSERT_AT(lpOutputBuffer1, "lpOutputBuffer1", CGS_ARTIST_CGSMODULEUTILS_FILE, 544);
        CGS_ASSERT_AT(lpOutputBuffer2, "lpOutputBuffer2", CGS_ARTIST_CGSMODULEUTILS_FILE, 545);
        CGS_ASSERT_AT(lpOutputBuffer3, "lpOutputBuffer3", CGS_ARTIST_CGSMODULEUTILS_FILE, 546);
        CGS_ASSERT_AT(lpOutputBuffer4, "lpOutputBuffer4", CGS_ARTIST_CGSMODULEUTILS_FILE, 547);
        CGS_ASSERT_AT(lpOutputBuffer5, "lpOutputBuffer5", CGS_ARTIST_CGSMODULEUTILS_FILE, 548);
        CGS_ASSERT_AT(lpOutputBuffer6, "lpOutputBuffer6", CGS_ARTIST_CGSMODULEUTILS_FILE, 549);
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
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 238);
        lpInputBuffer->LockForWrite();
    }

    inline void UnlockBuffersForIO(IOBuffer* lpInputBuffer)
    {
        CGS_ASSERT_AT(lpInputBuffer, "lpInputBuffer", CGS_ARTIST_CGSMODULEUTILS_FILE, 248);
        lpInputBuffer->UnlockForWrite();
    }
}

#endif // CGS_MODULE_UTILS_H

#undef CGS_ARTIST_CGSMODULEUTILS_FILE
