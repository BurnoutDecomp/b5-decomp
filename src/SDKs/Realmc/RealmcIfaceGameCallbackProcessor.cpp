#include "SDKs/Realmc/RealmcIfaceGameCallbackProcessor.h"

// ===========================================================================
// RealmcIface::GameCallbackProcessor -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: both SHAPE and BODY come from the X360 pseudocode +
// asm. See RealmcIfaceGameCallbackProcessor.h for the layout, the base
// (RealmcCore::IRealmcMessageFilter), and the FLAG notes.
//
// Bodied here:
//   GameCallbackProcessor::GameCallbackProcessor @0x82B54968
//   GameCallbackProcessor::ProcessMessage        @0x82B54388
//   GameCallbackProcessor::~GameCallbackProcessor@0x82B54340
//   GameCallbackProcessor::`vector deleting destructor` @0x82B549B8
//     (compiler-generated from the virtual dtor + the class operator delete)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// GameCallbackProcessor::GameCallbackProcessor @ 0x82B54968
//
//   stw  r4, 4(this)                     -> mpContext = pContext
//   stw  off_821486B8, 0(this)           -> final GameCallbackProcessor vtable
//   stw  r5, 8(this)                     -> mpQueue = pQueue
//   bl   RealmcCore::MessagePtr::EMPTY_MESSAGE
//   addi r3, this, 0xC ; bl sub_82C457F8 -> construct maMessage(EMPTY_MESSAGE())
//
// The vtable store at +0 is MSVC's derived-ctor prologue; the MessagePtr subobject
// at +0xC is constructed bound to the shared empty message, AddRefing it -- exactly
// RealmcCore::MessagePtr's own ctor (the same idiom RealmcCore::MessageFilter uses).
// ---------------------------------------------------------------------------
GameCallbackProcessor::GameCallbackProcessor(void* pContext, RealmcCore::MessageQueue* pQueue)
    : mpContext(pContext),
      mpQueue(pQueue),
      maMessage(RealmcCore::MessagePtr::EMPTY_MESSAGE())   // binds + AddRefs the empty message
{
}

// ---------------------------------------------------------------------------
// GameCallbackProcessor::ProcessMessage @ 0x82B54388
//
//   addi r3, sp, resp ; addi r30, this, 0xC
//   bl   RealmcCore::ResponsePtr::ResponsePtr   -> ResponsePtr lResponse(pResponse)
//   mr   r5, r3 ; mr r4, r30 ; lwz r3, 8(this)
//   bl   RealmcCore::MessageQueue::PostResponse -> mpQueue->PostResponse(maMessage, lResponse)
//   addi r3, sp, resp
//   bl   RealmcCore::ResponsePtr::~ResponsePtr  -> ~lResponse (Release)
//   bl   RealmcCore::MessagePtr::EMPTY_MESSAGE
//   mr   r4, r3 ; mr r3, r30
//   bl   RealmcCore::MessagePtr::operator=      -> maMessage = MessagePtr(EMPTY_MESSAGE())
//
// The stack ResponsePtr is scoped so its destructor (Release) runs BEFORE the held
// message is reset, matching the X360's explicit ~ResponsePtr ahead of the rebind.
// The rebind is modelled exactly as RealmcCore::MessageFilter::FilterMessage does:
// build a temporary MessagePtr over the empty message and assign it (operator=
// releases the old held message and AddRefs the empty one). r3 on exit == &maMessage
// (operator='s returned *this).
// ---------------------------------------------------------------------------
RealmcCore::MessagePtr& GameCallbackProcessor::ProcessMessage(RealmcCore::Response* pResponse)
{
    {
        // Wrap the incoming response (AddRef) on the stack and post the
        // (held message, response) pair onto the cross-thread queue.
        RealmcCore::ResponsePtr lResponse(pResponse);
        mpQueue->PostResponse(maMessage, lResponse);
    }   // ~ResponsePtr here (Release) -- matches the X360 ordering

    // Reset the held message back to the shared empty message and return it.
    RealmcCore::MessagePtr lEmpty(RealmcCore::MessagePtr::EMPTY_MESSAGE());
    return maMessage = lEmpty;
}

// ---------------------------------------------------------------------------
// GameCallbackProcessor::~GameCallbackProcessor @ 0x82B54340
//
//   stw  off_821486B8, 0(this)           -> (re)install the final vtable
//   addi r3, this, 0xC
//   bl   RealmcCore::MessagePtr::~MessagePtr  -> tear down maMessage (Release + null)
//   stw  off_82148660, 0(this)           -> restore the abstract-base vtable
//
// The embedded MessagePtr subobject's destructor (Release the held message) is the
// only real work; the two vtable stores frame the base-class teardown the compiler
// emits. The member is destroyed automatically -- the X360's inlined
// MessagePtr::~MessagePtr on this+0xC.
// ---------------------------------------------------------------------------
GameCallbackProcessor::~GameCallbackProcessor()
{
}

} // namespace RealmcIface
