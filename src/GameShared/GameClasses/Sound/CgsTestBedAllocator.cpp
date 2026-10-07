#include "GameShared/GameClasses/Sound/CgsTestBedAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStreamBase::operator<<
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"  // GetDefaultRwacSystem
#include "rw/audio/core/PlugIn.h"                                    // rw::audio::core::System::IsLocked

#include <new>

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

    // -------------------------------------------------------------------------------------------
    // Header::Dump @ 0x826961E8 - sanity-check this one block, then (gated on the message filter)
    // log one line: "  <blockAddr>: <size> bytes allocated.\"<name>\" \n". The block base address is
    // the first word of mResource (Header+0x04), streamed as a pointer (rendered 0x%X - the X360
    // manually forces the stream's PrintMode to HEX around the u32 stream and restores it); muSize
    // (Header+0x1C) is streamed as a decimal u32 (X360 StrStreamBase::operator<<(u32), "%u"); a null
    // name renders as "<NULLSTRING>". Called per live block by SafeDump / SortedDump.
    // -------------------------------------------------------------------------------------------
    void Allocator::Header::Dump(History& lrHistory)
    {
        // 0x826961FC - Header::SanityCheck(lrHistory, mpcName): r4 = History&, r5 = mpcName@0x18.
        SanityCheck(lrHistory, mpcName);

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            const char* lpcName = mpcName;          // lwz 0x18(header)
            if (!lpcName)                           // cmplwi 0 -> beq
                lpcName = "<NULLSTRING>";
            *CgsDev::Log::gpDebugPrint << "  "
                                       << mResource.m_baseResources[0]   // lwz 4(header) - block base ptr, hex
                                       << ": "
                                       << (u32)muSize                    // lwz 0x1C(header) - decimal u32
                                       << " bytes allocated."
                                       << "\""
                                       << lpcName
                                       << "\" "
                                       << "\n";
        }
    }

    // FLAG PC-platform: the console judges a debug pointer plausible when its top address bit is
    // set (every heap and image address lives in the upper half of its 32-bit space). The host
    // address space has no such split, so any non-null pointer is plausible here.
    static bool IsPlausiblePointerPC(const void* lpAddress)
    {
        return lpAddress != 0;
    }

    // -------------------------------------------------------------------------------------------
    // Header::SanityCheck - validate this block's start guard, list link and end guard. On a
    // trample, log the block (client address, allocator name, block name, size), dump every
    // recent allocation that bracketed the client address, and assert.
    // -------------------------------------------------------------------------------------------
    void Allocator::Header::SanityCheck(History& lrHistory, const char* lpcAllocatorName)
    {
        if (mStartGuard.IsValid() && (mpNext == 0 || IsPlausiblePointerPC(mpNext)))
        {
            if (GetEndGuard().IsValid())
                return;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "ERROR END GUARD TRAMPLE:";
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "ERROR START GUEARD TRAMPLE:";
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "<" << GetClientAddress() << ">";

        if (IsPlausiblePointerPC(lpcAllocatorName))
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "[" << lpcAllocatorName << "]";
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[" << static_cast<void*>(const_cast<char*>(lpcAllocatorName)) << "]";
        }

        if (IsPlausiblePointerPC(mpcName))
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "\"" << mpcName << "\" ";
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Name [BadPtr]" << static_cast<void*>(const_cast<char*>(mpcName));
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << " which is " << (u32)muSize << " bytes in size.\n";

        lrHistory.DumpAllPossibleDanglers(static_cast<u8*>(GetClientAddress()));
        CGS_ASSERT(false, "Error");
    }

    // -------------------------------------------------------------------------------------------
    // History::DumpAllPossibleDanglers - walk the whole ring, starting at the most recent record,
    // and log every recorded allocation whose [start, end] range contains lpAddress.
    // -------------------------------------------------------------------------------------------
    void Allocator::History::DumpAllPossibleDanglers(u8* lpAddress)
    {
        bool lbFoundDangler = false;
        u32  luPosition     = muCurrentPosition + KU_HISTORY_LENGTH - 1;
        for (u32 luCount = KU_HISTORY_LENGTH; luCount != 0; --luCount)
        {
            const Entry& lrEntry = maHistory[luPosition++ % KU_HISTORY_LENGTH];
            if (lrEntry.muSize > 0 && lpAddress >= lrEntry.mpStartMem && lpAddress <= lrEntry.mpEndMem)
            {
                lbFoundDangler = true;
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "PossibleDangler: "
                                               << (lrEntry.mpcName ? lrEntry.mpcName : "<NULLSTRING>")
                                               << "\t"
                                               << "Size: "
                                               << (u32)lrEntry.muSize
                                               << "\t"
                                               << "Start: "
                                               << static_cast<void*>(lrEntry.mpStartMem)
                                               << "\t"
                                               << "End: "
                                               << static_cast<void*>(lrEntry.mpEndMem)
                                               << "\n";
                }
            }
        }

        if (!lbFoundDangler && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "No danglers were found.\n";
    }

    // -------------------------------------------------------------------------------------------
    // SortedDump - log every live block from the highest address down, reporting the free gap
    // between each block's end and the block above it, then a total-bytes/total-count summary.
    // -------------------------------------------------------------------------------------------
    void Allocator::SortedDump()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Dumping all allocations ["
                                       << (mpcName ? mpcName : "<NULLSTRING>")
                                       << "]:\n";
        }

        Header* lpPreviousLargest = 0;
        u32     luCount           = 0;
        for (;;)
        {
            Header* lpCurrentLargest = 0;
            for (Header* lpHeader = mpFirstHeader; lpHeader; lpHeader = lpHeader->mpNext)
            {
                if ((lpPreviousLargest == 0 || lpHeader < lpPreviousLargest)
                    && (lpCurrentLargest == 0 || lpHeader > lpCurrentLargest))
                {
                    lpCurrentLargest = lpHeader;
                }
            }
            if (!lpCurrentLargest)
                break;

            ++luCount;
            // The console adds a fixed 160-byte allowance past the user size (the same constant
            // on both 32-bit builds, whatever their Header size).
            u8* lEndOfBlock = reinterpret_cast<u8*>(lpCurrentLargest) + lpCurrentLargest->muSize + 160;
            if (lpPreviousLargest && lEndOfBlock < reinterpret_cast<u8*>(lpPreviousLargest)
                && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "  "
                                           << static_cast<void*>(lEndOfBlock)
                                           << ": "
                                           << (u32)(reinterpret_cast<u8*>(lpPreviousLargest) - lEndOfBlock)
                                           << " "
                                           << " \"[Free Space] ----\" "
                                           << "\n";
            }
            lpCurrentLargest->Dump(mHistory);
            lpPreviousLargest = lpCurrentLargest;
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Total of "
                                       << (u32)muBytesAllocated
                                       << " bytes of user memory allocated in "
                                       << luCount
                                       << " allocations.\n";
        }
    }

    // -------------------------------------------------------------------------------------------
    // DoAllocate - the tracked carve. Over-allocate from the backing allocator, place a Header
    // (list link, backing Resource, name, user size, start guard) immediately before the aligned
    // client block and an end guard + name immediately after it, record the carve in the history
    // ring, and hand the client address back as the Resource's memory lane. A zero-size memory
    // lane is forwarded to the backing allocator untracked.
    // -------------------------------------------------------------------------------------------
    rw::Resource Allocator::DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName)
    {
        static const u32 SKU_OVERALLOCATE      = 16;
        static const u32 SKU_MINIMUM_ALIGNMENT = 4;

        if (mbEnableThreadSafety)
            mMutex.Lock();

        if (mbTestRwac)
        {
            CGS_ASSERT(rw::audio::core::System::IsLocked(Playback::GetDefaultRwacSystem()),
                       "rw::audio::core::System::GetInstance()->IsLocked()");
        }
        CGS_ASSERT(mpAllocator, "mpAllocator");
        CGS_ASSERT(!mbAllocEntered, "Another thread is already in this function!");
        CGS_ASSERT(!mbFreeEntered, "Another thread is already in DoFree()!");
        mbAllocEntered = true;

        rw::ResourceDescriptor      lMemResDesc = lrDescriptor;
        rw::BaseResourceDescriptor& lrMemDesc   = lMemResDesc.m_baseResourceDescriptors[0];
        rw::Resource                lResource;

        if (lrMemDesc.m_size != 0)
        {
            // Room for the Header before the client block (or the alignment, if larger), the end
            // guard + end name after it, plus a fixed slack.
            const size_t luOverhead = (lrMemDesc.m_alignment < sizeof(Header) ? sizeof(Header)
                                                                               : lrMemDesc.m_alignment)
                                    + sizeof(Guard) + sizeof(const char*) + SKU_OVERALLOCATE;
            if (lrMemDesc.m_alignment <= SKU_MINIMUM_ALIGNMENT)
                lrMemDesc.m_alignment = SKU_MINIMUM_ALIGNMENT;
            lrMemDesc.m_size = static_cast<u32>(((lrMemDesc.m_size + SKU_MINIMUM_ALIGNMENT - 1)
                                                 & ~(SKU_MINIMUM_ALIGNMENT - 1)) + luOverhead);

            lResource = mpAllocator->DoAllocate(lMemResDesc, lpcName);
            u8* lpMemory = static_cast<u8*>(lResource.m_baseResources[0]);
            if (!lpMemory)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "Ran out of memory. "
                                               << (u32)muBytesAllocated
                                               << " bytes allocated.";
                }
                SortedDump();
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "Failed allocating "
                                               << lrDescriptor.m_baseResourceDescriptors[0].m_size
                                               << " bytes  of alignment "
                                               << lrDescriptor.m_baseResourceDescriptors[0].m_alignment
                                               << " for alloc Name:"
                                               << lpcName
                                               << "\n";
                }

                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE + 1];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << mpcName
                           << " Allocator ran out of memory. QA!!! Copy the whole allocation dump list!!!!";
                CgsDev::Assert::FireAssert(lacMessageBuffer, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }

            u8* lpvClientAddress = lpMemory + sizeof(Header);
            if (lrMemDesc.m_alignment > 1)
            {
                lpvClientAddress = reinterpret_cast<u8*>(
                    (reinterpret_cast<uintptr_t>(lpMemory) + sizeof(Header) + lrMemDesc.m_alignment - 1)
                    & ~static_cast<uintptr_t>(lrMemDesc.m_alignment - 1));
            }

            if (mpvBadAlloc == lpvClientAddress)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "TestBed::Allocate: ["
                                               << mpcName
                                               << "] Trap on "
                                               << static_cast<void*>(lpvClientAddress)
                                               << "\n";
                }
                // FLAG PC-platform: the console arms its single-step trace trap here; the host
                // equivalent is a debugger break.
                __debugbreak();
            }

            Header& lHeader = *(reinterpret_cast<Header*>(lpvClientAddress) - 1);
            lHeader.mpNext    = mpFirstHeader;
            lHeader.mResource = lResource;
            lHeader.muSize    = lrDescriptor.m_baseResourceDescriptors[0].m_size;
            lHeader.mStartGuard.Construct();
            lHeader.GetEndGuard().Construct();
            CGS_ASSERT(lHeader.muSize, "lHeader.muSize");

            new (&mHistory.DoAllocate()) History::Entry(lpcName, lpMemory, luOverhead);

            if (lpcName)
            {
                lHeader.mpcName            = lpcName;
                *lHeader.GetEndGuardName() = lpcName;
            }
            else if (gpcDebugAllocString)
            {
                lHeader.mpcName            = gpcDebugAllocString;
                *lHeader.GetEndGuardName() = gpcDebugAllocString;
                if (gbRemoveDebugStringAfterUse)
                    gpcDebugAllocString = 0;
            }
            else
            {
                lHeader.mpcName            = "";
                *lHeader.GetEndGuardName() = "";
            }

            mpFirstHeader                = &lHeader;
            lResource.m_baseResources[0] = lpvClientAddress;
            CGS_ASSERT(lrMemDesc.m_alignment <= 1
                           || (reinterpret_cast<uintptr_t>(lpvClientAddress) & (lrMemDesc.m_alignment - 1)) == 0,
                       "rw::IsMemAligned(lpvClientAddress, lMemResDesc.GetAlignment())");

            muBytesAllocated += lHeader.muSize;
            if (mbSanityCheck)
                SanityCheck();

            if (mbVerbose && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "TestBed::Allocate: ["
                                           << mpcName
                                           << "] "
                                           << (u32)lHeader.muSize
                                           << " bytes at "
                                           << static_cast<void*>(lpvClientAddress)
                                           << " ("
                                           << static_cast<void*>(lpMemory)
                                           << ") \""
                                           << (lpcName ? lpcName : "")
                                           << "\"\n";
            }
        }
        else
        {
            lResource = mpAllocator->DoAllocate(lMemResDesc, 0);
            CGS_ASSERT(lResource.m_baseResources[0] == 0, "0 == lRes.GetMemoryResource()");
        }

        mbAllocEntered = false;
        if (mbEnableThreadSafety)
            mMutex.Unlock();
        return lResource;
    }

    // -------------------------------------------------------------------------------------------
    // DoFree - validate the block, unlink its Header from the live list (sanity-checking every
    // block on the way), stamp both guards deallocated and return the original backing Resource.
    // A null memory lane is forwarded to the backing allocator untouched.
    // -------------------------------------------------------------------------------------------
    void Allocator::DoFree(const rw::Resource& lrResource)
    {
        if (mbEnableThreadSafety)
            mMutex.Lock();

        CGS_ASSERT(mpAllocator, "mpAllocator");
        CGS_ASSERT(!mbFreeEntered, "Another thread is already in this function!");
        CGS_ASSERT(!mbAllocEntered, "Another thread is already in DoAllocate()!");
        if (mbTestRwac)
        {
            CGS_ASSERT(rw::audio::core::System::IsLocked(Playback::GetDefaultRwacSystem()),
                       "rw::audio::core::System::GetInstance()->IsLocked()");
        }
        mbFreeEntered = true;

        void* lpvClientAddress = lrResource.m_baseResources[0];
        if (lpvClientAddress)
        {
            if (mpvBadFree == lpvClientAddress)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "TestBed::Free: ["
                                               << mpcName
                                               << "] Trap on "
                                               << lpvClientAddress
                                               << "\n";
                }
                // FLAG PC-platform: the console arms its single-step trace trap here; the host
                // equivalent is a debugger break.
                __debugbreak();
            }

            Header& lHeader = *(static_cast<Header*>(lpvClientAddress) - 1);
            lHeader.SanityCheck(mHistory, mpcName);

            if (mbVerbose && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "TestBed::Free: ["
                                           << mpcName
                                           << "] "
                                           << (u32)lHeader.muSize
                                           << " bytes at "
                                           << lpvClientAddress
                                           << " ("
                                           << lHeader.mResource.m_baseResources[0]
                                           << ")\n";
            }

            Header* lpPreviousHeader = 0;
            for (Header* lpHeader = mpFirstHeader; lpHeader; lpHeader = lpHeader->mpNext)
            {
                lpHeader->SanityCheck(mHistory, mpcName);
                if (lpHeader == &lHeader)
                {
                    if (lpPreviousHeader)
                        lpPreviousHeader->mpNext = lHeader.mpNext;
                    else
                        mpFirstHeader = lHeader.mpNext;
                    break;
                }
                lpPreviousHeader = lpHeader;
            }

            muBytesAllocated -= lHeader.muSize;
            lHeader.mStartGuard.Destruct();
            lHeader.GetEndGuard().Destruct();

            rw::Resource lResource = lHeader.mResource;
            mpAllocator->DoFree(lResource);

            if (mbSanityCheck)
                SanityCheck();
        }
        else
        {
            mpAllocator->DoFree(lrResource);
        }

        mbFreeEntered = false;
        if (mbEnableThreadSafety)
            mMutex.Unlock();
    }
}
}
