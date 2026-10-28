#include "SDKs/Realmc/RealmcObjectManager.h"
#include "SDKs/Realmc/RealmcCore.h"   // MessagePtr / ResponsePtr / Message / Response, the holder globals

#include <new>   // placement new -- every object is constructed in backend memory

// ===========================================================================
// RealmcCore::ObjectManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both come from the console asm.
// See RealmcObjectManager.h.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// ObjectManager::Initialize
//
// Three blocks, one per holder:
//   holder = AllocateMem(null, 8)
//   if (holder) {
//     msg = AllocateMem(null, 8 or 12)
//     construct the message there when that succeeded (RefCount vtable, atomic
//       zero of the count, the result word for a Response, final vtable)
//     construct the holder over it (MessagePtr vtable, atomic increment of the
//       message's count, store; then the ResponsePtr vtable for blocks 2 and 3)
//     store the holder in its global
//   } else the global = null
// The console increments the count of a null message too (a fault on a failed
// allocation); the MessagePtr constructor reproduces that unguarded increment.
// ---------------------------------------------------------------------------
void ObjectManager::Initialize()
{
    if (void* const lpHolder = AllocateMem(0, sizeof(MessagePtr)))
    {
        void* const lpMessage = AllocateMem(0, sizeof(Message));
        Message* const pMessage = lpMessage ? new (lpMessage) Message() : 0;
        g_pRealmcEmptyMessage = new (lpHolder) MessagePtr(pMessage);
    }
    else
    {
        g_pRealmcEmptyMessage = 0;
    }

    if (void* const lpHolder = AllocateMem(0, sizeof(ResponsePtr)))
    {
        void* const lpResponse = AllocateMem(0, sizeof(Response));
        Response* const pResponse = lpResponse ? new (lpResponse) Response(0) : 0;
        g_pRealmcEmptyResponse = new (lpHolder) ResponsePtr(pResponse);
    }
    else
    {
        g_pRealmcEmptyResponse = 0;
    }

    if (void* const lpHolder = AllocateMem(0, sizeof(ResponsePtr)))
    {
        void* const lpResponse = AllocateMem(0, sizeof(Response));
        Response* const pResponse = lpResponse ? new (lpResponse) Response(5) : 0;
        g_pRealmcUnfilteredResponse = new (lpHolder) ResponsePtr(pResponse);
    }
    else
    {
        g_pRealmcUnfilteredResponse = 0;
    }
}

// ---------------------------------------------------------------------------
// ObjectManager::Finalize
//
// For each global in order: when non-null, call its vtable slot +0 with the
// delete flag (the scalar deleting destructor: Release the held message, free
// the holder through the backend). Then store null to all three globals.
// ---------------------------------------------------------------------------
void ObjectManager::Finalize()
{
    if (g_pRealmcEmptyMessage)
        delete g_pRealmcEmptyMessage;
    if (g_pRealmcEmptyResponse)
        delete g_pRealmcEmptyResponse;
    if (g_pRealmcUnfilteredResponse)
        delete g_pRealmcUnfilteredResponse;

    g_pRealmcEmptyMessage       = 0;
    g_pRealmcEmptyResponse      = 0;
    g_pRealmcUnfilteredResponse = 0;
}

} // namespace RealmcCore
