#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStreamBase::operator<<

// CgsTestBedAllocator.cpp:10/11 - the optional "tag the next allocation" debug string + whether to
// clear it after a single use. Defaults: no pending string, clear-after-use.
const char* gpcDebugAllocString          = 0;
bool        gbRemoveDebugStringAfterUse  = true;

namespace CgsSound
{
namespace TestBed
{
    // -------------------------------------------------------------------------------------------
    // History::Entry / History - the recent-events ring (zeroed at construction; the X360 inlines
    // this zeroing into the Allocator ctor's 500-slot loop).
    // -------------------------------------------------------------------------------------------
    Allocator::History::Entry::Entry()
        : mpcName(0)
        , mpStartMem(0)
        , mpEndMem(0)
        , muSize(0)
    {
    }

    Allocator::History::Entry::Entry(const char* lpcName, u8* lpStartMem, size_t luSize)
        : mpcName(lpcName)
        , mpStartMem(lpStartMem)
        , mpEndMem(lpStartMem + luSize)
        , muSize(luSize)
    {
    }

    Allocator::History::History()
        : muCurrentPosition(0)
    {
        // maHistory's Entry() default ctor zeroes each slot (the X360 ctor's `do{...}while` loop).
    }

    // -------------------------------------------------------------------------------------------
    // Allocator ctor @ 0x826ADE08 - set the backing allocator + debug name, clear all bookkeeping
    // state, and arm thread-safety. The vtable + the History ring init are implicit on PC.
    // -------------------------------------------------------------------------------------------
    Allocator::Allocator(const char* lpcName, rw::IResourceAllocator* lpAllocator)
        : mpAllocator(lpAllocator)            // stw r5, 4(this)
        , mpFirstHeader(0)                    // stw r30(0), 8(this)
        , mbSanityCheck(false)                // stb r30(0), 0xC(this)
        , mbVerbose(false)                    // stb r30(0), 0xD(this)
        , mbAllocEntered(false)               // stb r30(0), 0xE(this)
        , mbFreeEntered(false)                // stb r30(0), 0xF(this)
        , mbEnableThreadSafety(true)          // stb r10(1), 0x10(this)
        , mbTestRwac(false)                   // stb r30(0), 0x11(this)
        , mpvBadAlloc(0)                      // stw r30(0), 0x14(this)
        , mpvBadFree(0)                       // stw r30(0), 0x18(this)
        , mpcName(lpcName)                    // stw r11, 0x1C(this)
        , muBytesAllocated(0)                 // stw r30(0), 0x20(this)
        , mMutex(0, true)                     // EA::Thread::Mutex::Mutex(this+0x28, NULL, true)
    {
        // The X360 ctor defaults a null name to "Unknown" (lwz 0x1C; cmplwi 0; stw "Unknown").
        if (!mpcName)
            mpcName = "Unknown";
    }

    // -------------------------------------------------------------------------------------------
    // dtor @ 0x82682690 ('vector deleting destructor' restores the base vtable, runs ~Mutex, then
    // optionally operator-deletes). The member mMutex destructs automatically; the deleting thunk
    // and the operator delete are compiler-generated on PC.
    // -------------------------------------------------------------------------------------------
    Allocator::~Allocator()
    {
    }

    // -------------------------------------------------------------------------------------------
    // SetAllocator @ 0x826825F0 - install the backing allocator. Asserts (non-fatally) if one is
    // already set to a different allocator, then stores the new one regardless.
    // -------------------------------------------------------------------------------------------
    void Allocator::SetAllocator(rw::IResourceAllocator* lpAllocator)
    {
        CGS_ASSERT(!mpAllocator || lpAllocator == mpAllocator,
                   "Testbed::Allocator::SetAllocator(): mpAllocator is already set\n");
        mpAllocator = lpAllocator;
    }

    // -------------------------------------------------------------------------------------------
    // SanityCheck @ 0x826ADED8 - walk the live-allocation list, validating each block's guards.
    // Detects a corrupt/cyclic list (a node whose next loops back to the head) and asserts.
    // -------------------------------------------------------------------------------------------
    void Allocator::SanityCheck()
    {
        Header* lpHeader = mpFirstHeader;
        while (lpHeader)
        {
            lpHeader->SanityCheck(mHistory, mpcName);
            lpHeader = lpHeader->mpNext;
            if (mpFirstHeader == lpHeader)
            {
                CGS_ASSERT(false,
                           "Cycle in allocation list. Makes no bloody sense whatsoever, but there it is...\n");
            }
        }
    }

    // -------------------------------------------------------------------------------------------
    // SafeDump @ 0x826ADFA8 - log every live allocation (in list order), bracketed by a header line
    // and a total-bytes/total-count summary. The logging is gated on the message filter; the same
    // cyclic-list guard as SanityCheck fires if the list is corrupt.
    // -------------------------------------------------------------------------------------------
    void Allocator::SafeDump()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            const char* lpcName = mpcName;
            if (!lpcName)
                lpcName = "<NULLSTRING>";
            *CgsDev::Log::gpDebugPrint << "Dumping all allocations [" << lpcName << "]:\n";
        }

        Header* lpHeader   = mpFirstHeader;
        s32     liNumBlocks = 0;
        while (lpHeader)
        {
            lpHeader->Dump(mHistory);
            lpHeader = lpHeader->mpNext;
            ++liNumBlocks;
            if (mpFirstHeader == lpHeader)
            {
                CGS_ASSERT(false,
                           "Cycle in allocation list. Makes no bloody sense whatsoever, but there it is...\n");
            }
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Total of "
                                       << (u64)muBytesAllocated
                                       << " bytes of user memory allocated in "
                                       << liNumBlocks
                                       << " allocations.\n";
        }
    }
}
}
