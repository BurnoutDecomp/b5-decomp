// =====================================================================================
// CWMVFARoutines -- 16x16 block-mean kernel (vtable slot 2). Partfile of
// CWMVFARoutines.cpp; see CWMVFARoutines.h for the class shape.
//
// Reconstructed from the console executable; the PowerPC asm is authoritative.
// =====================================================================================
#include "types.hpp"

#include "CWMVFARoutines.h"

// -------------------------------------------------------------------------------------
// CWMVFARoutines::calcMean16x16B.
// Mean of a 16x16 block of byte samples: sum 256 values, >>8, store the low byte to
// *lpMeanOut. The block base is liSampleBase+liColumn; each of the 16 rows steps by
// liRowStride and reads 16 consecutive columns. The asm walks four rows per do-while pass
// (four passes) with sixteen column cursors; the reduction is order-independent. Returns
// the column-10 cursor advanced past the block (liSampleBase+liColumn+10 + 16*liRowStride),
// the residual return the callers discard.
// -------------------------------------------------------------------------------------
unsigned char* CWMVFARoutines::calcMean16x16B(u8* lpMeanOut, int liSampleBase, int liColumn, int liRowStride)
{
    u8* lpBlock = reinterpret_cast<u8*>(liSampleBase + liColumn);

    u32 luSum = 0;
    for (int liRow = 0; liRow < 16; ++liRow)
    {
        const u8* lpRow = lpBlock + liRow * liRowStride;
        for (int liCol = 0; liCol < 16; ++liCol)
        {
            luSum += lpRow[liCol];
        }
    }

    *lpMeanOut = static_cast<u8>(luSum >> 8);
    return lpBlock + 10 + 16 * liRowStride;
}
