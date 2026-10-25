#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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
//   rw::core::filesys::Manager::RegisterDevice / UnregisterDevice
//
// The singleton (gpFileSysManager) is defined here; the
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

            // The global filesys Manager (null until CreateInstance).
            Manager* gpFileSysManager = nullptr;

            // -----------------------------------------------------------------------------
            // ARTIST821811E0: this genuine null driver's IO slots return zero;
            // its Close slot is empty. Those are the original routing-driver semantics.
            class NullFileDriver : public DeviceDriver
            {
            public:
                explicit NullFileDriver(const char* lpcName) : DeviceDriver(lpcName) {}
                ~NullFileDriver() override = default;
                void* Open(const char*, unsigned int, Handle**) override { return nullptr; }
                void Close(void*) override {}
                unsigned int Read(void*, void*, unsigned int, DeviceDriver*, void*) override { return 0; }
                u64 Seek(void*, u64, int, DeviceDriver*, void*) override { return 0; }
                u64 GetSize(void*) override { return 0; }
            };

            // ARTIST82BBE558/82BBE870 allocate this real per-open record.
            // HANDLE and cached64-bit position are the only fields, in this order.
            struct XenonFileHandle
            {
                HANDLE mhFile;
                u64 muPosition;
            };

            class XenonFileDeviceDriver : public DeviceDriver
            {
                static void NormalizePath(char* lpcDest, const char* lpcSource)
                {
                    // Original null-input continuation reads the destination;
                    // do not invent empty-name handling or a truncation policy.
                    if (!lpcSource)
                        lpcSource = lpcDest;
                    char lcCharacter;
                    do
                    {
                        lcCharacter = *lpcSource++;
                        *lpcDest++ = lcCharacter == '/' ? '\\' : lcCharacter;
                    } while (lcCharacter);
                }
                static u64 FileTimeValue(const FILETIME& lrTime)
                {
                    return (static_cast<u64>(lrTime.dwHighDateTime) << 32) | lrTime.dwLowDateTime;
                }
                static void StoreFindData(FindData* lpDest, const WIN32_FIND_DATAA& lrSource)
                {
                    lpDest->mCreationTime = FileTimeValue(lrSource.ftCreationTime);
                    lpDest->mAccessTime = FileTimeValue(lrSource.ftLastAccessTime);
                    lpDest->mModificationTime = FileTimeValue(lrSource.ftLastWriteTime);
                    lpDest->mSize = (static_cast<u64>(lrSource.nFileSizeHigh) << 32) | lrSource.nFileSizeLow;
                    std::strncpy(lpDest->mName, lrSource.cFileName, sizeof(lpDest->mName));
                    lpDest->mName[sizeof(lpDest->mName) - 1] = 0;
                    lpDest->mFlags = 0;
                    if (lrSource.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                        lpDest->mFlags = 1;
                    if (lrSource.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        lpDest->mFlags |= 2;
                }
            public:
                // Static births82C73F28/3F70/3FB8 install only the real base name.
                explicit XenonFileDeviceDriver(const char* lpcName) : DeviceDriver(lpcName) {}
                ~XenonFileDeviceDriver() override = default;

                // FLAG PC-platform leaf: XDK's named Win32-compatible file API
                // is expressed through the host API; all engine flags/order remain.
                void* Open(const char* lpcName, unsigned int luFlags, Handle**) override
                {
                    DWORD luAccess = GENERIC_READ;
                    if (luFlags & 1)
                        luAccess |= GENERIC_WRITE;
                    DWORD luDisposition = OPEN_EXISTING;
                    switch (luFlags & 6)
                    {
                    case 2: luDisposition = CREATE_NEW; break;
                    case 4: luDisposition = TRUNCATE_EXISTING; break;
                    case 6: luDisposition = CREATE_ALWAYS; break;
                    }
                    DWORD luAttributes = FILE_ATTRIBUTE_NORMAL;
                    if (luFlags & 16)
                        luAttributes |= FILE_FLAG_NO_BUFFERING;
                    if (luFlags & 8)
                        luAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
                    char lacPath[256];
                    if (*lpcName == '/' || *lpcName == '\\')
                    {
                        std::strcpy(lacPath, mDeviceName);
                        std::strcat(lacPath, lpcName);
                    }
                    else
                        std::strcpy(lacPath, lpcName);
                    for (char* lpc = lacPath; *lpc; ++lpc)
                        if (*lpc == '/')
                            *lpc = '\\';
                    HANDLE lhFile = ::CreateFileA(lacPath, luAccess, FILE_SHARE_READ, nullptr,
                                                 luDisposition, luAttributes, nullptr);
                    if (lhFile == INVALID_HANDLE_VALUE)
                        return nullptr;
                    auto* lpResult = static_cast<XenonFileHandle*>(gpFileSysAllocator->Alloc(
                        sizeof(XenonFileHandle), KAC_ALLOC_TAG, 0, 4, 0));
                    if (!lpResult)
                        return nullptr;
                    lpResult->mhFile = lhFile;
                    lpResult->muPosition = 0;
                    return lpResult;
                }

                void Close(void* lpFile) override
                {
                    ::CloseHandle(static_cast<XenonFileHandle*>(lpFile)->mhFile);
                    gpFileSysAllocator->Free(lpFile, 0);
                }
                unsigned int Read(void* lpFile, void* lpBuffer, unsigned int luSize,
                                  DeviceDriver*, void*) override
                {
                    auto* lpHandle = static_cast<XenonFileHandle*>(lpFile);
                    DWORD luRead;
                    if (!::ReadFile(lpHandle->mhFile, lpBuffer, luSize, &luRead, nullptr))
                    {
                        bool (*const lpErrorHandler)(s32) = gpFileSysManager->mErrorHandler;
                        if (!lpErrorHandler(static_cast<s32>(::GetLastError())))
                            ::Sleep(INFINITE);
                    }
                    lpHandle->muPosition += luRead;
                    return luRead;
                }
                unsigned int Write(void* lpFile, const void* lpBuffer, unsigned int luSize,
                                   DeviceDriver*, void*) override
                {
                    auto* lpHandle = static_cast<XenonFileHandle*>(lpFile);
                    DWORD luWritten;
                    ::WriteFile(lpHandle->mhFile, lpBuffer, luSize, &luWritten, nullptr);
                    lpHandle->muPosition += luWritten;
                    return luWritten;
                }
                u64 Seek(void* lpFile, u64 luPosition, int, DeviceDriver*, void*) override
                {
                    auto* lpHandle = static_cast<XenonFileHandle*>(lpFile);
                    if (luPosition == lpHandle->muPosition)
                        return luPosition;
                    LARGE_INTEGER lDistance;
                    lDistance.QuadPart = static_cast<LONGLONG>(luPosition);
                    LARGE_INTEGER lResult;
                    if (::SetFilePointerEx(lpHandle->mhFile, lDistance, &lResult, FILE_BEGIN))
                        lpHandle->muPosition = static_cast<u64>(lResult.QuadPart);
                    else
                        lpHandle->muPosition = ::SetFilePointer(lpHandle->mhFile, 0, nullptr, FILE_CURRENT);
                    return lpHandle->muPosition;
                }
                u64 GetSize(void* lpFile) override
                {
                    LARGE_INTEGER lSize;
                    if (::GetFileSizeEx(static_cast<XenonFileHandle*>(lpFile)->mhFile, &lSize) == TRUE)
                        return static_cast<u64>(lSize.QuadPart);
                    return 0;
                }
                bool Resize(void* lpFile, u64 luSize) override
                {
                    auto* lpHandle = static_cast<XenonFileHandle*>(lpFile);
                    LARGE_INTEGER lZero;
                    lZero.QuadPart = 0;
                    LARGE_INTEGER lSize;
                    lSize.QuadPart = static_cast<LONGLONG>(luSize);
                    LARGE_INTEGER lPosition;
                    if (::SetFilePointerEx(lpHandle->mhFile, lZero, &lPosition, FILE_CURRENT)
                        && ::SetFilePointerEx(lpHandle->mhFile, lSize, nullptr, FILE_BEGIN)
                        && ::SetEndOfFile(lpHandle->mhFile)
                        && ::SetFilePointerEx(lpHandle->mhFile, lPosition, nullptr, FILE_BEGIN))
                    {
                        lpHandle->muPosition = static_cast<u64>(lPosition.QuadPart);
                        return true;
                    }
                    return false;
                }
                bool Delete(const char* lpcName) override
                {
                    char lacPath[256];
                    NormalizePath(lacPath, lpcName);
                    return ::DeleteFileA(lacPath) == TRUE;
                }
                bool Move(const char* lpcSource, const char* lpcDestination) override
                {
                    char lacDestination[256];
                    char lacSource[256];
                    NormalizePath(lacDestination, lpcDestination);
                    NormalizePath(lacSource, lpcSource);
                    return ::MoveFileA(lacSource, lacDestination) == TRUE;
                }
                void* FindBegin(const char* lpcName, FindData* lpFindData) override
                {
                    char lacPath[256];
                    NormalizePath(lacPath, lpcName);
                    WIN32_FIND_DATAA lData;
                    HANDLE lhFind = ::FindFirstFileA(lacPath, &lData);
                    XenonFileHandle* lpResult = nullptr;
                    if (lhFind != INVALID_HANDLE_VALUE)
                    {
                        lpResult = static_cast<XenonFileHandle*>(gpFileSysAllocator->Alloc(
                            sizeof(XenonFileHandle), KAC_ALLOC_TAG, 0, 4, 0));
                        if (lpResult)
                        {
                            lpResult->mhFile = lhFind;
                            lpResult->muPosition = 0;
                        }
                        if (lpFindData)
                            StoreFindData(lpFindData, lData);
                    }
                    return lpResult;
                }
                bool FindNext(void* lpFind, FindData* lpFindData) override
                {
                    WIN32_FIND_DATAA lData;
                    const bool lbFound = ::FindNextFileA(static_cast<XenonFileHandle*>(lpFind)->mhFile, &lData) == TRUE;
                    if (lbFound && lpFindData)
                        StoreFindData(lpFindData, lData);
                    return lbFound;
                }
                bool FindEnd(void* lpFind) override
                {
                    // FLAG PC-platform leaf: a native FindFirstFile handle uses
                    // FindClose; this is XDK CloseHandle's matching host operation.
                    const bool lbResult = ::FindClose(static_cast<XenonFileHandle*>(lpFind)->mhFile) != FALSE;
                    gpFileSysAllocator->Free(lpFind, 0);
                    return lbResult;
                }
                bool DirectoryCreate(const char* lpcName) override
                {
                    char lacPath[256];
                    NormalizePath(lacPath, lpcName);
                    return ::CreateDirectoryA(lacPath, nullptr) == TRUE;
                }
                bool DirectoryRemove(const char* lpcName) override
                {
                    char lacPath[256];
                    NormalizePath(lacPath, lpcName);
                    return ::RemoveDirectoryA(lacPath) == TRUE;
                }
            };

            // Exact original birth/table order and name strings, not null descriptors.
            // 82C73EE0 ->8327F098;3F28 ->F084;3F70 ->F0AC;3FB8 ->F0C0.
            namespace
            {
                NullFileDriver gNullFileDriver("null:");
                XenonFileDeviceDriver gDiscFileDriver("D:");
                XenonFileDeviceDriver gCacheFileDriver("CACHE:");
                XenonFileDeviceDriver gNetFileDriver("NET:");
                DeviceDriver* const KP_DEFAULT_DEVICE_DESC = &gNullFileDriver;
                DeviceDriver* const KAP_DEVICE_TABLE[] =
                    { &gDiscFileDriver, &gCacheFileDriver, &gNetFileDriver };
                const char* const KPC_DEFAULT_SEARCH_PATH = "D:";
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
            // Clear the registered-device list (mDeviceList head/tail/count),
            // default-construct the embedded ThreadParameters, then bump their priority by 2;
            // clear the Init-owned fields, seed the handle id to -1, and latch the search-
            // path capacity / scratch size from the config (config[0] / config[1]).
            // -----------------------------------------------------------------------------
            Manager::Manager(const FileSystemConfig* lpConfig)
                // mDeviceList emptied in the body (a1[0..2] = 0).
                // mThreadParameters default-constructed here (a1+3); priority bumped below.
                : miMaxReadSize(-1)            // a1[9] = -1
                , mpDefaultDevice(nullptr)     // a1[10] = 0
                , mpScratchBuffer(nullptr)     // a1[11] = 0
                , mpCurrentDir(nullptr)        // a1[12] = 0
                , muScratchSize(lpConfig->muScratchSize)     // a1[13] = a2[1]
                , muMaxSearchPaths(lpConfig->muMaxSearchPaths) // a1[14] = a2[0]
                , mpSearchPathTable(nullptr)   // a1[15] = 0
                , mbSortByPosition(0)          // a1[16] = 0
                , mErrorHandler(nullptr)              // a1[17] = 0
            {
                mDeviceList.mpHead  = nullptr;   // *a1 = 0
                mDeviceList.mpTail  = nullptr;   // a1[1] = 0
                mDeviceList.muCount = 0;         // a1[2] = 0

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
                ::memset(mpSearchPathTable, 0, sizeof(SearchPathEntry) * muMaxSearchPaths);

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
            // (sizeof(SearchPathEntry)*muMaxSearchPaths), register the default device, then each
            // entry in the static device table, then point mpCurrentDir at the default search
            // path string and apply it. Returns the SetSearchPath result.
            // -----------------------------------------------------------------------------
            int Manager::Init()
            {
                mpScratchBuffer   = static_cast<char*>(Allocate(muScratchSize));      // a1[11]
                mpSearchPathTable = static_cast<SearchPathEntry*>(Allocate(static_cast<u32>(sizeof(SearchPathEntry) * muMaxSearchPaths))); // a1[15]

                // a1[10] = RegisterDevice(off_8327F078, &dword_8327F098, 0). The X360 call
                // target is gpFileSysManager (== this); the actual NullFileDriver is statically owned.
                mpDefaultDevice = RegisterDevice(KP_DEFAULT_DEVICE_DESC, 0);

                // Register every entry in the static device table [off_82F91A3C, off_82F91A48).
                // Actual three-driver order and names are pinned by raw image + static births.
                DeviceDriver* const* lpEntry = KAP_DEVICE_TABLE;                     // v2
                DeviceDriver* const* lpEnd   = KAP_DEVICE_TABLE + sizeof(KAP_DEVICE_TABLE) / sizeof(KAP_DEVICE_TABLE[0]);                 // table end
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

                // FLAG PC-platform leaf: actual Manager sizeof and two-pointer
                // SearchPathEntry sizeof follow native widths; retain original
                // allocator/name/flags/alignment4 and construction order.
                void* lpRaw = gpFileSysAllocator->Alloc(sizeof(Manager), KAC_ALLOC_TAG, 0, 4, 0);
                Manager* lpMgr = lpRaw ? new (lpRaw) Manager(lpConfig) : nullptr;
                gpFileSysManager = lpMgr;                    // off_8327F078 = v3

                gpFileSysManager->Init();
                return gpFileSysManager;
            }

            // -----------------------------------------------------------------------------
            // Manager::~Manager @0x82BBF008
            //
            //   Free(mpSearchPathTable); mpSearchPathTable = 0;
            //   Free(mpScratchBuffer);   mpScratchBuffer   = 0;
            //   while (head) UnregisterDevice(head);
            //   mpDefaultDevice = 0; tail = 0; head = 0; count = 0;
            // -----------------------------------------------------------------------------
            Manager::~Manager()
            {
                gpFileSysAllocator->Free(mpSearchPathTable, 0);  // (*(*alloc+12))(alloc, a1[15], 0)
                mpSearchPathTable = nullptr;                     // a1[15] = 0
                gpFileSysAllocator->Free(mpScratchBuffer, 0);    // (*(*alloc+12))(alloc, a1[11], 0)
                mpScratchBuffer = nullptr;                       // a1[11] = 0

                while (mDeviceList.mpHead)
                    UnregisterDevice(mDeviceList.mpHead);

                mpDefaultDevice     = nullptr;   // a1[10] = 0
                mDeviceList.mpTail  = nullptr;   // a1[1]  = 0
                mDeviceList.mpHead  = nullptr;   // *a1    = 0
                mDeviceList.muCount = 0;         // a1[2]  = 0
            }

            // -----------------------------------------------------------------------------
            // Manager::RegisterDevice
            //
            // Allocate and construct a Device for lpDriver (luFlags bit 0 = externally
            // pumped), then append it to the global manager's device list -- the list of
            // gpFileSysManager, not `this` (the method is const).
            // -----------------------------------------------------------------------------
            Device* Manager::RegisterDevice(DeviceDriver* lpDriver, unsigned int luFlags) const
            {
                void* lpRaw = Allocate(sizeof(Device));
                Device* lpDevice = lpRaw ? new (lpRaw) Device(lpDriver, luFlags) : nullptr;
                gpFileSysManager->mDeviceList.Append(lpDevice);
                return lpDevice;
            }

            // -----------------------------------------------------------------------------
            // Manager::UnregisterDevice
            //
            // Unlink lpDevice from the global manager's device list; if it was registered,
            // destroy it (scalar deleting destructor, freeing through the filesys allocator).
            // -----------------------------------------------------------------------------
            bool Manager::UnregisterDevice(Device* lpDevice) const
            {
                if (!gpFileSysManager->mDeviceList.Remove(lpDevice, nullptr))
                    return false;
                if (lpDevice)
                    DeviceScalarDeletingDtor(lpDevice, 1);
                return true;
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
