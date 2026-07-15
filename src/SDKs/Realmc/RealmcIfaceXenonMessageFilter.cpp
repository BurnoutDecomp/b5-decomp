#include "SDKs/Realmc/RealmcIfaceXenonMessageFilter.h"

#include <new>   // placement new -- reproduces the X360 `Response::Response(Mem, value)`

// ===========================================================================
// RealmcIface::XenonMessageFilter -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: both SHAPE and BODY come from the X360 pseudocode +
// asm. See RealmcIfaceXenonMessageFilter.h for the layout, the base
// (RealmcCore::MessageFilter), and the FLAG notes.
//
// Bodied here:
//   XenonMessageFilter::XenonMessageFilter          @0x82B54BE0
//   XenonMessageFilter::ProcessMessage              @0x82B54C38
//   XenonMessageFilter::Reset                       @0x82B54C28
//   XenonMessageFilter::`vector deleting destructor`@0x82B54D20
//     (compiler-generated from the virtual dtor + the class operator delete)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// XenonMessageFilter::XenonMessageFilter @ 0x82B54BE0
//
//   bl   RealmcCore::MessageFilter::MessageFilter   -> base ctor (forwards the handler)
//   stw  off_82148728, 0(this)                      -> final XenonMessageFilter vtable
//   stb  r10(0), 0x10(this)                          -> mbState = 0
//
// The vtable store at +0 is MSVC's derived-ctor prologue (emitted by the compiler
// here). The handler forwarded to the base is the RealmcCore::MemcardState the
// filter later queries via GetAutosaveState.
// ---------------------------------------------------------------------------
XenonMessageFilter::XenonMessageFilter(RealmcCore::MemcardState* pMemcardState)
    : RealmcCore::MessageFilter(pMemcardState),
      mbState(false)
{
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::ProcessMessage @ 0x82B54C38
//
//   lwz  r3, 4(this) ; bl MemcardState::GetAutosaveState -> mpHandler->GetAutosaveState()
//   clrlwi r11, r3, 24 ; cmplwi 1 ; bne exit            -> gate on autosave == 1 (active)
//   lwz  r11, 0x1C(msg)                                  -> incoming message's result word
//   cmplwi 1 -> value 1 branch ; cmplwi 2 -> value 2 branch ; else exit
//     li r4, 0xC ; li r3, 0 ; bl AllocateMem             -> Mem = AllocateMem(nullptr, 12)
//     (Mem ? li r4, value ; bl Response::Response : 0)    -> new(Mem) Response((void*)value)
//     bl ResponsePtr::ResponsePtr                         -> ResponsePtr lRp(response)
//     addi r3, this, 8 ; bl MessagePtr::operator=         -> maMessage = lRp
//     bl ResponsePtr::~ResponsePtr                        -> ~lRp
//
// The held MessagePtr slot at this+8 is the inherited MessageFilter::maMessage. The
// X360 builds a RealmcCore::ResponsePtr over the Response and assigns it straight
// into that MessagePtr: ResponsePtr and MessagePtr are layout-identical smart
// pointers that share the same refcount teardown (ResponsePtr::~ResponsePtr tail-
// branches into MessagePtr::~MessagePtr), so the assignment rebinds maMessage to the
// Response through the shared RefCount machinery. That type-pun is reproduced below
// with a reference reinterpret_cast -- exactly what the binary does. The two value
// branches (1 / 2) are kept separate to mirror the asm's two distinct blocks.
// Returns the autosave state (the function's dominant return; the matched branch's
// register leftover from ~ResponsePtr is a compiler artifact, not a real result).
// ---------------------------------------------------------------------------
bool XenonMessageFilter::ProcessMessage(void* pMessage)
{
    const bool lbAutosaveActive = static_cast<RealmcCore::MemcardState*>(mpHandler)->GetAutosaveState();
    if (lbAutosaveActive)
    {
        // FLAG: the incoming message type is un-homed; its result/status word lives
        // at the attested byte offset +0x1C and selects the response value (1 or 2).
        const int liResult = *reinterpret_cast<const int*>(static_cast<const u8*>(pMessage) + 0x1C);

        if (liResult == 1)
        {
            void* lpMem = RealmcCore::AllocateMem(nullptr, 12);
            RealmcCore::Response* lpResponse =
                lpMem ? new (lpMem) RealmcCore::Response(reinterpret_cast<void*>(1)) : nullptr;

            RealmcCore::ResponsePtr lResponse(lpResponse);
            // maMessage = lResponse (type-punned: ResponsePtr storage passed to MessagePtr::operator=).
            maMessage = reinterpret_cast<const RealmcCore::MessagePtr&>(lResponse);
        }   // ~lResponse here (Release) -- matches the X360 ~ResponsePtr ordering
        else if (liResult == 2)
        {
            void* lpMem = RealmcCore::AllocateMem(nullptr, 12);
            RealmcCore::Response* lpResponse =
                lpMem ? new (lpMem) RealmcCore::Response(reinterpret_cast<void*>(2)) : nullptr;

            RealmcCore::ResponsePtr lResponse(lpResponse);
            maMessage = reinterpret_cast<const RealmcCore::MessagePtr&>(lResponse);
        }   // ~lResponse here (Release)
    }

    return lbAutosaveActive;
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::Reset @ 0x82B54C28
//
//   li  r11, 0 ; stb r11, 0x10(this)  -> mbState = 0
// ---------------------------------------------------------------------------
void XenonMessageFilter::Reset()
{
    mbState = false;
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::~XenonMessageFilter @ 0x82B54D20 (backs the `vector deleting
// destructor')
//
//   stw  off_82148728, 0(this)                         -> (re)install the final vtable
//   bl   RealmcCore::MessageFilter::~MessageFilter     -> base teardown (Release maMessage,
//                                                         restore off_82148660)
//   (a2 & 1) ? li r4, 0x14 ; bl RealmcCore::FreeMemSize : --  -> free 20 bytes if delete flag set
//
// All real teardown lives in the MessageFilter base dtor (destroying the embedded
// MessagePtr); this override's own body is empty. The vtable install + base dtor
// call + the sized free are the compiler-generated `vector deleting destructor'
// (the virtual dtor + the class operator delete), reproduced by the empty body
// plus operator delete in the header.
// ---------------------------------------------------------------------------
XenonMessageFilter::~XenonMessageFilter()
{
}

} // namespace RealmcIface
