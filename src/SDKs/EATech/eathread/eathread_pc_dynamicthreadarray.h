#pragma once

// ARTIST 0x82B42328 and 0x82B423B0 identify the EA::Thread member functions.
// They operate on the same 24-handle table and its critical section at +96.
namespace EA
{
namespace Thread
{

struct DynamicThreadArray
{
    // X360: fixed 24-handle table guarded by an OS critical section that sits
    // immediately after the table (offset 96). HANDLE == void* in this codebase.
    static const size_t kMaxThreadDynamicArrayCount = 24; // X360: 24 (upstream PC build uses 128)

    HANDLE           mhDynamicThreadArray[kMaxThreadDynamicArrayCount]; // offset 0, 24 * 4 = 96 bytes on X360
    CRITICAL_SECTION mCriticalSection;                                  // offset 96

    // Declared-only here (defined elsewhere in the X360 build; not in this TU):
    void Initialize();

    // Reconstructed in this TU:
    void CheckDynamicThreadArray(bool bCloseAll);
    void AddDynamicThreadHandle(HANDLE hThread, bool bAdd);
};

} // namespace Thread
} // namespace EA
