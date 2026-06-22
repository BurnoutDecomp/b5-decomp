// Tiny embed check: proves event.h is self-contained and EA::Jobs::Event plus the
// BucketListNode<Event,10>/<Event,16> instantiations are usable by name with the
// proven X360 layouts/signatures. Compile-only; no linkage.

#include "SDKs/EATech/eajobs/event.h"

namespace
{
    void EaJobsEventEmbedCallback(void* /*pContext*/) {}

    void EaJobsEventEmbedCheck()
    {
        using namespace EA::Jobs;

        // Event layout: type @0, value/loc/callback union @4/@8, decrementer @0xC.
        u32 luValue = 0;
        u32 luDecrementer = 1;
        Event lWriteEvent(Event::EVENT_TYPE_WRITE, 0x1234u, &luValue, &luDecrementer);
        lWriteEvent.Run();
        (void)lWriteEvent.mType;
        (void)lWriteEvent.mWriteValue;
        (void)lWriteEvent.mWriteLocation;
        (void)lWriteEvent.mDecrementerLocation;

        Event lCallbackEvent(
            Event::EVENT_TYPE_CALLBACK, EaJobsEventEmbedCallback, (void*)0, (u32*)0);
        (void)lCallbackEvent.mCallback;
        (void)lCallbackEvent.mContext;
        lCallbackEvent.Run();

        // The When enumerators are reachable by name.
        Event::When leWhen = Event::EVENT_WHEN_JOB_END;
        (void)leWhen;

        // Both BucketListNode instantiations: Add an event and read it back.
        Event lElement;
        lElement.mType = Event::EVENT_TYPE_NONE;
        lElement.mWriteValue = 0;
        lElement.mWriteLocation = 0;
        lElement.mDecrementerLocation = 0;

        Detail::BucketListNode<Event, 10> lEvents10;
        lEvents10.Add(lElement);
        (void)lEvents10.ListSize();
        (void)lEvents10[0].mType;
        (void)lEvents10.mNext;
        (void)lEvents10.mSize;

        Detail::BucketListNode<Event, 16> lEvents16;
        lEvents16.Add(lElement);
        lEvents16.Clear();
        (void)lEvents16.ListSize();
        (void)lEvents16[0].mType;
    }
}
