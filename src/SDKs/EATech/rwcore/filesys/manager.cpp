// =====================================================================================
// rw::core::filesys::Manager -- the RenderWare core filesystem manager singleton.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::core::filesys::Manager::Allocate                @0x82BBD640
//   rw::core::filesys::Manager::Free                    @0x82BBD678
//   rw::core::filesys::Manager::GetInstance             @0x82BBD630
//   rw::core::filesys::Manager::CreateInstance          @0x82BBF518
//   rw::core::filesys::Manager::DestroyInstance         @0x82BBF850
//   rw::core::filesys::Manager::Manager                 @0x82BBE418  (ctor)
//   rw::core::filesys::Manager::~Manager                @0x82BBF008  (dtor)
//   rw::core::filesys::Manager::Init                    @0x82BBF0B8
//   rw::core::filesys::Manager::SetSearchPath           @0x82BBEEC8
//   rw::core::filesys::Manager::`scalar deleting dtor'  @0x82BBF5A0
//
// The singleton lives at off_8327F078 (gpFileSysManager, defined in device.cpp); the
// filesys allocator lives at off_8327F07C (gpFileSysAllocator, defined in handle.cpp).
// Every heap op goes through the allocator's vtable:
//   Alloc  = (*(*alloc + 4))(alloc, size, "rw::core::filesystem::Manager::Allocate", 0, 4, 0)
//   Free   = (*(*alloc + 12))(alloc, block, 0)
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/device.h"   // Manager / Device / Allocator / FileSystemConfig

#include <cstring>   // memset, strchr
#include <cstdint>   // intptr_t
#include <new>       // placement new (Manager construction over allocator storage)

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // The allocation tag string the X360 hands as Alloc's name argument
            // (aRwCoreFilesyst). Reproduced verbatim from the asm string reference.
            static const char KAC_ALLOC_TAG[] = "rw::core::filesystem::Manager::Allocate";

            // -----------------------------------------------------------------------------
            // Un-recoverable rodata referenced by Init (device descriptors + default search
            // path). The X360 addresses are known but their CONTENTS are not in any export,
            // so they are modelled as flagged-null placeholders -- NEVER fabricated. When the
            // device-table / default-path data is recovered, fill these in.
            //   &dword_8327F098 : the default-device descriptor handed to RegisterDevice
            //   off_82F91A3C .. off_82F91A48 : a static table of device descriptors
            //   &unk_821811B4   : the default search-path string
            // -----------------------------------------------------------------------------
            namespace
            {
                const void* const KP_DEFAULT_DEVICE_DESC = nullptr; // &dword_8327F098 (UNRECOVERED)
                const void* const KAP_DEVICE_TABLE[1] = { nullptr };// off_82F91A3C.. (UNRECOVERED)
                const char* const KPC_DEFAULT_SEARCH_PATH = nullptr;// &unk_821811B4   (UNRECOVERED)
            }

            // -----------------------------------------------------------------------------
            // Manager::Allocate @0x82BBD640
            //   return (*(*alloc + 4))(alloc, a1, "rw::core::filesystem::Manager::Allocate",
            //                          0, 4, 0);
            // -----------------------------------------------------------------------------
            void* Manager::Allocate(u32 luSize)
            {
                return gpFileSysAllocator->Alloc(luSize, KAC_ALLOC_TAG, 0, 4, 0);
            }

            // -----------------------------------------------------------------------------
            // Manager::Free @0x82BBD678
            //   return (*(*alloc + 12))(alloc, a1, 0);
            // The X360 returns the Free vtable call's value (modelled as void*).
            // -----------------------------------------------------------------------------
            void* Manager::Free(void* lpBlock)
            {
                gpFileSysAllocator->Free(lpBlock, 0);
                return lpBlock;
            }

            // -----------------------------------------------------------------------------
            // Manager::GetInstance @0x82BBD630
            //   return off_8327F078;
            // -----------------------------------------------------------------------------
            void* Manager::GetInstance()
            {
                return gpFileSysManager;
            }

            // -----------------------------------------------------------------------------
            // Manager::Manager @0x82BBE418  (ctor)
            //
            // Clear the registered-device list (mpDeviceListHead/mpListField1/mpListField2),
            // default-construct the embedded ThreadParameters, then bump their priority by 2;
            // clear the Init-owned fields, seed the handle id to -1, and latch the search-
            // path capacity / scratch size from the config (config[0] / config[1]).
            // -----------------------------------------------------------------------------
            Manager::Manager(const FileSystemConfig* lpConfig)
                : mpDeviceListHead(nullptr)   // *a1 = 0
                , mpListField1(nullptr)        // a1[1] = 0
                , mpListField2(nullptr)        // a1[2] = 0
                // mThreadParameters default-constructed here (a1+3); priority bumped below.
                , miMaxReadSize(-1)            // a1[9] = -1
                , mpDefaultDevice(nullptr)     // a1[10] = 0
                , mpScratchBuffer(nullptr)     // a1[11] = 0
                , mpCurrentDir(nullptr)        // a1[12] = 0
                , muScratchSize(lpConfig->muScratchSize)     // a1[13] = a2[1]
                , muMaxSearchPaths(lpConfig->muMaxSearchPaths) // a1[14] = a2[0]
                , mpSearchPathTable(nullptr)   // a1[15] = 0
                , mbSortByPosition(0)          // a1[16] = 0
                , muReserved44(0)              // a1[17] = 0
            {
                // a1[5] += 2 -- bump the embedded ThreadParameters' priority by 2.
                mThreadParameters.mnPriority = mThreadParameters.mnPriority + 2;
            }

            // -----------------------------------------------------------------------------
            // Manager::SetSearchPath @0x82BBEEC8
            //
            // Rebuild the search-path table from a ';'-separated path string:
            //   * zero the scratch buffer (muScratchSize bytes) and the table (8*max bytes),
            //   * copy the source path into the scratch buffer,
            //   * split on ';', trimming a trailing '/' or '\\' off each segment, and for
            //     each segment store { segment, Device::GetInstance(segment) } into the table,
            //   * finally trim a trailing '/' or '\\' off the last stored segment.
            // Returns the last Device::GetInstance result (X360 r3).
            // -----------------------------------------------------------------------------
            int Manager::SetSearchPath(const char* lpcSearchPath)
            {
                ::memset(mpScratchBuffer, 0, muScratchSize);
                ::memset(mpSearchPathTable, 0, 8 * muMaxSearchPaths);

                // Copy the source path (NUL-terminated) into the scratch buffer.
                char* lpDst = mpScratchBuffer;
                const char* lpSrc = lpcSearchPath;
                char lcCh;
                do
                {
                    lcCh = *lpSrc++;
                    *lpDst++ = lcCh;
                }
                while (lcCh);

                int liResult = 0;            // r3 (last GetInstance result)
                char* lpSeg = mpScratchBuffer; // v8 -- start of the current segment
                u32 luIndex = 0;             // v9 -- table entry index
                char* lpEntryStart = lpSeg;   // v10 -- segment recorded into the table

                if (lpSeg)
                {
                    do
                    {
                        char* lpSemi = ::strchr(lpSeg, ';');   // v12
                        lpSeg = lpSemi;
                        if (lpSemi)
                        {
                            char lcPrev = lpSemi[-1];           // v13
                            *lpSemi = '\0';
                            if (lcPrev == '/' || lcPrev == '\\')
                                lpSemi[-1] = '\0';
                            lpSeg = lpSemi + 1;
                        }

                        mpSearchPathTable[luIndex].mpcPath = lpEntryStart;  // *(table + v11) = v10
                        Device* lpDev = Device::GetInstance(lpEntryStart, nullptr);
                        liResult = static_cast<int>(reinterpret_cast<intptr_t>(lpDev));
                        lpEntryStart = lpSeg;
                        mpSearchPathTable[luIndex].mpDevice = lpDev;          // *(v14 + 4) = result
                        ++luIndex;
                    }
                    while (lpSeg);
                }

                // Trim a trailing '/' or '\\' off the last stored segment.
                const char* lpLast = mpSearchPathTable[luIndex - 1].mpcPath; // *(8*v9 + table - 8)
                char* lpEnd = const_cast<char*>(lpLast);
                while (*lpEnd)
                    ++lpEnd;
                if (lpEnd != lpLast)
                {
                    char lcEnd = lpEnd[-1];                                   // v18
                    if (lcEnd == '/' || lcEnd == '\\')
                        lpEnd[-1] = '\0';
                }

                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Manager::Init @0x82BBF0B8
            //
            // Allocate the scratch buffer (muScratchSize) and the search-path table
            // (8*muMaxSearchPaths), register the default device descriptor, register each
            // entry in the static device table, then point mpCurrentDir at the default search
            // path string and apply it. Returns the SetSearchPath result.
            // -----------------------------------------------------------------------------
            int Manager::Init()
            {
                mpScratchBuffer   = static_cast<char*>(Allocate(muScratchSize));      // a1[11]
                mpSearchPathTable = static_cast<SearchPathEntry*>(Allocate(8 * muMaxSearchPaths)); // a1[15]

                // a1[10] = RegisterDevice(off_8327F078, &dword_8327F098, 0). The X360 call
                // target is gpFileSysManager (== this); the descriptor is UNRECOVERED rodata.
                mpDefaultDevice = RegisterDevice(KP_DEFAULT_DEVICE_DESC, 0);

                // Register every entry in the static device table [off_82F91A3C, off_82F91A48).
                // The table CONTENTS are UNRECOVERED; the placeholder has a single null entry
                // so the loop shape (do/while v2 < end) is reproduced without fabricating data.
                const void* const* lpEntry = KAP_DEVICE_TABLE;                       // v2
                const void* const* lpEnd   = KAP_DEVICE_TABLE + 1;                   // off_82F91A48
                do
                {
                    RegisterDevice(*lpEntry, 0);
                    ++lpEntry;
                }
                while (lpEntry < lpEnd);

                mpCurrentDir = KPC_DEFAULT_SEARCH_PATH;     // a1[12] = &unk_821811B4
                return SetSearchPath(KPC_DEFAULT_SEARCH_PATH);
            }

            // -----------------------------------------------------------------------------
            // Manager::CreateInstance @0x82BBF518
            //
            //   off_8327F07C = config->mpAllocator;       // install the allocator
            //   raw = Alloc(0x48);                        // 72 bytes for the Manager
            //   off_8327F078 = raw ? Manager(raw, config) : 0;
            //   Manager::Init();
            //   return off_8327F078;
            // -----------------------------------------------------------------------------
            void* Manager::CreateInstance(const FileSystemConfig* lpConfig)
            {
                gpFileSysAllocator = lpConfig->mpAllocator;  // off_8327F07C = *(a1+8)

                void* lpRaw = gpFileSysAllocator->Alloc(0x48, KAC_ALLOC_TAG, 0, 4, 0);
                Manager* lpMgr = lpRaw ? new (lpRaw) Manager(lpConfig) : nullptr;
                gpFileSysManager = lpMgr;                    // off_8327F078 = v3

                if (gpFileSysManager)
                    gpFileSysManager->Init();
                return gpFileSysManager;
            }

            // -----------------------------------------------------------------------------
            // Manager::~Manager @0x82BBF008
            //
            //   Free(mpSearchPathTable); mpSearchPathTable = 0;
            //   Free(mpScratchBuffer);   mpScratchBuffer   = 0;
            //   while (mpDeviceListHead) UnregisterDevice();
            //   mpDefaultDevice = 0; mpListField1 = 0; mpDeviceListHead = 0; mpListField2 = 0;
            // -----------------------------------------------------------------------------
            Manager::~Manager()
            {
                gpFileSysAllocator->Free(mpSearchPathTable, 0);  // (*(*alloc+12))(alloc, a1[15], 0)
                mpSearchPathTable = nullptr;                     // a1[15] = 0
                gpFileSysAllocator->Free(mpScratchBuffer, 0);    // (*(*alloc+12))(alloc, a1[11], 0)
                mpScratchBuffer = nullptr;                       // a1[11] = 0

                while (mpDeviceListHead)
                    UnregisterDevice();

                mpDefaultDevice  = nullptr;   // a1[10] = 0
                mpListField1     = nullptr;   // a1[1]  = 0
                mpDeviceListHead = nullptr;   // *a1    = 0
                mpListField2     = nullptr;   // a1[2]  = 0
            }

            // -----------------------------------------------------------------------------
            // Manager::`scalar deleting destructor' @0x82BBF5A0
            //   ~Manager(this);
            //   if (flags & 1) (*(*off_8327F07C + 12))(off_8327F07C, this, 0);  // Free
            //   return this;
            // -----------------------------------------------------------------------------
            Manager* ManagerScalarDeletingDtor(Manager* lpManager, char lcFlags)
            {
                lpManager->~Manager();
                if (lcFlags & 1)
                    gpFileSysAllocator->Free(lpManager, 0);
                return lpManager;
            }

            // -----------------------------------------------------------------------------
            // Manager::DestroyInstance @0x82BBF850
            //   result = off_8327F078;
            //   if (off_8327F078) result = scalar_deleting_dtor(off_8327F078, 1);
            //   off_8327F078 = 0;
            //   return result;
            // -----------------------------------------------------------------------------
            void* Manager::DestroyInstance()
            {
                Manager* lpResult = gpFileSysManager;
                if (gpFileSysManager)
                    lpResult = ManagerScalarDeletingDtor(gpFileSysManager, 1);
                gpFileSysManager = nullptr;
                return lpResult;
            }
        }
    }
}
