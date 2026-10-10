#include "SDKs/Realmc/RealmcIfaceXenonMessageFilter.h"

#include <new>   // placement new -- the console constructs each Response in place

// ===========================================================================
// RealmcIface::XenonMessageFilter -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Both SHAPE and BODY come from the console
// pseudocode + asm. See RealmcIfaceXenonMessageFilter.h for the layout, the
// vtable and the FLAG notes.
// ===========================================================================

namespace RealmcIface
{

namespace
{

// The answer every handler below builds, inlined at each site on the console:
// AllocateMem(null, 12); construct Response(iValue) there when the allocation
// succeeded (null otherwise); wrap it in a stack ResponsePtr; assign that into
// maResponse (MessagePtr::operator=); the stack ResponsePtr is torn down.
void AnswerWith(RealmcCore::ResponsePtr& rHeld, int iValue)
{
    void* const lpMem = RealmcCore::AllocateMem(nullptr, sizeof(RealmcCore::Response));
    RealmcCore::Response* const lpResponse =
        lpMem ? new (lpMem) RealmcCore::Response(iValue) : nullptr;
    const RealmcCore::ResponsePtr lAnswer(lpResponse);
    rHeld = lAnswer;
}

// The hidden-message bits the MessageClear handler tests.
const std::uint32_t KU_HIDDEN_MESSAGE_BIT_0200 = 0x200;
const std::uint32_t KU_HIDDEN_MESSAGE_BIT_2000 = 0x2000;

} // namespace

// ---------------------------------------------------------------------------
// XenonMessageFilter::XenonMessageFilter
//
// MessageFilter(pMemcardState); store the XenonMessageFilter vtable; clear the
// state byte at +0x10.
// ---------------------------------------------------------------------------
XenonMessageFilter::XenonMessageFilter(RealmcCore::MemcardState* pMemcardState)
    : RealmcCore::MessageFilter(pMemcardState),
      mbState(false)
{
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::ProcessMessage(MessageClear*) -- processor slot +0x44
//
//   if (state->GetMainTask() == 2) {
//     if (state->GetCurrentTask() == 16)                   answer 1
//     else if (state->GetCurrentTask() == 2 && !mbState)   answer 1
//   }
//   if (state->GetMainTask() == 8  && (state->GetHiddenMessages() & 0x200))   answer 1
//   if (state->GetMainTask() == 16 && (state->GetHiddenMessages() & 0x2000))  answer 1
//
// The message itself is not read. Every query goes back to the MemcardState
// (each takes its lock), in the console's order.
// ---------------------------------------------------------------------------
void XenonMessageFilter::ProcessMessage(RealmcCore::MessageClear* /*pMessage*/)
{
    RealmcCore::MemcardState* const lpState = static_cast<RealmcCore::MemcardState*>(mpHandler);

    if (lpState->GetMainTask() == 2)
    {
        if (lpState->GetCurrentTask() == 16)
        {
            AnswerWith(maResponse, 1);
        }
        else if (lpState->GetCurrentTask() == 2 && !mbState)
        {
            AnswerWith(maResponse, 1);
        }
    }

    if (lpState->GetMainTask() == 8 &&
        (lpState->GetHiddenMessages() & KU_HIDDEN_MESSAGE_BIT_0200) != 0)
    {
        AnswerWith(maResponse, 1);
    }

    if (lpState->GetMainTask() == 16 &&
        (lpState->GetHiddenMessages() & KU_HIDDEN_MESSAGE_BIT_2000) != 0)
    {
        AnswerWith(maResponse, 1);
    }
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::ProcessMessage(MessageTrc*) -- processor slot +0x48
//
//   if (state->GetAutosaveState() == 1) {           (the returned byte compared to 1)
//     n = message's Trc option count                 (the word at message +0x1C)
//     if (n == 1)      answer 1
//     else if (n == 2) answer 2
//   }
// ---------------------------------------------------------------------------
void XenonMessageFilter::ProcessMessage(RealmcCore::MessageTrc* pMessage)
{
    RealmcCore::MemcardState* const lpState = static_cast<RealmcCore::MemcardState*>(mpHandler);

    if (lpState->GetAutosaveState())
    {
        const int liNumOptions = pMessage->GetTrc().GetNumOptions();
        if (liNumOptions == 1)
        {
            AnswerWith(maResponse, 1);
        }
        else if (liNumOptions == 2)
        {
            AnswerWith(maResponse, 2);
        }
    }
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::Reset -- MessageFilter slot +0x5C
//
// Store 0 to the state byte at +0x10.
// ---------------------------------------------------------------------------
void XenonMessageFilter::Reset()
{
    mbState = false;
}

// ---------------------------------------------------------------------------
// XenonMessageFilter::~XenonMessageFilter
//
// Reinstall the XenonMessageFilter vtable and run ~MessageFilter; the vector
// deleting destructor then frees 20 bytes through FreeMemSize when its delete
// flag is set. The body is empty: the base dtor does the teardown.
// ---------------------------------------------------------------------------
XenonMessageFilter::~XenonMessageFilter()
{
}

} // namespace RealmcIface
