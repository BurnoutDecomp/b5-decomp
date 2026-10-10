#pragma once

// ===========================================================================
// RealmcCore::ObjectManager -- creates and deletes the three shared message
// holders of the Realmc memory-card library (the RealmcIface / RealmcCore family
// in the console image):
//
//   g_pRealmcEmptyMessage       -- MessagePtr over a bare Message
//                                  (MessagePtr::EMPTY_MESSAGE)
//   g_pRealmcEmptyResponse      -- ResponsePtr over Response(0)
//                                  (ResponsePtr::EMPTY_RESPONSE)
//   g_pRealmcUnfilteredResponse -- ResponsePtr over Response(5)
//                                  (MessageFilter::FilterMessage's "no answer")
//
// The holders and their globals are declared in RealmcCore.h.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the console pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, ObjectManager, Initialize,
// Finalize) are preserved verbatim.
// ===========================================================================

namespace RealmcCore
{

class ObjectManager
{
public:
    // Create the three holders in order. Each block allocates the 8-byte holder
    // through the backend (AllocateMem), and when that succeeded allocates and
    // constructs its message (a Message, Response(0), Response(5)), binds the
    // holder to it (MessagePtr / ResponsePtr, AddRef) and stores the holder in
    // its global; when the holder allocation failed the global is nulled.
    static void Initialize();

    // Delete each non-null holder through its deleting destructor (Release the
    // held message, free the holder), then null all three globals.
    static void Finalize();
};

} // namespace RealmcCore
