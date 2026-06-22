#include "SDKs/EATech/eajobs/event.h"

#include <intrin.h> // _InterlockedDecrement (MSVC atomic intrinsic)

// EA::Jobs::Event::Run @ 0x82BC98B8.
//
// Reconstructed store-for-store from the X360 .XEX. The asm:
//   lwz   r11,0xC(r3)        ; mDecrementerLocation
//   cmplwi r11,0 ; beq ...   ; skip the counter dance if null
//   <lwarx/addi -1/stwcx.>   ; atomic --(*mDecrementerLocation)
//   if result != 0 -> return ; not the last waiter: do nothing yet
//   lwz   r11,0(r3)          ; mType
//   cmpwi r11,2 -> CALLBACK  : lwz mCallback@4; if !=0 tail-call mCallback(mContext@8)
//   cmpwi r11,1 || r11,3     : *(mWriteLocation@8) = mWriteValue@4   (WRITE / WRITE_PTR)
//
// On X360 the decrement is a lwarx/stwcx. reservation pair guarded by a brief
// interrupt mask (mfmsr/mtmsree). The portable reconstruction expresses the same
// observable effect: atomically decrement the shared counter and only fire the
// action when this Run drives it to zero (or when there is no counter at all).

namespace EA
{
namespace Jobs
{
    void Event::Run() const
    {
        if (mDecrementerLocation)
        {
            // Atomic decrement; fire only when we are the one that reaches zero.
            long liRemaining = _InterlockedDecrement(
                reinterpret_cast<volatile long*>(mDecrementerLocation));
            if (liRemaining != 0)
                return;
        }

        if (mType == EVENT_TYPE_CALLBACK)
        {
            if (mCallback)
                mCallback(mContext);
        }
        else if (mType == EVENT_TYPE_WRITE || mType == EVENT_TYPE_WRITE_PTR)
        {
            *mWriteLocation = mWriteValue;
        }
    }
}
}
