#include "SDKs/EATech/eajobs/entry_point.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   EA::Jobs::EntryPoint::SetCode        @ 0x82BC9840   (export hole; 5 insns lifted)
//   EA::Jobs::EntryPoint::SetName        @ 0x82BC9858   (22)
//   EA::Jobs::EntryPoint::SetAffinity    @ 0x82BC98B0   (2)
//   EA::Jobs::EntryPoint::SetCodeRecycle @ 0x82AD5078   (ICF-folded to a bare `blr`)
//
// ⭐⭐ ODR FORK #3 RETIRED 2026-08-10 (fill-worker wave 2).
// This TU used to declare its OWN `class EA::Jobs::EntryPoint` inline -- a second
// definition of the same qualified name, with a DIFFERENT layout:
//     char macName[16]; u8 mPad16[4]; int miAffinity;   (24 bytes)
// against the real 44-byte struct in entry_point.h (mName / mPriority / mAffinity /
// mEnvironment / mKernelSwap / mCodeRecycle / mBreakOnEntry / mAllowSleepOn / the code
// union). Both bodies were compiled against the local shape. It happened to be harmless
// -- macName[16] and miAffinity@+20 coincide with mName and mAffinity -- but it is
// exactly [[odr-forks-link-silently]]: a body written against one class definition
// linking cleanly into callers that saw another, because the mangled name encodes
// neither the class-key nor the members. Found while closing the fill worker's link,
// which is the first thing in this port that ever needed SetCode.
//
// Vendor EA code: members/enumerators follow the EA SDK convention, not Brn/Cgs.

namespace EA
{
namespace Jobs
{
    // ------------------------------------------------------------------------
    // SetCode @0x82BC9840 -- ⚠️ EXPORT HOLE (no 0x82BC9840.json). Five instructions
    // lifted from the image with ppcdis.py:
    //   0x82BC9840  cmpi cr6, r4, 0        ; leEnvironment == JOB_ENVIRONMENT_LOCAL ?
    //   0x82BC9844  stw  r4, 24(r3)        ; mEnvironment = leEnvironment   (+0x18)
    //   0x82BC9848  4C9A0020  bnelr cr6    ; BO=4 (branch-if-false), BI=26 (CR6.EQ)
    //   0x82BC984C  stw  r5, 40(r3)        ; mpfnLocalJob = lpvCode         (+0x28)
    //   0x82BC9850  blr
    //
    // ⚠️ DECODE NOTE: 0x4C9A0020 is `bnelr cr6`, NOT `beqlr cr6` (BO field 0b00100 ==
    // "branch if condition FALSE"). Reading it the other way inverts the function into
    // one that DROPS the entry-point pointer on the only path this build ever takes --
    // the difference between a job that runs and a job whose code is null.
    //
    // liSize (r6) is never read: the X360 has no SPUs, so the SPU-image arm (and the
    // whole mSpuJobSize/mSpuJobInfo tail this header already documents as PS3-only) is
    // compiled out of this build.
    // ------------------------------------------------------------------------
    void EntryPoint::SetCode(JobEnvironment leEnvironment, const void* lpvCode, int liSize)
    {
        (void)liSize;

        mEnvironment = leEnvironment;

        if (leEnvironment != JOB_ENVIRONMENT_LOCAL)
        {
            return;
        }

        mpfnLocalJob = reinterpret_cast<void (*)(Param, Param, Param, Param)>(
            const_cast<void*>(lpvCode));
    }

    // entry_point.h:56 -- the local-job convenience form. No standalone X360 body (the
    // build only ever emits the environment form above); it is the same two stores.
    void EntryPoint::SetCode(void (*lpfnLocalJob)(Param, Param, Param, Param))
    {
        mEnvironment = JOB_ENVIRONMENT_LOCAL;
        mpfnLocalJob = lpfnLocalJob;
    }

    // SetAffinity @0x82BC98B0 -- stores the mask at +20 (mAffinity).
    void EntryPoint::SetAffinity(JobAffinity leAffinity)
    {
        mAffinity = leAffinity;
    }

    // SetName @0x82BC9858 (22) -- copy up to 15 characters of lpcName into the inline
    // 16-byte name buffer and null-terminate; a null source yields an empty string.
    void EntryPoint::SetName(const char* lpcName)
    {
        if (lpcName)
        {
            int liLen = 0;
            for (; liLen < 16; ++liLen)
            {
                if (!lpcName[liLen])
                    break;
                mName[liLen] = lpcName[liLen];
            }
            if (liLen >= 16)
                liLen = 15;
            mName[liLen] = 0;
        }
        else
        {
            mName[0] = 0;
        }
    }

    // ------------------------------------------------------------------------
    // SetCodeRecycle -- ⚠️ AS-SHIPPED THIS IS EMPTY ON X360.
    // Both call sites (RunFillTriangleCacheStream @0x82810E38 and the same ICF-folded
    // call CollisionBatch::SetupJob @0x82810508 makes) branch to 0x82AD5078, and the
    // word at 0x82AD5078 read out of the image is a bare `4E800020 blr`. Code recycling
    // is an SPU-loader concept and there are no SPUs in this build, so the compiler
    // folded the setter to nothing. Reproduced as shipped (the argument is consumed and
    // dropped) rather than "corrected" into a store that the console does not make --
    // writing mCodeRecycle here would be inventing state no console path ever reads.
    // ------------------------------------------------------------------------
    void EntryPoint::SetCodeRecycle(CodeRecycle leRecycle)
    {
        (void)leRecycle;
    }
}
}
