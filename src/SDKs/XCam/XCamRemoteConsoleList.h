#pragma once

// ===========================================================================
// XCAM::CRemoteConsoleList -- the fixed pool of remote-peer receive pipelines in
// the X360 XCAM (Xbox Live Vision camera) streaming SDK of
// BURNOUT_X360_ARTIST.XEX. It owns 30 CRemoteConsole objects by value and keeps
// two intrusive circular doubly-linked lists over them: an ACTIVE list (peers
// currently registered) and a FREE list (idle pool). A peer is registered by
// popping a free console, stamping it with the peer address and appending it to
// the active list (ActivateRemoteConsole), and unregistered by the reverse
// (DeActivateRemoteConsole). All traversals take the embedded reader/writer lock.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamRemoteConsoleList.cpp:
//
//     XCAM::CRemoteConsoleList::CRemoteConsoleList                @ 0x82981CE8
//     XCAM::CRemoteConsoleList::Initialize                       @ 0x82982F30
//     XCAM::CRemoteConsoleList::Shutdown                         @ 0x82982FE8
//     XCAM::CRemoteConsoleList::DoWork                           @ 0x82983048
//     XCAM::CRemoteConsoleList::GetActiveConsoleAddresses        @ 0x82983168
//     XCAM::CRemoteConsoleList::GetFirstActiveRemoteConsole      @ 0x829831F8
//     XCAM::CRemoteConsoleList::GetNextActiveRemoteConsole       @ 0x82983228
//     XCAM::CRemoteConsoleList::SetEnabledStateForAllActiveConsoles @ 0x82983258
//     XCAM::CRemoteConsoleList::ActivateRemoteConsole            @ 0x82983430
//     XCAM::CRemoteConsoleList::DeActivateRemoteConsole          @ 0x82983500
//
// (XCAM::CRemoteConsoleList::FindRemoteConsole, which ActivateRemoteConsole and
// DeActivateRemoteConsole call to locate a console by peer address, is its own TU
// -- declared here but not defined.)
//
// There is no reference source and no DWARF for this TU; the SHAPE below is
// reconstructed store-for-store from the X360 asm (member ORDER is the load/store
// order off `this`; the X360 offsets are noted in comments -- e.g. the two list
// heads land at +0x8610 / +0x8618 == 30 * 0x478-byte consoles -- but the PC
// layout widens the console pointers so those displacements do not carry over,
// and access is by named member throughout). `XCAM` is an X360 SDK boundary, so
// its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamRemoteConsole.h"

namespace XCAM
{

// --- Xbox-360 executive reader/writer lock (platform boundary) ---------------
// The kernel ERWLOCK the list guards its traversals with. Its exact layout is a
// platform detail; modelled as an opaque fixed-size block (only its address is
// taken, never a field). Supplied by the platform layer; NOT reconstructed here.
struct ERWLOCK
{
    u8 mOpaque[0x18];
};

// The executive reader/writer-lock entry points the list drives. Declared with
// the argument/return shapes the X360 call sites use (ExReleaseReadWriteLock's
// result is passed straight back as the return of DoWork / GetActiveConsole-
// Addresses, so it is typed to return int). Supplied by the platform layer.
void ExInitializeReadWriteLock(ERWLOCK* pLock);
void ExAcquireReadWriteLockShared(ERWLOCK* pLock);
void ExAcquireReadWriteLockExclusive(ERWLOCK* pLock);
int  ExReleaseReadWriteLock(ERWLOCK* pLock);

class CRemoteConsoleList
{
public:
    // @ 0x82981CE8 -- construct the 30-console pool (each console default-built)
    // and leave both intrusive lists empty.
    CRemoteConsoleList();

    // @ 0x82982F30 -- reset both lists, bring up pConfig->miConsoleCount consoles
    // (each pushed onto the free list) and initialise the lock. Returns 0, or the
    // first console's Initialize failure code (in which case the lock is left
    // uninitialised, matching the asm early-out).
    int Initialize(RemoteConsoleConfig* pConfig);

    // @ 0x82982FE8 -- shut every configured console's decoder and depacketizer
    // subobject down. Returns the last depacketizer-shutdown result (0 if none).
    int Shutdown();

    // @ 0x82983048 -- pump every active console once, under the shared lock.
    int DoWork();

    // @ 0x82983168 -- copy the 36-byte peer address of each active console into
    // pOutAddresses (up to *pInOutCount entries), writing the count actually
    // copied back through pInOutCount. Under the shared lock.
    int GetActiveConsoleAddresses(void* pOutAddresses, u32* pInOutCount);

    // @ 0x829831F8 -- the first active console, or nullptr if none. No lock.
    CRemoteConsole* GetFirstActiveRemoteConsole();

    // @ 0x82983228 -- the active console after pConsole, or nullptr at the end.
    CRemoteConsole* GetNextActiveRemoteConsole(CRemoteConsole* pConsole);

    // @ 0x82983258 -- set every active console's disabled flag to (iEnabled == 0).
    // Under the shared lock.
    int SetEnabledStateForAllActiveConsoles(int iEnabled);

    // @ 0x82983430 -- register a peer: if it is not already active and a free
    // console is available, pop it, stamp the 36-byte address and append it to the
    // active list. Returns the (existing or newly activated) console, or nullptr.
    // Under the shared lock.
    CRemoteConsole* ActivateRemoteConsole(const void* pAddress);

    // @ 0x82983500 -- unregister a peer: locate its console, unlink it from the
    // active list, return it to the free list (under the exclusive lock) and
    // re-initialise it. Returns 0, or 87 (ERROR_INVALID_PARAMETER) if not found.
    int DeActivateRemoteConsole(const void* pAddress);

    // Locate an active console by its 36-byte peer address. Its own TU; declared
    // so the two (de)activate bodies compile against it.  @ external
    CRemoteConsole* FindRemoteConsole(const void* pAddress);

    // --- layout (member ORDER store-for-store with the X360 asm) ---
    CRemoteConsole maConsoles[30];   // +0x0000 the pool (30 * 0x478 on X360)
    ListLink       mActiveList;      // +0x8610 head of the active (registered) list
    ListLink       mFreeList;        // +0x8618 head of the free (idle) list
    s32            miActiveCount;    // +0x8620 zeroed by Initialize (activation count)
    RemoteConsoleConfig* mpInitData; // +0x8624 stream config captured at Initialize
    ERWLOCK        mLock;            // +0x8628 traversal reader/writer lock
};

} // namespace XCAM
