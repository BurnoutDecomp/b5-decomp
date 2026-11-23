#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h" // original diagnostic file and line
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (assert/log message build)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsEventQueue.h"    // CgsModule::EventQueue<T,N> source for the typed Append<EventT,SRCN>

#include <cstring>   // memcpy / memset / strlen

// Variable-size event queue. Unlike the fixed-stride EventQueue<T, N>
// (CgsEventQueue.h), each event here is a variable-length byte record packed into
// a single inline buffer (macData). Recovered from the X360 spine with member
// names/types from the DecFIGS DWARF (CgsVariableEventQueue.h) and the original
// LegacySrc partial source (GameShared/GameClasses/Module/CgsVariableEventQueue.h).
//
// BUFSIZE is the inline buffer size in bytes; ALIGN is the record alignment (16).
// The generic template bodies are inline here (the X360 emits each instantiation's
// methods out-of-line; callers compile against the generic and the per-instantiation
// thin .cpp files in this directory force the out-of-line emission).
//
// LAYOUT (X360 authoritative; the binary disagrees with the DWARF on the offset
// math, which is PS3/LegacySrc drift -- see Clear()/Construct() below):
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
    public:
        // ADDITIVE GROW (CgsGui::GuiModule's ctor @0x827E54B0, whose single-byte zero
        // store targets exactly this leading flag of the embedded load-notification
        // queue): put the queue in the not-yet-Constructed state.
        void MarkUnconstructed() { mbIsConstructed = false; }

    protected:
        // DecFIGS h:51; ARTIST ReplayModule827E0428 and GUI827E54B0
        // initialize only this marker before constructing later members.
        BaseVariableEventQueue() : mbIsConstructed(false) {}

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


        // DecFIGS CgsVariableEventQueue.h:686 (assert h:688): queue one typed event by
        // reference, `bool AddEvent<T>(const T& lEvent, int32_t liEventId)`; ARTIST decorates
        // every instance `QAA_NABV<T>...H` (e.g. 0x82252EB8 AddEvent<LoadGameDataEvent> on
        // VariableEventQueue<512,16>). Forwards to the three-arg AddEvent with sizeof(T).
        template <typename EventT>
        bool AddEvent(const EventT& lEvent, s32 liEventId);

        // ADDITIVE GROW (FLAG): typed-size convenience form of AddEventSafe -- the
        // non-asserting, overflow-guarded sibling of the typed AddEvent<EventT> above. The
        // X360 emits this as a distinct templated member of VariableEventQueue<BUFSIZE,ALIGN>
        // (e.g. ??$AddEventSafe@VCInEventDrawAxis@Internal@CgsDev@@...@?$VariableEventQueue@
        // $0EAAA@$0BA@@CgsModule@@ == VariableEventQueue<16384,16>::AddEventSafe<
        // CgsDev::Internal::CInEventDrawAxis> @ 0x82828868): it asserts the queue is
        // constructed (CgsVariableEventQueue.h:711 inline), then forwards to the three-arg
        // AddEventSafe with liSize == sizeof(EventT). Used by the CgsDev debug-draw publishers
        // (CgsDev::DebugRender::Draw*). DWARF declares only the three-arg AddEventSafe; this
        // definition lives in CgsVariableEventQueueSafe.inl with the native assert lock.
        template <typename EventT>
        bool AddEventSafe(const EventT& lrEvent, s32 liType);

        // ADDITIVE GROW (FLAG): bulk-append another (smaller) VariableEventQueue's packed
        // event bytes into this one. The X360 emits this as a distinct templated member of
        // VariableEventQueue<BUFSIZE,ALIGN> parameterised on the *source* queue's
        // <SRCBUF,SRCALIGN>: e.g. ??$Append@$0BAAA@$0BA@@?$VariableEventQueue@$0EIAA@$0BA@@
        // CgsModule@@ == VariableEventQueue<18432,16>::Append<4096,16>(const
        // VariableEventQueue<4096,16>&) @ 0x82504F68. It asserts this queue is constructed
        // (CgsVariableEventQueue.h:759), checks the destination would not overflow
        // (lrSource.GetSizeInBytes() + miBufferWritePos > BUFSIZE -> overflow), then memcpy's
        // the source's packed bytes (from GetFirstWritePointer(), GetSizeInBytes() bytes) to
        // &macData[miBufferWritePos], advances miBufferWritePos by the byte count and miLength
        // by the source's event count. Used by the GUI/resource OutputBuffer AddGuiOutEvents /
        // AddResourceRequests publishers. DWARF declares only the single-event AddEvent forms;
        // this bulk Append is X360-attested and added here additively.
        template <s32 SRCBUF, s32 SRCALIGN>
        bool Append(const VariableEventQueue<SRCBUF, SRCALIGN>& lrSource);

        // ADDITIVE GROW (FLAG): non-asserting, per-event safe variant of Append. Where Append
        // bulk-memcpy's the source's packed bytes (and asserts on overflow), AppendSafe walks
        // the source queue event-by-event (GetFirstEvent/GetNextEvent) and re-adds each through
        // this queue's AddEventSafe, returning false the moment an AddEventSafe would overflow
        // (no assert, no partial-record copy). The X360 emits it as a distinct templated member
        // parameterised on the source <SRCBUF,SRCALIGN>: AppendSafe<32768,16> @ 0x828529D8 and
        // AppendSafe<65536,16> @ 0x82852AE8 (dest VariableEventQueue<65536,16>). Asserts this
        // queue is constructed (CgsVariableEventQueue.h:805). DWARF declares only Append; this
        // safe form is X360-attested and added here additively.
        template <s32 SRCBUF, s32 SRCALIGN>
        bool AppendSafe(const VariableEventQueue<SRCBUF, SRCALIGN>& lrSource);

        // ADDITIVE GROW (FLAG): typed per-event append from a fixed-stride EventQueue<EventT,SRCN>.
        // Distinct from the VariableEventQueue-source Append above -- the X360 emits this as a
        // templated member parameterised on the *source element type and capacity*: e.g.
        // ??$Append@...HitOverheadSignEvent...@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@ ==
        // VariableEventQueue<1536,16>::Append<BrnGameState::GameStateModuleIO::HitOverheadSignEvent,
        // 100> @ 0x827AECF8, and the RecordPropHitEvent,50 sibling @ 0x827AEC10. It asserts this
        // queue is constructed (CgsVariableEventQueue.h:841), then walks the source queue
        // (0..GetLength()) and forwards each element to the three-arg AddEvent with the caller-
        // supplied liType and liSize == sizeof(EventT) (li r6,1 for the 1-byte HitOverheadSignEvent,
        // li r6,0x20 for the 32-byte RecordPropHitEvent). Used by WorldModule::
        // BridgeEntityModulesToOutput_PostPhysics to fold the per-frame typed game-event queues into
        // the packed GameState input buffer. DWARF declares only the single-event AddEvent forms;
        // this typed bulk Append is X360-attested and added here additively.
        template <typename EventT, s32 SRCN>
        bool Append(const EventQueue<EventT, SRCN>& lrSource, s32 liType);

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
        // ARTIST 0x82211348 computes the offset before setting the constructed flag,
        // then checks alignment using the signed address remainder.
        const s32 liDataAddress = reinterpret_cast<s32>(&macData[0]);
        const s32 liFirstEventOffset = ALIGN - (liDataAddress & (ALIGN - 1));
        mbIsConstructed = true;
        miFirstEventOffset = liFirstEventOffset;

        CGS_ASSERT(liFirstEventOffset >= 0, CgsDev::Assert::MessageAt(
            "miFirstEventOffset >= 0", detail::KAC_VEQ_FILE, 226));
        CGS_ASSERT(((reinterpret_cast<s32>(macData) + miFirstEventOffset) % ALIGN) == 0,
                   CgsDev::Assert::MessageAt(
                       "( ( (int32_t)&macData[0] + miFirstEventOffset ) % Alignment ) == 0", detail::KAC_VEQ_FILE, 227));

        Clear();
    }

    // -------- Clear @ X360 0x821FF890 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void VariableEventQueue<BUFSIZE, ALIGN>::Clear()
    {
        if (!mbIsConstructed)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 298);
            CgsDev::Assert::EndAssert();
        }

        miLength = 0;

        // "AlexM" self-check: recompute the first-event offset the "old style" way and
        // assert it matches the stored "new style" value computed in Construct().
        s32 liOldStyle = -(static_cast<s32>(reinterpret_cast<intptr_t>(&macData[0])) % ALIGN);
        while (liOldStyle < 0)
            liOldStyle += ALIGN;
        if (liOldStyle != miFirstEventOffset)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "AlexM screwed up his math: old style=" << liOldStyle
                       << ", new style=" << miFirstEventOffset;
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 310);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 245);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 263);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 281);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 348);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 728);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 367);
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
        CGS_ASSERT(mbIsConstructed, CgsDev::Assert::StreamMessageAt(
            "Not Constructed\n", detail::KAC_VEQ_FILE, 407));

        s32 liPrevEntryPosition =
            (s32)(reinterpret_cast<const char*>(lpEvent) - macData) - (s32)sizeof(CBufferEntry);
        if (!((liPrevEntryPosition >= 0) && (liPrevEntryPosition < miBufferWritePos)))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            CgsDev::StrStreamBase& lrMessage = lStrStream << "Prev entry is invalid, position: " << liPrevEntryPosition;
            lrMessage << " PrevEventPtr=0x";
            (lrMessage.SetPrintMode(CgsDev::E_PRINTMODE_HEX) << reinterpret_cast<u32>(lpEvent))
                .SetPrintMode(CgsDev::E_PRINTMODE_HEXONCE);
            lrMessage << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 410);
            CgsDev::Assert::EndAssert();
        }

        const CBufferEntry* lpPrevEntry =
            reinterpret_cast<const CBufferEntry*>(&macData[liPrevEntryPosition]);
        s32 liNextEntryPosition = liPrevEntryPosition + (s32)sizeof(CBufferEntry) + lpPrevEntry->miSize +
            GetEventPaddingSize(lpPrevEntry->miSize + (s32)sizeof(CBufferEntry));

        if (!(liNextEntryPosition >= 0))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Next entry position is invalid: " << liNextEntryPosition << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 419);
            CgsDev::Assert::EndAssert();
        }
        if (!(lpPrevEntry->miSize >= 0))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Next entry size is invalid: " << lpPrevEntry->miSize;
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 420);
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
        CGS_ASSERT(mbIsConstructed, CgsDev::Assert::StreamMessageAt(
            "Not Constructed\n", detail::KAC_VEQ_FILE, 454));

        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);

        // FAKEMATCH: Keep the native operand order in the capacity and write-offset sums.
        if (!((miBufferWritePos + liPaddingBytes + liTotalEventSize) <= BUFSIZE))
        {
            OutputQueueContents();
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "CgsModule::" __FUNCTION__
                       << "\nQueue overflow.\nWrite Pos=" << miBufferWritePos
                       << " Event Type=" << liType
                       << "\nTotalEventSize=" << liTotalEventSize
                       << " Padding=" << liPaddingBytes
                       << "\nBufferSize=" << (s32)BUFSIZE
                       << " Length=" << miLength << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 469);
            CgsDev::Assert::EndAssert();
        }

        CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
        lpEntry->miID   = liType;
        lpEntry->miSize = liSize;

        miBufferWritePos += (s32)sizeof(CBufferEntry);
        memcpy(&macData[miBufferWritePos], lpEvent, liSize);
        miBufferWritePos = miBufferWritePos + liPaddingBytes + liSize;
        ++miLength;

        return true;
    }

    // -------- AddEvent<EventT> (typed) --------
    // DecFIGS CgsVariableEventQueue.h:686; X360 e.g. 0x82252EB8 (<512,16>, LoadGameDataEvent),
    // 0x82296320 (<4096,16>, LoadGameDataEvent), 0x8273FB48 (<4096,16>, GetVehicleListRequest):
    // the "Not Constructed" assert (h:688), then the three-arg AddEvent with sizeof(EventT).
    template <s32 BUFSIZE, s32 ALIGN>
    template <typename EventT>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AddEvent(const EventT& lEvent, s32 liEventId)
    {
        CGS_ASSERT(mbIsConstructed, CgsDev::Assert::StreamMessageAt(
            "Not Constructed\n", detail::KAC_VEQ_FILE, 688));

        // DWARF derives every record from CgsModule::Event and ARTIST converts implicitly (for
        // CgsSound::Io::Message<T>, whose Event base follows MessageHeader's vptr, 0x826DDF28 adds
        // 4 with a null check). The tree's GameEvent<N>, SceneManagerIO line-test records and
        // Message<T> do not derive from Event yet, so the record pointer is converted explicitly.
        return AddEvent(reinterpret_cast<const Event*>(&lEvent), liEventId, (s32)sizeof(EventT));
    }

    // -------- Append<SRCBUF,SRCALIGN> @ X360 0x82504F68 (dest <18432,16>, src <4096,16>) --------
    template <s32 BUFSIZE, s32 ALIGN>
    template <s32 SRCBUF, s32 SRCALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::Append(const VariableEventQueue<SRCBUF, SRCALIGN>& lrSource)
    {
        if (!mbIsConstructed)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 759);
            CgsDev::Assert::EndAssert();
        }

        s32 liSizeInBytes = lrSource.GetSizeInBytes();
        if (liSizeInBytes + miBufferWritePos > BUFSIZE)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "ERROR: Overflowed variable event queue when appending another one\n";
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "  Our contents:\n";
            OutputQueueContents();
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "\n  Other queue contents:\n";
            lrSource.OutputQueueContents();
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("\n\nVariable event queue overflowed", detail::KAC_VEQ_FILE, 773);
            CgsDev::Assert::EndAssert();
        }

        const char* lpSrc = lrSource.GetFirstWritePointer();
        XMemCpy(&macData[miBufferWritePos], lpSrc, liSizeInBytes);
        miBufferWritePos = liSizeInBytes + miBufferWritePos;
        miLength += lrSource.GetLength();
        return true;
    }

    // -------- Append<EventT,SRCN> @ X360 0x827AECF8 (dest <1536,16>, src EventQueue<HitOverheadSignEvent,100>) --------
    // Typed per-event append from a fixed-stride EventQueue<EventT,SRCN>: forward every live source
    // event (0..GetLength()) to the three-arg AddEvent with the caller-supplied liType and
    // liSize == sizeof(EventT). No overflow pre-check -- AddEvent asserts on overflow itself.
    template <s32 BUFSIZE, s32 ALIGN>
    template <typename EventT, s32 SRCN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::Append(const EventQueue<EventT, SRCN>& lrSource, s32 liType)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 841);
            CgsDev::Assert::EndAssert();
        }

        for (s32 liIndex = 0; liIndex < lrSource.GetLength(); ++liIndex)
        {
            const EventT& lrEvent = lrSource.GetEvent(liIndex);
            AddEvent(reinterpret_cast<const Event*>(&lrEvent), liType, (s32)sizeof(EventT));
        }
        return true;
    }

    // -------- AppendSafe<SRCBUF,SRCALIGN> @ X360 0x828529D8 (dest <65536,16>, src <32768,16>) --------
    // Per-event safe append: walk the source queue with GetFirstEvent/GetNextEvent and re-add
    // each event through this queue's AddEventSafe. Returns false the instant an AddEventSafe
    // overflows (no assert, no partial copy); returns true once every source event is added.
    template <s32 BUFSIZE, s32 ALIGN>
    template <s32 SRCBUF, s32 SRCALIGN>
    bool VariableEventQueue<BUFSIZE, ALIGN>::AppendSafe(const VariableEventQueue<SRCBUF, SRCALIGN>& lrSource)
    {
        if (!mbIsConstructed)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), detail::KAC_VEQ_FILE, 805);
            CgsDev::Assert::EndAssert();
        }

        const Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lrSource.GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (!AddEventSafe(lpEvent, liType, liSize))
                return false;
            liType = lrSource.GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        return true;
    }

    // -------- AllocateEvent @ X360 0x82652420 --------
    template <s32 BUFSIZE, s32 ALIGN>
    void* VariableEventQueue<BUFSIZE, ALIGN>::AllocateEvent(s32 liType, s32 liSize)
    {
        CGS_ASSERT(mbIsConstructed, CgsDev::Assert::StreamMessageAt(
            "Not Constructed\n", detail::KAC_VEQ_FILE, 556));

        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);

        if (!((miBufferWritePos + liPaddingBytes + liTotalEventSize) <= BUFSIZE))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "CgsModule::" __FUNCTION__
                       << "\nQueue overflow\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 563);
            CgsDev::Assert::EndAssert();
        }

        CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
        lpEntry->miID   = liType;
        lpEntry->miSize = liSize;

        miBufferWritePos += (s32)sizeof(CBufferEntry);
        void* lpResult = &macData[miBufferWritePos];
        miBufferWritePos = miBufferWritePos + liPaddingBytes + liSize;
        ++miLength;

        return lpResult;
    }

    // -------- GetSizeInBytes @ X360 0x823B07D8 --------
    template <s32 BUFSIZE, s32 ALIGN>
    s32 VariableEventQueue<BUFSIZE, ALIGN>::GetSizeInBytes() const
    {
        if (!mbIsConstructed)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 983);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 1000);
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
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStrStream << "Not Constructed\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, detail::KAC_VEQ_FILE, 1000);
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
        s32 liSize = 0;
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

        const Event* lpEvent;
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

        for (s32 liIndex = 0; liIndex < liNumTypes; ++liIndex)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
                lrStream << "ERROR: Event Type ";
                lrStream.PrintSignedInteger(laiTypes[liIndex]);
                lrStream << " has ";
                const s32 liCount = laiCounts[liIndex];
                lrStream.PrintSignedInteger(liCount);
                lrStream << " entries in the queue and each event's size is ";
                const s32 liEventSize = laiSizes[liIndex];
                lrStream.PrintSignedInteger(liEventSize);
                lrStream << " (total mem usage is ";
                lrStream.PrintSignedInteger(liCount * liEventSize);
                lrStream << ") \n";
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
        CGS_ASSERT(mbIsConstructed, CgsDev::Assert::StreamMessageAt(
            "Not Constructed\n", detail::KAC_VEQ_FILE, 508));
        s32 liTotalEventSize = liSize + (s32)sizeof(CBufferEntry);
        s32 liPaddingBytes = GetEventPaddingSize(liTotalEventSize);
        if ((miBufferWritePos + liPaddingBytes + liTotalEventSize) <= BUFSIZE)
        {
            CBufferEntry* lpEntry = reinterpret_cast<CBufferEntry*>(&macData[miBufferWritePos]);
            lpEntry->miID   = liType;
            lpEntry->miSize = liSize;
            miBufferWritePos += (s32)sizeof(CBufferEntry);
            memcpy(&macData[miBufferWritePos], lpEvent, liSize);
            miBufferWritePos = miBufferWritePos + liPaddingBytes + liSize;
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

    template <>
    bool VariableEventQueue<16384, 16>::AddStringEventSafe(const char* lpacString, s32 liType);
}
