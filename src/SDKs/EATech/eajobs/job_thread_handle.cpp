#include "SDKs/EATech/eajobs/job_thread_handle.h"

// EA::Jobs::JobThreadHandle member bodies. DWARF job_thread_handle.h:37 declares the
// index accessor; ARTIST keeps no out-of-line copy (the job manager is a bulk TU,
// BB_runtime.job_manager.cpp, so its callers in LocalBackend read the spilled handle's
// index word directly, e.g. WaitForEnd 0x82BCA744/0x82BCA748).

namespace EA
{
namespace Jobs
{
    u32 JobThreadHandle::Handle() const
    {
        return mHandle;
    }
}
}
