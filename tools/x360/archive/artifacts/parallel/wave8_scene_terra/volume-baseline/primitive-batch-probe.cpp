#include "vendor/renderware/collision/GPInstance.hpp"
#include "vendor/renderware/collision/VolRef.hpp"
#include "SDKs/EATech/rwcollision/volume_debug_access.h"
namespace rw { namespace collision {
s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,
                            s32 aiResBufMaxSize,
                            GPInstance* lapInstancingBuffer,
                            VolRef1xN* lapPairs, s32 aiNumPairs,
                            f32 afPadding)
{
    s32 liNumIntersections = 0;                                     // r30

    if (aiNumPairs > 0)                                             // cmpwi/ble (signed)
    {
        VolRef1xN* lpGroup           = lapPairs;                    // r31
        s32        liGroupsRemaining = aiNumPairs;                  // r23
        do
        {
            if (liNumIntersections < aiResBufMaxSize)               // cmpw/bge (signed)
            {
                // ---- instance the "1"-side volume into slot 0 ----
                const VolRef* lpRef1    = lpGroup->vRef1;
                const Volume* lpVolume1 = lpRef1->volume;
                lpVolume1->CreateGPInstance(lapInstancingBuffer[0], lpRef1->tm);
                lapInstancingBuffer[0].mUserTag = lpRef1->tag;  // stw r11, 0x88(r29)

                // ---- instance the N-side volumes into slots 1..N ----
                u32 luRefIndex = 0;                                 // r26
                if (lpGroup->vRefsNCount != 0)                      // cmplwi/ble (unsigned)
                {
                    GPInstance* lpInstanceN = &lapInstancingBuffer[1]; // r27 (0xC0 stride)
                    VolRef**    lppRefN     = &lpGroup->vRefsN[0];     // r28
                    do
                    {
                        const VolRef* lpRefN    = *lppRefN;
                        const Volume* lpVolumeN = lpRefN->volume;
                        lpVolumeN->CreateGPInstance(*lpInstanceN, lpRefN->tm);
                        ++luRefIndex;                               // addi r26, r26, 1
                        lpInstanceN->mUserTag = lpRefN->tag;    // stw r11, 0(r27)
                        ++lppRefN;                                  // addi r28, r28, 4
                        ++lpInstanceN;                              // addi r27, r27, 0xC0
                    }
                    while (luRefIndex < lpGroup->vRefsNCount);      // count reloaded per pass
                }

                // ---- narrow-phase batch on the staged instances ----
                const s32 liRemaining = aiResBufMaxSize - liNumIntersections; // subf
                PrimitivePairIntersectResult* lpOut =
                    lapResults + liNumIntersections;                // mulli 0x750 + add

                s32 liFound;
                if (lpGroup->volumesSwapped != 0)                   // cmplwi/beq on word 2
                {
                    liFound = GPInstanceBatchIntersectNx1(
                        lpOut, liRemaining,
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        lapInstancingBuffer[0], afPadding);
                }
                else
                {
                    liFound = GPInstanceBatchIntersect1xN(
                        lpOut, liRemaining,
                        lapInstancingBuffer[0],
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        afPadding);
                }
                liNumIntersections += liFound;                      // add r30, r3, r30
            }

            // Group advance runs even when the result buffer is full
            // (console: lpuGroup += lpuGroup[1] + 3).
            lpGroup = lpGroup->NextGroup();
            --liGroupsRemaining;                                    // addic.
        }
        while (liGroupsRemaining != 0);                             // bne
    }

    return liNumIntersections;                                      // mr r3, r30
}
} }
