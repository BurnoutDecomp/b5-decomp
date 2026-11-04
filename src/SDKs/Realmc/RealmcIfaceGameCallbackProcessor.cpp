#include "SDKs/Realmc/RealmcIfaceGameCallbackProcessor.h"
#include "SDKs/Realmc/RealmcIfaceOpaqueMessages.h"
#include "SDKs/Realmc/RealmcIfaceMessageSetActiveCardDone.h"
#include "SDKs/Realmc/RealmcIfaceMessageCardRemoved.h"
#include "SDKs/Realmc/RealmcIfaceMessageCheckLoadedData.h"
#include "SDKs/Realmc/RealmcIfaceMessageSetAutosaveDone.h"
#include "SDKs/Realmc/RealmcIfaceMessageLoadDone.h"
#include "SDKs/Realmc/RealmcIfaceMessageSaveDone.h"
#include "SDKs/Realmc/RealmcIfaceMessages.h"
#include "SDKs/Realmc/RealmcMessage.h"
#include "SDKs/Realmc/RealmcCoreMessageClear.h"
#include "SDKs/Realmc/RealmcTrc.h"

#include <new>

// ===========================================================================
// RealmcIface::GameCallbackProcessor -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: both SHAPE and BODY come from the X360 pseudocode +
// asm. See RealmcIfaceGameCallbackProcessor.h for the layout, the base
// (RealmcCore::IMessageProcessor), and the FLAG notes.
//
// Bodied here:
//   GameCallbackProcessor::GameCallbackProcessor @0x82B54968
//   GameCallbackProcessor::ProcessMessage        @0x82B54388
//   GameCallbackProcessor::~GameCallbackProcessor@0x82B54340
//   GameCallbackProcessor::`vector deleting destructor` @0x82B549B8
//     (compiler-generated from the virtual dtor + the class operator delete)
//   the 20 message handlers in vtable off_821486B8 (0x82B543F0..0x82B54B60)
// ===========================================================================

namespace RealmcIface
{

namespace
{

void PostEmptyResponse(RealmcCore::MessageQueue* pQueue,
                       RealmcCore::Message* pMessage)
{
    RealmcCore::MessagePtr lMessage(pMessage);
    // The native queue takes a non-const reference but copies the holder into
    // its node and does not modify it (PostResponse @0x82C46C98).
    RealmcCore::ResponsePtr& lrEmptyResponse = const_cast<RealmcCore::ResponsePtr&>(
        RealmcCore::ResponsePtr::EMPTY_RESPONSE());
    pQueue->PostResponse(lMessage, lrEmptyResponse);
}

void PostResponse(RealmcCore::MessageQueue* pQueue,
                  RealmcCore::Message* pMessage,
                  RealmcCore::Response* pResponse)
{
    RealmcCore::MessagePtr lMessage(pMessage);
    RealmcCore::ResponsePtr lResponse(pResponse);
    pQueue->PostResponse(lMessage, lResponse);
}

} // namespace

// ---------------------------------------------------------------------------
// GameCallbackProcessor::GameCallbackProcessor @ 0x82B54968
//
//   stw  r4, 4(this)                     -> mpGameInterface = pGameInterface
//   stw  off_821486B8, 0(this)           -> final GameCallbackProcessor vtable
//   stw  r5, 8(this)                     -> mpQueue = pQueue
//   bl   RealmcCore::MessagePtr::EMPTY_MESSAGE
//   addi r3, this, 0xC ; bl sub_82C457F8 -> construct maMessage(EMPTY_MESSAGE())
//
// The vtable store at +0 is MSVC's derived-ctor prologue; the MessagePtr subobject
// at +0xC is constructed bound to the shared empty message, AddRefing it -- exactly
// RealmcCore::MessagePtr's own ctor (the same idiom RealmcCore::MessageFilter uses).
// ---------------------------------------------------------------------------
GameCallbackProcessor::GameCallbackProcessor(IGameInterface* pGameInterface,
                                             RealmcCore::MessageQueue* pQueue)
    : mpGameInterface(pGameInterface),
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
// releases the old held message and AddRefs the empty one). Nothing reads the
// handler's result.
// ---------------------------------------------------------------------------
void GameCallbackProcessor::ProcessMessage(RealmcCore::Response* pResponse)
{
    {
        // Wrap the incoming response (AddRef) on the stack and post the
        // (held message, response) pair onto the cross-thread queue.
        RealmcCore::ResponsePtr lResponse(pResponse);
        mpQueue->PostResponse(maMessage, lResponse);
    }   // ~ResponsePtr here (Release) -- matches the X360 ordering

    // Reset the held message back to the shared empty message.
    maMessage = RealmcCore::MessagePtr::EMPTY_MESSAGE();
}

// ARTIST slot +0x04, handler @0x82B54740.
void GameCallbackProcessor::ProcessMessage(MessageSetActiveCardDone* pMessage)
{
    mpGameInterface->SetActiveCardDone(pMessage->GetField08(), pMessage->GetField0C());
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x08 points at the shared no-op body 0x82AD5078.
void GameCallbackProcessor::ProcessMessage(MessageSlot08Payload*)
{
}

// ARTIST slot +0x0C, handler @0x82B54950. This callback returns directly and
// ARTIST does not post a queue response on this path.
void GameCallbackProcessor::ProcessMessage(MessageCardRemoved*)
{
    mpGameInterface->CardRemoved();
}

// ARTIST slot +0x10, handler @0x82B547B8.
void GameCallbackProcessor::ProcessMessage(MessageCheckLoadedData* pMessage)
{
    const bool lbLoadedDataIsValid = mpGameInterface->CheckLoadedData(
        pMessage->GetCount(), pMessage->GetEntries());

    void* const lpMemory = RealmcCore::AllocateMem(0, sizeof(RealmcCore::Response));
    RealmcCore::Response* const lpResponse = lpMemory
        ? new (lpMemory) RealmcCore::Response(lbLoadedDataIsValid)
        : 0;
    PostResponse(mpQueue, pMessage, lpResponse);
}

// ARTIST slot +0x14, handler @0x82B548E0.
void GameCallbackProcessor::ProcessMessage(MessageClearEntriesPayload* pMessage)
{
    mpGameInterface->ClearEntries();
    PostEmptyResponse(mpQueue, &pMessage->mHeader);
}

// ARTIST slot +0x18, handler @0x82B54860.
void GameCallbackProcessor::ProcessMessage(MessageSetAutosaveDone* pMessage)
{
    mpGameInterface->SetAutosaveDone(
        static_cast<s32>(pMessage->GetField08()),
        static_cast<s32>(pMessage->GetField0C()),
        static_cast<s32>(pMessage->GetField10()));
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x1C points at the shared no-op body 0x82AD5078.
void GameCallbackProcessor::ProcessMessage(MessageSlot1CPayload*)
{
}

// ARTIST slot +0x20, handler @0x82B546C0.
void GameCallbackProcessor::ProcessMessage(MessageCallback3CPayload* pMessage)
{
    mpGameInterface->Callback3C(pMessage->maArguments[0],
                                pMessage->maArguments[1],
                                pMessage->maArguments + 2);
    PostEmptyResponse(mpQueue, &pMessage->mHeader);
}

// ARTIST slot +0x24, handler @0x82B54648. It forwards the address of +0x08,
// rather than loading a pointer from that word.
void GameCallbackProcessor::ProcessMessage(MessageFoundEntryPayload* pMessage)
{
    mpGameInterface->FoundEntry(pMessage->maPayload08);
    PostEmptyResponse(mpQueue, &pMessage->mHeader);
}

// ARTIST slot +0x28, handler @0x82B545D0.
void GameCallbackProcessor::ProcessMessage(MessageFindEntriesDonePayload* pMessage)
{
    mpGameInterface->FindEntriesDone(pMessage->muField08, pMessage->muField0C);
    PostEmptyResponse(mpQueue, &pMessage->mHeader);
}

// ARTIST slot +0x2C points at the shared no-op body 0x82AD5078.
void GameCallbackProcessor::ProcessMessage(MessageSlot2CPayload*)
{
}

// ARTIST slot +0x30, handler @0x82B54550.
void GameCallbackProcessor::ProcessMessage(MessageLoadDone* pMessage)
{
    mpGameInterface->LoadDone(pMessage->GetId(), pMessage->GetField24(),
                              pMessage->GetField1C(), pMessage->GetField20());
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x34, handler @0x82B54B60.
void GameCallbackProcessor::ProcessMessage(MessageSaveDone* pMessage)
{
    mpGameInterface->SaveDone(pMessage->GetId(), pMessage->GetField1C(),
                              pMessage->GetText());
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x38, handler @0x82B54AC0.
void GameCallbackProcessor::ProcessMessage(MessageSaveCheckDonePayload* pMessage)
{
    const s32* lpValues = 0;
    s32 liValueCount = 0;
    if (pMessage->mpValuesBegin != pMessage->mpValuesEnd)
    {
        lpValues = pMessage->mpValuesBegin;
        liValueCount = static_cast<s32>(pMessage->mpValuesEnd - pMessage->mpValuesBegin);
    }

    mpGameInterface->SaveCheckDone(
        pMessage->muField08, pMessage->muField0C, pMessage->muField20,
        pMessage->muField24, lpValues, liValueCount);
    PostEmptyResponse(mpQueue, &pMessage->mHeader);
}

// ARTIST slot +0x3C, handler @0x82B544D8.
void GameCallbackProcessor::ProcessMessage(MessageBootupDone* pMessage)
{
    mpGameInterface->BootupCheckDone(pMessage->GetResult());
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x40, handler @0x82B54460.
void GameCallbackProcessor::ProcessMessage(MessageShowAutosaveIcon* pMessage)
{
    mpGameInterface->ShowAutosaveIcon(pMessage->IsVisible());
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x44, handler @0x82B543F0.
void GameCallbackProcessor::ProcessMessage(RealmcCore::MessageClear* pMessage)
{
    mpGameInterface->ClearMessage();
    PostEmptyResponse(mpQueue, pMessage);
}

// ARTIST slot +0x48, handler @0x82B54A08. Keep the prompt alive while the game
// UI owns it; a zero-option prompt answers through the native empty Response.
void GameCallbackProcessor::ProcessMessage(RealmcCore::MessageTrc* pMessage)
{
    maMessage = RealmcCore::MessagePtr(pMessage);

    const RealmcCore::Trc& lrTrc = pMessage->GetTrc();
    const u32 luOptionCount = static_cast<u32>(lrTrc.GetNumOptions());
    const char* laOptions[4];
    for (int i = 0; i < 4; ++i)
        laOptions[i] = reinterpret_cast<const char*>(lrTrc.GetOptionText(i));

    mpGameInterface->ShowMessage(
        reinterpret_cast<const char*>(lrTrc.GetMessageText()), luOptionCount, laOptions);
    if (luOptionCount == 0)
        RealmcCore::ResponsePtr::EMPTY_RESPONSE().Apply(this);
}

// ARTIST slot +0x4C points at the shared no-op body 0x82AD5078.
void GameCallbackProcessor::ProcessMessage(RealmcCore::MessageString*)
{
}

// ARTIST slot +0x54 points at the shared no-op body 0x82AD5078.
void GameCallbackProcessor::ProcessMessage(RealmcCore::Message*)
{
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
