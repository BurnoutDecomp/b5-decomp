#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator (base), rw::Resource, rw::ResourceDescriptor
#include <eathread/eathread_mutex.h>            // EA::Thread::Mutex (mMutex)

// CgsSound::TestBed::Allocator - the sound "test bed" resource allocator: a debug wrapper around a
// real rw::IResourceAllocator that tracks every live allocation (a linked list of Header blocks +
// guard words around each user block + a ring of the last 500 alloc/free events) so dangling pointers,
// double frees and heap stomps can be caught and dumped. Layout + method set come from the DecFIGS
// DWARF (GameShared/GameClasses/Sound/CgsTestBedAllocator.h); ctor field stores + the dump/sanity/set
// bodies come from the X360 (Allocator 0x826ADE08, SafeDump 0x826ADFA8, SanityCheck 0x826ADED8,
// SetAllocator 0x826825F0, 'vector deleting destructor' 0x82682690).
namespace CgsSound
{
namespace TestBed
{
    struct Allocator : public rw::IResourceAllocator
    {
        // CgsTestBedAllocator.h:96 - the last-N allocation/free events, a fixed ring buffer used by
        // DumpAllPossibleDanglers to show recent activity around a suspect address.
        struct History
        {
            // CgsTestBedAllocator.h:97 - ring length.
            static const u32 KU_HISTORY_LENGTH = 500;

            // CgsTestBedAllocator.h:100 - one recorded allocation: its name, [start,end) range, size.
            struct Entry
            {
                Entry();
                Entry(const char* lpcName, u8* lpStartMem, size_t luSize);

                const char* mpcName;
                u8*         mpStartMem;
                u8*         mpEndMem;
                size_t      muSize;
            };

            History();

            // CgsTestBedAllocator.h:121 - advance the ring head and return the slot to record into.
            Entry& DoAllocate();

            // CgsTestBedAllocator.h:128 - dump the ring entries that bracket a suspect address.
            void DumpAllPossibleDanglers(u8* lpAddress);

            Entry maHistory[KU_HISTORY_LENGTH];   // CgsTestBedAllocator.h:113
            u32   muCurrentPosition;              // CgsTestBedAllocator.h:115
        };

        // CgsTestBedAllocator.h:652 - the sentinel word written before/after each user block; checked
        // by SanityCheck to detect overruns. The values are the ASCII tags packed big-endian.
        struct Guard
        {
            static const uintptr_t SK_GUARD             = 1936946035;  // CgsTestBedAllocator.h:653 ('sdfm'/"GMFS")
            static const uintptr_t SK_GUARD_DEALLOCATED = 1920103026;  // CgsTestBedAllocator.h:654 ('dvfr'/"rfvd")

            void Construct();   // CgsTestBedAllocator.h:657 - stamp SK_GUARD
            bool IsValid();     // CgsTestBedAllocator.h:663 - true while == SK_GUARD
            void Destruct();    // CgsTestBedAllocator.h:669 - stamp SK_GUARD_DEALLOCATED

        private:
            uintptr_t muValue;  // CgsTestBedAllocator.h:676
        };

        // CgsTestBedAllocator.h:680 - the bookkeeping header that precedes every tracked user block,
        // forming the live-allocation linked list.
        struct Header
        {
            // CgsTestBedAllocator.h:691 - recover the Header that precedes a client pointer.
            Header& GetHeaderFromAddress(void* lpClientAddress);
            // CgsTestBedAllocator.h:697 - the trailing guard word.
            Guard&  GetEndGuard();
            // CgsTestBedAllocator.h:703 - where the trailing name pointer lives.
            const char** GetEndGuardName();
            // CgsTestBedAllocator.h:716 - validate this block's guards against the history.
            void SanityCheck(History& lrHistory, const char* lpcAllocatorName);
            // CgsTestBedAllocator.h:765 - log this block's details.
            void Dump(History& lrHistory);
            // CgsTestBedAllocator.h:774 - the user-visible pointer after the header.
            void* GetClientAddress();

            Header*     mpNext;        // CgsTestBedAllocator.h:681
            rw::Resource mResource;    // CgsTestBedAllocator.h:682
            const char* mpcName;       // CgsTestBedAllocator.h:684
            uintptr_t   muSize;        // CgsTestBedAllocator.h:686
            Guard       mStartGuard;   // CgsTestBedAllocator.h:687
        };

        // CgsTestBedAllocator.h:72 - construct over a backing allocator with a debug name.
        Allocator(const char* lpcName, rw::IResourceAllocator* lpAllocator);
        // CgsTestBedAllocator.h:223
        virtual ~Allocator();

        // CgsTestBedAllocator.h:161 / :183 - is this pointer a live tracked allocation?
        bool IsValidMemoryAddress(const void* lpAddress);
        bool IsValidMemoryAddress(const char* lpcAddress);

        // CgsTestBedAllocator.h:206 / :212 / :217 - the optional "tag this alloc" debug string.
        void SetDebugAllocStringBehavior(bool lbRemoveAfterUse);
        void SetDebugAllocString(const char* lpcString);
        void ClearDebugAllocString();

        // CgsTestBedAllocator.h:229 / :236 -- header-inline flag setters (the X360
        // RootSoundModule::Prepare RWAC stage INLINES them as direct stb stores into
        // the debug-toggle bytes: +0x0C sanity / +0x0D verbose / +0x11 rwac-locked,
        // proving the inline-accessor shape; bodied 2026-08-25 phase A4).
        void SetSanityCheck(bool lbEnable) { mbSanityCheck = lbEnable; }
        void SetVerbose(bool lbEnable)     { mbVerbose = lbEnable; }
        // CgsTestBedAllocator.h:243 - install the backing allocator (asserts if already set).
        void SetAllocator(rw::IResourceAllocator* lpAllocator);
        // CgsTestBedAllocator.h:254 / :262 -- same header-inline setter shape.
        void EnableThreadSafety(bool lbEnable)  { mbEnableThreadSafety = lbEnable; }
        void EnableRwacLockedTest(bool lbEnable) { mbTestRwac = lbEnable; }
        // CgsTestBedAllocator.h:269 / :272 - break when a specific block is allocated/freed.
        void TrapOnAllocate(uintptr_t luAddress);
        void TrapOnFree(uintptr_t luAddress);
        // CgsTestBedAllocator.h:275 - walk the live list and validate every block.
        void SanityCheck();
        // CgsTestBedAllocator.h:294 / :320 - dump every live allocation (list order / sorted).
        void SafeDump();
        void SortedDump();
        // CgsTestBedAllocator.h:406
        void ResetThreadId();
        // CgsTestBedAllocator.h:413
        size_t GetBytesAllocated() const;

    private:
        // CgsTestBedAllocator.h:422 / :562 - the rw::IResourceAllocator overrides (carve/return a block).
        virtual rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* lpcName) override;
        virtual void         DoFree(const rw::Resource& lrResource);

        rw::IResourceAllocator* mpAllocator;        // CgsTestBedAllocator.h:784 - the backing allocator
        Header*                 mpFirstHeader;      // CgsTestBedAllocator.h:785 - live-allocation list head
        bool                    mbSanityCheck;      // CgsTestBedAllocator.h:786
        bool                    mbVerbose;          // CgsTestBedAllocator.h:787
        volatile bool           mbAllocEntered;     // CgsTestBedAllocator.h:788
        volatile bool           mbFreeEntered;      // CgsTestBedAllocator.h:789
        bool                    mbEnableThreadSafety;  // CgsTestBedAllocator.h:790
        bool                    mbTestRwac;         // CgsTestBedAllocator.h:792
        void*                   mpvBadAlloc;        // CgsTestBedAllocator.h:794 - TrapOnAllocate target
        void*                   mpvBadFree;         // CgsTestBedAllocator.h:795 - TrapOnFree target
        const char*             mpcName;            // CgsTestBedAllocator.h:796 - debug name ("Unknown")
        size_t                  muBytesAllocated;   // CgsTestBedAllocator.h:797 - running user-bytes total
        EA::Thread::Mutex       mMutex;             // CgsTestBedAllocator.h:798 - alloc/free serialisation
        History                 mHistory;           // CgsTestBedAllocator.h:801 - recent-events ring
    };
}
}

// CgsTestBedAllocator.cpp:10 - the optional "tag the next allocation with this string" debug hook.
extern const char* gpcDebugAllocString;
extern bool        gbRemoveDebugStringAfterUse;
