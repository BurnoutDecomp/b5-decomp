// BB_runtime.job_manager.cpp: the EA job manager 2.01.02 bulk-build translation unit.
// Each member also compiles on its own; this file holds only their includes.
// Evidence:
// - The package was compiled as one bulk TU of this name: the PS3 DecFIGS symbols of its
//   file-local names carry it, e.g. EA::Jobs `anonymous namespace'::gAllocator =
//   _ZN2EA4Jobs166_GLOBAL__N_D__Dev_depot_Burnout_MAIN_Code_SDKs_Packages_eatech_dev_build_
//   job_manager_2.01.02_ps3_gcc_dev_opt_build_source_BB_runtime.job_manager.cpp_... (145 symbols).
// - ARTIST holds the package as one contiguous object, 0x82BC9830-0x82BCC6B0, its members'
//   functions interleaved in four emission passes, and inlines accessors across members with no
//   out-of-line copy anywhere: JobInstanceHandle::Valid() into Job::IsDone 0x82BCA2E8, SleepOn
//   0x82BCA1F8 and WaitOn 0x82BCB238; JobThreadHandle::Handle() into LocalBackend::GetThreadId
//   0x82BC9E30, WaitForEnd 0x82BCA738 and GetThreadParameters 0x82BCA770.
// Members: the package sources of this tree (SDKs/EATech/eajobs). Order: the linear order that
// keeps the most adjacent member transitions of ARTIST's layout forward (17 of 21), first member
// jobs.cpp (the object's first function, SetAllocator 0x82BC9830); members with no function
// ARTIST places in the range follow alphabetically.

#include "jobs.cpp"
#include "entrypoint.cpp"
#include "event.cpp"
#include "job_thread.cpp"
#include "bucket_list_node.cpp"
#include "job.cpp"
#include "job_instance_handle.cpp"
#include "reference_count.cpp"
#include "job_scheduler.cpp"
#include "detail.cpp"
#include "local_backend.cpp"
#include "job_thread_handle.cpp"
#include "job_thread_id.cpp"
#include "job_thread_parameters.cpp"
