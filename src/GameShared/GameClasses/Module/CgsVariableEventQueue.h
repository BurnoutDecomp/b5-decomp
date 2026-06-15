#pragma once

#include "types.hpp"

// Variable-size event queue. Unlike the fixed-stride EventQueue<T, N>
// (CgsEventQueue.h), each event here is a variable-length byte record packed into
// a single inline buffer (macData). Recovered from the X360 spine with member
// names/types and the method set from the DecFIGS DWARF (CgsVariableEventQueue.h).
//
// BUFSIZE is the inline buffer size in bytes; ALIGN is the record alignment.
// The member functions are declared but not defined here: each instantiation's
// bodies are their own ledger TUs (the X360 emits them out-of-line), so callers
// compile against these declarations and resolve the bodies at link time.
namespace CgsModule
{
    // Empty base for every queued event. Concrete events (e.g. CgsGui::GuiEvent<N>)
    // derive from this; the queue stores them by their byte image.
    struct Event {};

    class BaseVariableEventQueue
    {
    protected:
        bool mbIsConstructed;
    };

    template <s32 BUFSIZE, s32 ALIGN>
    class VariableEventQueue : public BaseVariableEventQueue
    {
    public:
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void Clear();

        s32 GetMaxLength() const;
        s32 GetLength() const;

        s32 GetFirstEvent(const Event** lppEvent, s32* lpiSize) const;
        s32 GetNextEvent(const Event* lpEvent, const Event** lppNextEvent, s32* lpiSize) const;

        bool AddEvent(const Event* lpEvent, s32 liType, s32 liSize);
        bool AddEventSafe(const Event* lpEvent, s32 liType, s32 liSize);

        void* AllocateEvent(s32 liType, s32 liSize);
        void* AllocateEventSafe(s32 liType, s32 liSize);

        bool AddStringEvent(const char* lpacString, s32 liType);
        bool AddStringEventSafe(const char* lpacString, s32 liType);

        char* GetFirstWritePointer();
        const char* GetFirstWritePointer() const;
        s32 GetSizeInBytes() const;

    protected:
        s32 GetEventPaddingSize(s32 liSize) const;

        alignas(ALIGN) char macData[BUFSIZE];
        s32 miBufferWritePos;
        s32 miLength;
        s32 miFirstEventOffset;
    };
}
