#ifndef EA_JOBS_ENTRY_POINT_H
#define EA_JOBS_ENTRY_POINT_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h" // JobEnvironment, JobAffinity, JobPriority, Param

// SDKs/EATech/eajobs/entry_point.h
//
// EA::Jobs::EntryPoint -- the per-job descriptor that names a job and selects the
// code it runs (the local function pointer / SPU job) plus its scheduling
// attributes (priority / affinity / environment). Reconstructed from the EATech
// DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/entry_point.h)
// and the X360 .XEX:
//   EA::Jobs::EntryPoint::SetName     @ 0x82BC9858  (copies <=15 chars into mName)
//   EA::Jobs::EntryPoint::SetAffinity @ 0x82BC98B0  (stores the mask at +20 = mAffinity)
//   EA::Jobs::EntryPoint::SetCode     @ 0x82BC9840  (env form: env + code ptr + size)
//
// X360 LAYOUT DELTA: the Xbox 360 has no SPUs, so the DWARF's SPU-only tail
// (mSpuJobSize + the 9-word SpuJobInfo) is absent from the ARTIST object. The X360
// EntryPoint is exactly 44 bytes -- proven by the enclosing Job: its mParams[4] sit
// at Job+0x30 with mEntryPoint at Job+0x04, leaving 0x2C (44) bytes for EntryPoint,
// and Job::Clear @ 0x82BCA110 memcpy's a 44-byte default-settings block into Job+4.
// SetAffinity storing at +20 pins mAffinity, matching the layout below.
//
// Vendor EA code reconstructed in its canonical home; members/enumerators follow
// the EA SDK convention (mFoo / *_DEFAULT), NOT the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    struct EntryPoint
    {
        // entry_point.h:26 -- per-job kernel-swap control.
        enum KernelSwap
        {
            SWAP_OFF     = 0,
            SWAP_ON      = 1,
            SWAP_DEFAULT = 0
        };

        // entry_point.h:33 -- whether the loaded job code may be recycled.
        enum CodeRecycle
        {
            CODE_RECYCLE_OFF     = 0,
            CODE_RECYCLE_ON      = 1,
            CODE_RECYCLE_DEFAULT = 1
        };

        // entry_point.h:55 -- point this entry at a job's code. The environment form
        // selects JOB_ENVIRONMENT_LOCAL (a function pointer) or JOB_ENVIRONMENT_SPU_THREADS
        // (an SPU job image + size); lpvCode is the code address, liSize its size (0 for
        // a local function). X360 0x82BC9840.
        void SetCode(JobEnvironment leEnvironment, const void* lpvCode, int liSize);
        // entry_point.h:56 -- local-job convenience form (stores the function pointer
        // and selects JOB_ENVIRONMENT_LOCAL).
        void SetCode(void (*lpfnLocalJob)(Param, Param, Param, Param));

        // entry_point.h:57 -- copy up to 15 chars of lpcName into the inline name buffer
        // and null-terminate (a null source yields an empty string). X360 0x82BC9858.
        void SetName(const char* lpcName);

        // entry_point.h:58 -- store the affinity mask (X360 0x82BC98B0 writes +20).
        void SetAffinity(JobAffinity leAffinity);

        // ⭐ ADDED 2026-08-10 (fill-worker wave 2). Job::SetCodeRecycle forwards here, and
        // both X360 call sites resolve to the ICF-folded empty function at 0x82AD5078
        // (a bare `blr`) -- code recycling is an SPU-loader concept and this build has no
        // SPUs. Declared here because the enclosing Job declares its forwarder, and bodied
        // (empty) in the vendor TU so the link closes over both.
        void SetCodeRecycle(CodeRecycle leRecycle);

        // Accessors (declaration-only; bodies live in the vendor entrypoint TU).
        const char*    GetName() const;
        JobEnvironment GetEnvironment() const;
        JobPriority    GetPriority() const;
        JobAffinity    GetAffinity() const;

    private:
        char           mName[16];        // +0x00 entry_point.h:78
        JobPriority    mPriority;        // +0x10 entry_point.h:80
        JobAffinity    mAffinity;        // +0x14 entry_point.h:81 (SetAffinity stores here)
        JobEnvironment mEnvironment;     // +0x18 entry_point.h:82
        KernelSwap     mKernelSwap;      // +0x1C entry_point.h:83
        CodeRecycle    mCodeRecycle;     // +0x20 entry_point.h:84
        bool           mBreakOnEntry;    // +0x24 entry_point.h:85
        bool           mAllowSleepOn;    // +0x25 entry_point.h:86 (+2 pad to +0x28)
        // entry_point.h:89 -- the code to run: a local function pointer or an SPU job image.
        union
        {
            void (*mpfnLocalJob)(Param, Param, Param, Param); // +0x28 entry_point.h:90
            const char* mpcSpuJob;                            // +0x28 entry_point.h:93
        };
        // X360 has no SPU info tail (mSpuJobSize / mSpuJobInfo are PS3-only) -- the
        // object ends here at 44 bytes.
    };
}
}

#endif // EA_JOBS_ENTRY_POINT_H
