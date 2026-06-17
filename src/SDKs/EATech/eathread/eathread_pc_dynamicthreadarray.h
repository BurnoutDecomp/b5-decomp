#pragma once

// X360-faithful variant of EA::Thread::DynamicThreadArray.
//
// IMPORTANT (committed-type swap -- FLAG, do not silently overwrite):
// b5-decomp/vendor/EAThread/source/pc/eathread_pc.cpp ALREADY contains a
// DynamicThreadArray, but it is the *modern* upstream EAThread variant
// (kMaxThreadDynamicArrayCount = 128; lazy Futex-based Initialize();
// mhDynamicThreadArray / mCriticalSection as independent static members).
//
// The Burnout 5 X360 binary (this dossier) links an *older* EAThread variant:
//   * fixed array of 24 entries (loops gate on >= 0x18 / counter starts at 24),
//   * NO Initialize() call -- both methods enter the OS critical section
//     directly (the section is constructed elsewhere, e.g. a ctor/Initialize
//     not in this TU),
//   * the array and the critical section live in ONE instance: the binary
//     reaches mCriticalSection at byte offset 96 == &mhDynamicThreadArray[24],
//     and both functions take the instance base as their first ("this") arg.
//
// Below is the X360-authoritative shape. Both CheckDynamicThreadArray and
// AddDynamicThreadHandle are reconstructed as instance methods to match the
// binary's single-base-pointer access (a1 / result == this). Per project rule
// "fix committed types by GROWING the home, not redefining", the conductor must
// decide whether to (a) gate this variant behind an X360 platform branch in
// eathread_pc.cpp, or (b) accept that the committed upstream already differs
// and keep the X360 facts in a comment. I did NOT rewrite the committed copy.

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