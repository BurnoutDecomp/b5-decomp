#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectModelDynamic::GetNextAssetID @ 0x82BDE960
//
// Override of the base asset-id cursor (base vftable +0x20). Rotates through the
// base CMassiveAdObject's delivered-asset list: with nothing delivered it
// returns 0; with a current asset and at least two delivered ids it scans
// forward (bounded by the delivered count) for the current id and steps one past
// it -- wrapping back to the head and resetting muActiveAssetCount when that step
// runs off the tail; otherwise it just takes the head. The id now under the
// cursor becomes the new current asset and is returned.
// ---------------------------------------------------------------------------
int CMassiveAdObjectModelDynamic::GetNextAssetID()
{
    if (!muAssetIDCount)
        return 0;  // lhz +0x4C == 0: nothing delivered yet

    mAssetIDList.GoToStart();

    if (mnCurrentAssetID && muAssetIDCount >= 2)
    {
        // Rotate: find the current id, then step one past it.
        unsigned short uStep = 1;  // li r29, 1
        for (;;)
        {
            if (!mAssetIDList.GetCurrent())  // lwz +0x58 == 0 (loop test, .9E8)
            {
                mAssetIDList.GoToStart();  // ran off the list without a match
                break;
            }

            unsigned short uThis = uStep;
            uStep = static_cast<unsigned short>(uStep + 1);  // clrlwi r29, r9, 16
            if (uThis >= muAssetIDCount)  // cmplw + bge (UNSIGNED, not float)
            {
                mAssetIDList.GoToStart();  // scanned every delivered slot
                break;
            }

            if (*static_cast<int*>(mAssetIDList.GetCurrData()) == mnCurrentAssetID)
            {
                mAssetIDList.GoToNext();  // step past the current id
                ++muActiveAssetCount;     // lhz/addi 1/sth +0x68 (unconditional)
                if (!mAssetIDList.GetCurrent())
                {
                    mAssetIDList.GoToStart();  // wrapped off the tail
                    muActiveAssetCount = 0;
                }
                break;
            }

            mAssetIDList.GoToNext();
        }
    }
    else if (!mAssetIDList.GetCount())  // lwz +0x5C == 0
    {
        return 0;  // no current asset (or a single one) and an empty list
    }

    int nAssetId = *static_cast<int*>(mAssetIDList.GetCurrData());
    mnCurrentAssetID = nAssetId;  // stw r3, 0x64(r31)
    return nAssetId;
}

} // namespace MassiveAdClient3
