#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CgsDev::Assert::Begin/Fire/EndAssert (verbatim X360 file/line)
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (assert/log message build)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

#include <cstring>   // memcpy / memset / strlen

// Variable-size event queue. Unlike the fixed-stride EventQueue<T, N>
// (CgsEventQueue.h), each event here is a variable-length byte record packed into
// a single inline buffer (macData). Recovered from the X360 spine with member
// names/types from the DecFIGS DWARF (CgsVariableEventQueue.h) and the original
// Feb-2007 partial source (GameShared/GameClasses/Module/CgsVariableEventQueue.h).
//
// BUFSIZE is the inline buffer size in bytes; ALIGN is the record alignment (16).
// The generic template bodies are inline here (the X360 emits each instantiation's
// methods out-of-line; callers compile against the generic and the per-instantiation
// thin .cpp files in this directory force the out-of-line emission).
//
// LAYOUT (X360 authoritative; the binary disagrees with the DWARF on the offset
// math, which is PS3/Feb-2007 drift -- see Clear()/Construct() below):
//   +0      bool mbIsConstructed        (BaseVariableEventQueue)
//   +1      char macData[BUFSIZE]       (BYTE-aligned -- NO alignas; X360 Construct
//                                        computes miFirstEventOffset precisely because
//                                        &macData[0] is only byte-aligned at offset 1)
//   +BUFSIZE+4   s32 miBufferWritePos
//   +BUFSIZE+8   s32 miLength
//   +BUFSIZE+12  s32 miFirstEventOffset
// Each record is a 16-byte CBufferEntry header { miID@+0, miSize@+4, padTo16 } followed
// by the miSize-byte payload, then GetEventPaddingSize() bytes of alignment padding.
namespace CgsModule
{
    // Empty base for every queued event. Concrete events (e.g. CgsGui::GuiEvent<N>)
    // derive from this; the queue stores them by their byte image.
    struct Event {};

    // CgsVariableEventQueue.h:25 (DWARF) -- histogram cap in OutputQueueContents.
    const s32 KI_MAX_EVENT_TYPES_IN_QUEUE = 100;

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

        void OutputQueueContents() const;

        char* GetFirstWritePointer();
        const char* GetFirstWritePointer() const;
        s32 GetSizeInBytes() const;

    protected:
        s32 GetEventPaddingSize(s32 liTotalEventSize) const;

        // Per-record header. sizeof(CBufferEntry) == ALIGN == 16 (the trailing
        // muPadTo16Bytes rounds the header to a full alignment unit). DWARF
        // CgsVariableEventQueue.h:183-187.
        class CBufferEntry
        {
        public:
            s32 miID;
            s32 miSize;
            u64 muPadTo16Bytes;
        };

        char macData[BUFSIZE];        // NO alignas: byte-aligned at struct offset 1 (X360-proven)
        s32  miBufferWritePos;
        s32  miLength;
        s32  miFirstEventOffset;
    };

    // The verbatim X360-baked source path for this header's asserts (preserved exactly).
    namespace detail
    {
        const char* const KAC_VEQ_FILE =
            "..\\..\\..\\GameShared\\GameClasses\\Module/CgsVariableEventQueue.h";
    }

    // -------- Construct @ X360 0x82211348 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void VariableEventQueue<BUFSIZE, ALIGN>::Construct()
    {
        // X360: miFirstEventOffset = ALIGN - (&macData[0] & 0xF). The Feb-2007/DWARF
        // source subtracted sizeof(CBufferEntry) and looped (+= ALIGN) while negative;
        // the X360 build collapsed that to the single ALIGN - rem form (binary wins).
        mbIsConstructed = true;
        const uintptr_t luDataAddr = reinterpret_cast<uintptr_t>(&macData[0]);
        miFirstEventOffset = ALIGN - (s32)(luDataAddr & (ALIGN - 1));

        CGS_ASSERT(miFirstEventOffset >= 0, "miFirstEventOffset >= 0");
        CGS_ASSERT(((luDataAddr + (uintptr_t)miFirstEventOffset) % ALIGN) == 0,
                   "( ( (int32_t)&macData[0] + miFirstEventOffset ) % Alignment ) == 0");

        Clear();
    }

    // -------- Clear @ X360 0x821FF890 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void VariableEventQueue<BUFSIZE, ALIGN>::Clear()
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 298);
            CgsDev::Assert::EndAssert();
        }

        miLength = 0;

        // "AlexM" self-check: recompute the first-event offset the "old style" way and
        // assert it matches the stored "new style" value computed in Construct().
        s32 liAlignRem = (s32)(reinterpret_cast<uintptr_t>(&macData[0]) & (ALIGN - 1));
        s32 liOldStyle = -liAlignRem;
        if (liAlignRem > 0)
            liOldStyle = ALIGN * (((liAlignRem - 1) >> 4) + 1) - liAlignRem;
        if (liOldStyle != miFirstEventOffset)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "AlexM screwed up his math: old style=" << liOldStyle
                       << ", new style=" << miFirstEventOffset;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 310);
            CgsDev::Assert::EndAssert();
        }

        miBufferWritePos = miFirstEventOffset;
    }

    // -------- Prepare @ X360 0x8258C2A0 --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::Prepare()
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 245);
            CgsDev::Assert::EndAssert();
        }
        Clear();
        return true;
    }

    // -------- Release @ X360 0x8258C348 --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::Release()
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 263);
            CgsDev::Assert::EndAssert();
        }
        Clear();
        return true;
    }

    // -------- Destruct @ X360 0x823683E0 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void VariableEventQueue<BUFSIZE, ALIGN>::Destruct()
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 281);
            CgsDev::Assert::EndAssert();
        }
        Clear();
    }

    // -------- GetMaxLength (DWARF 330; no X360 ledger addr -- inline, no assert) --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetMaxLength() const
    {
        return BUFSIZE;
    }

    // -------- GetLength @ X360 0x8231B428 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetLength() const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 348);
            CgsDev::Assert::EndAssert();
        }
        return miLength;
    }

    // -------- GetEventPaddingSize @ X360 0x821FFA08 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetEventPaddingSize(s32 liTotalEventSize) const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 728);
            CgsDev::Assert::EndAssert();
        }
        s32 liRem = liTotalEventSize & (ALIGN - 1);
        s32 liPaddingBytes = 0;
        if (liRem != 0)
            liPaddingBytes = ALIGN - liRem;
        return liPaddingBytes;
    }

    // -------- GetFirstEvent @ X360 0x821FC0A0 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetFirstEvent(const Event** lppEvent, s32* lpiSize) const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 367);
            CgsDev::Assert::EndAssert();
        }

        if (miLength > 0)
        {
            const CBufferEntry* lpEntry =
                reinterpret_cast<const CBufferEntry*>(&macData[miFirstEventOffset]);
            *lppEvent = reinterpret_cast<const Event*>(&macData[miFirstEventOffset + (s32)sizeof(CBufferEntry)]);
            *lpiSize  = lpEntry->miSize;
            return lpEntry->miID;
        }

        *lppEvent = 0;
        *lpiSize  = 0;
        return -1;
    }

    // -------- GetNextEvent @ X360 0x82211400 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetNextEvent(
        const Event* lpEvent, const Event** lppNextEvent, s32* lpiSize) const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 407);
            CgsDev::Assert::EndAssert();
        }

        s32 liPrevEntryPosition =
            (s32)(reinterpret_cast<const char*>(lpEvent) - macData) - (s32)sizeof(CBufferEntry);
        if (!((liPrevEntryPosition >= 0) && (liPrevEntryPosition < miBufferWritePos)))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Prev entry is invalid, position: " << liPrevEntryPosition
                       << " PrevEventPtr=0x" << CgsDev::E_PRINTMODE_HEX
                       << reinterpret_cast<void*>(const_cast<Event*>(lpEvent)) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 410);
            CgsDev::Assert::EndAssert();
        }

        const CBufferEntry* lpPrevEntry =
            reinterpret_cast<const CBufferEntry*>(&macData[liPrevEntryPosition]);
        s32 liNextEntryPosition = liPrevEntryPosition + (s32)sizeof(CBufferEntry) + lpPrevEntry->miSize +
            GetEventPaddingSize(lpPrevEntry->miSize + (s32)sizeof(CBufferEntry));

        if (!(liNextEntryPosition >= 0))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Next entry position is invalid: " << liNextEntryPosition << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 419);
            CgsDev::Assert::EndAssert();
        }
        if (!(lpPrevEntry->miSize >= 0))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Next entry size is invalid: " << lpPrevEntry->miSize;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 420);
            CgsDev::Assert::EndAssert();
        }

        if (liNextEntryPosition < miBufferWritePos)
        {
            const CBufferEntry* lpNextEntry =
                reinterpret_cast<const CBufferEntry*>(&macData[liNextEntryPosition]);
            *lppNextEvent = reinterpret_cast<const Event*>(&macData[liNextEntryPosition + (s32)sizeof(CBufferEntry)]);
            *lpiSize      = lpNextEntry->miSize;
            return lpNextEntry->miID;
        }

        *lppNextEvent = 0;
        *lpiSize      = 0;
        return -1;
    }

    // -------- AddEvent @ X360 0x8233FAE8 --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddEvent(const Event* lpEvent, s32 liType, s32 liSize)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 454);
            CgsDev::Assert::EndAssert();
        }

        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);

        if (!((miBufferWritePos + liTotalEventSize + liPaddingBytes) <= BUFSIZE))
        {
            OutputQueueContents();
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "CgsModule::VariableEventQueue<" << (s32)BUFSIZE << "," << (s32)ALIGN << ">::AddEvent"
                       << "\nQueue overflow.\nWrite Pos=" << miBufferWritePos
                       << " Event Type=" << liType
                       << "\nTotalEventSize=" << liTotalEventSize
                       << " Padding=" << liPaddingBytes
                       << "\nBufferSize=" << (s32)BUFSIZE
                       << " Length=" << miLength << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 469);
            CgsDev::Assert::EndAssert();
        }

        CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
        lpEntry->miID   = liType;
        lpEntry->miSize = liSize;

        miBufferWritePos += (s32)sizeof(CBufferEntry);
        memcpy(&macData[miBufferWritePos], lpEvent, liSize);
        miBufferWritePos += liSize + liPaddingBytes;
        ++miLength;

        return true;
    }

    // -------- AllocateEvent @ X360 0x82652420 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void* VariableEventQueue<BUFSIZE, ALIGN>::AllocateEvent(s32 liType, s32 liSize)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 556);
            CgsDev::Assert::EndAssert();
        }

        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);

        if (!((miBufferWritePos + liTotalEventSize + liPaddingBytes) <= BUFSIZE))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "CgsModule::VariableEventQueue<" << (s32)BUFSIZE << "," << (s32)ALIGN << ">::AllocateEvent"
                       << "\nQueue overflow\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 563);
            CgsDev::Assert::EndAssert();
        }

        CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
        lpEntry->miID   = liType;
        lpEntry->miSize = liSize;

        miBufferWritePos += (s32)sizeof(CBufferEntry);
        void* lpResult = &macData[miBufferWritePos];
        miBufferWritePos += liSize + liPaddingBytes;
        ++miLength;

        return lpResult;
    }

    // -------- GetSizeInBytes @ X360 0x823B07D8 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetSizeInBytes() const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 983);
            CgsDev::Assert::EndAssert();
        }
        return miBufferWritePos - miFirstEventOffset;
    }

    // -------- GetFirstWritePointer @ X360 0x823B0730 (non-const) --------
    template <s32 BUFSIZE, s32 ALIGN>
    char* VariableEventQueue<BUFSIZE, ALIGN>::GetFirstWritePointer()
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 1000);
            CgsDev::Assert::EndAssert();
        }
        return &macData[miFirstEventOffset];
    }

    // const overload (DWARF 998): same body, returns const char*.
    template <s32 BUFSIZE, s32 ALIGN>
    const char* VariableEventQueue<BUFSIZE, ALIGN>::GetFirstWritePointer() const
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 1000);
            CgsDev::Assert::EndAssert();
        }
        return &macData[miFirstEventOffset];
    }

    // -------- OutputQueueContents @ X360 0x82336B10 --------
    // Debug-only histogram of the queued event types. Walks the queue via
    // GetFirstEvent/GetNextEvent, counts unique event types (cap
    // KI_MAX_EVENT_TYPES_IN_QUEUE) and the per-type sizes, then logs a summary line per
    // type through CgsDev::Log::gpDebugPrint, gated on the message filter.
    template <s32 BUFSIZE, s32 ALIGN>
    void VariableEventQueue<BUFSIZE, ALIGN>::OutputQueueContents() const
    {
        s32 liNumTypes = 0;
        s32 laiTypes[KI_MAX_EVENT_TYPES_IN_QUEUE];
        s32 laiCounts[KI_MAX_EVENT_TYPES_IN_QUEUE];
        s32 laiSizes[KI_MAX_EVENT_TYPES_IN_QUEUE];

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "ERROR: Queue full. It has " << GetLength() << " entries\n";
        }

        memset(laiCounts, 0, sizeof(laiCounts));
        memset(laiTypes, -1, sizeof(laiTypes));
        memset(laiSizes, 0, sizeof(laiSizes));

        const Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = GetFirstEvent(&lpEvent, &liSize);
        while (liType >= 0)
        {
            s32 liSlot = 0;
            while (liSlot < liNumTypes && laiTypes[liSlot] != liType)
                ++liSlot;

            if (liSlot < KI_MAX_EVENT_TYPES_IN_QUEUE)
            {
                ++laiCounts[liSlot];
                if (liSlot == liNumTypes)
                {
                    laiTypes[liNumTypes] = liType;
                    laiSizes[liNumTypes] = liSize;
                    ++liNumTypes;
                }
            }

            liType = GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            for (s32 liIndex = 0; liIndex < liNumTypes; ++liIndex)
            {
                *CgsDev::Log::gpDebugPrint
                    << "ERROR: Event Type " << laiTypes[liIndex]
                    << " has " << laiCounts[liIndex]
                    << " entries in the queue and each event's size is " << laiSizes[liIndex]
                    << " (total mem usage is " << (laiSizes[liIndex] * laiCounts[liIndex]) << ") \n";
            }
        }
    }

    // -------- AddStringEvent (DWARF 646) -- thin wrapper over AddEvent --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddStringEvent(const char* lpacString, s32 liType)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 647);
            CgsDev::Assert::EndAssert();
        }
        return AddEvent(reinterpret_cast<const Event*>(lpacString), liType, (s32)strlen(lpacString));
    }

    // -------- AddEventSafe (DWARF 506) -- non-asserting overflow-guarded AddEvent --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddEventSafe(const Event* lpEvent, s32 liType, s32 liSize)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 467);
            CgsDev::Assert::EndAssert();
        }
        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);
        if ((miBufferWritePos + liTotalEventSize + liPaddingBytes) <= BUFSIZE)
        {
            CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
            lpEntry->miID   = liType;
            lpEntry->miSize = liSize;
            miBufferWritePos += (s32)sizeof(CBufferEntry);
            memcpy(&macData[miBufferWritePos], lpEvent, liSize);
            miBufferWritePos += liSize + liPaddingBytes;
            ++miLength;
            return true;
        }
        return false;
    }

    // -------- AllocateEventSafe (DWARF 598) -- non-asserting overflow-guarded AllocateEvent --------
    template <s32 BUFSIZE, s32 ALIGN>
    void* VariableEventQueue<BUFSIZE, ALIGN>::AllocateEventSafe(s32 liType, s32 liSize)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 559);
            CgsDev::Assert::EndAssert();
        }
        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);
        if ((miBufferWritePos + liTotalEventSize + liPaddingBytes) <= BUFSIZE)
        {
            CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
            lpEntry->miID   = liType;
            lpEntry->miSize = liSize;
            miBufferWritePos += (s32)sizeof(CBufferEntry);
            void* lpResult = &macData[miBufferWritePos];
            miBufferWritePos += liSize + liPaddingBytes;
            ++miLength;
            return lpResult;
        }
        return 0;
    }

    // -------- AddStringEventSafe (DWARF 663) --------
    template <s32 BUFSIZE, s32 ALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddStringEventSafe(const char* lpacString, s32 liType)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 624);
            CgsDev::Assert::EndAssert();
        }
        return AddEventSafe(reinterpret_cast<const Event*>(lpacString), liType, (s32)strlen(lpacString) + 1);
    }
}
