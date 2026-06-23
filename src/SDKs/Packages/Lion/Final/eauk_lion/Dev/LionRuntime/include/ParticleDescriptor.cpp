#include "ParticleDescriptor.h"

// Out-of-line body for cParticleDescriptor::GetRequiredBucketType.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x82908660:
//   - If mFlags bit 0x10 is clear, no bucket is needed -> return 0 (the function
//     returns r3=0 immediately).
//   - Otherwise switch on mRenderMode (0..9): cases {0,3,4,9} -> bucket type 1, every
//     other case (the jump-table default) -> bucket type 2.
//   - Finally, if mFlags bit 0x20 is also set, force bucket type 2 regardless.
//
// The render-mode block evaluates first, then the 0x20 override; that ordering matches
// the asm (the switch picks 1 or 2, then a second `rlwinm` test bumps the result to 2).

cParticleDescriptor::BucketType cParticleDescriptor::GetRequiredBucketType() const
{
    BucketType result = E_BUCKET_NONE;

    if ((mFlags & E_FLAG_NEEDS_BUCKET) != 0)
    {
        switch (mRenderMode)
        {
            case 0:
            case 3:
            case 4:
            case 9:
                result = E_BUCKET_LIGHT;
                break;
            default:
                result = E_BUCKET_HEAVY;
                break;
        }

        if ((mFlags & E_FLAG_FORCE_HEAVY) != 0)
            return E_BUCKET_HEAVY;
    }

    return result;
}
