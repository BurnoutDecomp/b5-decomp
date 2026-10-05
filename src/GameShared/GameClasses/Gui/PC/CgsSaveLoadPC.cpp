#include "GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.h"

// FLAG PC-platform leaf (whole TU): the PC realisation of the console memory-card
// storage edge -- see the header banner. Deliberately self-contained (types.hpp +
// Win32 only) so the container round-trip is unit-testable standalone.

#include <cstring>
#include <cstdio>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <exception>

#include <Windows.h>

#include "GameShared/GameClasses/System/CgsHarnessSlot.h"  // BRN_HARNESS_SLOT Memcard-directory suffix (parallel harness slots)

namespace CgsGui
{
namespace SaveLoadPC
{
namespace
{
    // The container directory/extension: the profile container sits next to the
    // "Memcard\SaveImage.png" content image the save/load system already reads.
    const char KACP_MEMCARD_DIR[]     = "Memcard";
    const char KACP_SAVE_EXTENSION[]  = ".sav";

    // ⭐ THE HARNESS SLOT SUFFIX (CgsHarnessSlot.h). The profile container is the one piece
    // of per-instance state the parallel test harness cannot share: the directory is resolved
    // against the working directory, which every slot shares (that is what lets one 5.9 GB
    // data set serve them all), so two instances writing one Profile.sav is a CORRUPTED save,
    // not merely a contended one. BRN_HARNESS_SLOT unset or 0 yields "Memcard" verbatim, so
    // an ordinary launch reads and writes exactly the save it always has.
    const char* MemcardDir()
    {
        static char sacDir[32] = { 0 };
        static const char* spcDir = 0;
        if (spcDir == 0)
            spcDir = CgsSystem::HarnessSlot::Name(sacDir, sizeof(sacDir), KACP_MEMCARD_DIR);
        return spcDir;
    }

    const u32 KU_CONTAINER_MAGIC   = 0x42355356u;   // 'B5SV'
    const u32 KU_CONTAINER_VERSION = 1u;

    // The on-disk container header (PC-native little-endian; a serialised file-format
    // record, all access through this struct). The payload follows immediately:
    // muImageSize bytes of profile image, then muMugshotsSize bytes of mugshot blob.
    struct ContainerHeader
    {
        u32  muMagic;               // KU_CONTAINER_MAGIC
        u32  muVersion;             // KU_CONTAINER_VERSION
        u32  muImageSize;           // profile-image payload bytes
        u32  muMugshotsSize;        // mugshot payload bytes (0 == none stored)
        u32  muPayloadHash;         // FNV-1a over image bytes then mugshot bytes
        u32  muReserved;            // 0
        char macTitle[32];          // SaveInfo title (user-facing, save-listing UI)
        char macDescription[256];   // SaveInfo description
    };

    // FNV-1a (32-bit), incremental -- the container's corruption guard. Seed the first
    // call with KU_HASH_SEED and fold every payload span through in file order.
    const u32 KU_HASH_SEED = 2166136261u;

    u32 HashBytes(u32 luHash, const void* lpData, u32 luSize)
    {
        const u8* lpBytes = static_cast<const u8*>(lpData);
        for (u32 luIndex = 0; luIndex < luSize; ++luIndex)
        {
            luHash = (luHash ^ lpBytes[luIndex]) * 16777619u;
        }
        return luHash;
    }

    u32 HashPayload(const void* lpImage, u32 luImageSize,
                    const void* lpMugshots, u32 luMugshotsSize)
    {
        u32 luHash = HashBytes(KU_HASH_SEED, lpImage, luImageSize);
        if (lpMugshots != 0 && luMugshotsSize != 0)
        {
            luHash = HashBytes(luHash, lpMugshots, luMugshotsSize);
        }
        return luHash;
    }

    // Build "Memcard\<name>.sav" into the caller's buffer. False when the name is
    // missing/empty or does not fit.
    bool BuildContainerPath(char* lpacPath, u32 luPathSize, const char* lpacName)
    {
        if (lpacName == 0 || lpacName[0] == '\0')
        {
            return false;
        }
        const int liWritten = std::snprintf(lpacPath, luPathSize, "%s\\%s%s",
                                            MemcardDir(), lpacName, KACP_SAVE_EXTENSION);
        return liWritten > 0 && static_cast<u32>(liWritten) < luPathSize;
    }

    bool WriteAll(HANDLE lhFile, const void* lpData, u32 luSize)
    {
        const u8* lpBytes    = static_cast<const u8*>(lpData);
        u32       luRemaining = luSize;
        while (luRemaining > 0)
        {
            DWORD luWritten = 0;
            if (!::WriteFile(lhFile, lpBytes, luRemaining, &luWritten, 0) || luWritten == 0)
            {
                return false;
            }
            lpBytes     += luWritten;
            luRemaining -= luWritten;
        }
        return true;
    }

    bool ReadAll(HANDLE lhFile, void* lpData, u32 luSize)
    {
        u8* lpBytes     = static_cast<u8*>(lpData);
        u32 luRemaining = luSize;
        while (luRemaining > 0)
        {
            DWORD luRead = 0;
            if (!::ReadFile(lhFile, lpBytes, luRemaining, &luRead, 0) || luRead == 0)
            {
                return false;
            }
            lpBytes     += luRead;
            luRemaining -= luRead;
        }
        return true;
    }

    void CopyStringField(char* lpacDest, u32 luDestSize, const char* lpacSource)
    {
        std::memset(lpacDest, 0, luDestSize);
        if (lpacSource != 0)
        {
            std::strncpy(lpacDest, lpacSource, luDestSize - 1);
        }
    }
}

bool ContainerExists(const char* lpacName)
{
    char lacPath[MAX_PATH];
    if (!BuildContainerPath(lacPath, sizeof(lacPath), lpacName))
    {
        return false;
    }
    const DWORD luAttributes = ::GetFileAttributesA(lacPath);
    return luAttributes != INVALID_FILE_ATTRIBUTES &&
           (luAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool WriteContainerAtPath(const char* lpcDirectory, const char* lpcPath,
                    const void* lpImage, u32 luImageSize,
                    const void* lpMugshots, u32 luMugshotsSize,
                    const char* lpacTitle, const char* lpacDescription)
{
    if (lpImage == 0 || luImageSize == 0)
    {
        return false;
    }
    if (lpMugshots == 0)
    {
        luMugshotsSize = 0;
    }

    // The container directory may not exist on a fresh install; ERROR_ALREADY_EXISTS
    // is the normal case afterwards.
    ::CreateDirectoryA(lpcDirectory, 0);

    ContainerHeader lHeader;
    std::memset(&lHeader, 0, sizeof(lHeader));
    lHeader.muMagic        = KU_CONTAINER_MAGIC;
    lHeader.muVersion      = KU_CONTAINER_VERSION;
    lHeader.muImageSize    = luImageSize;
    lHeader.muMugshotsSize = luMugshotsSize;
    lHeader.muPayloadHash  = HashPayload(lpImage, luImageSize, lpMugshots, luMugshotsSize);
    CopyStringField(lHeader.macTitle, sizeof(lHeader.macTitle), lpacTitle);
    CopyStringField(lHeader.macDescription, sizeof(lHeader.macDescription), lpacDescription);

    // Write the whole container to a temp sibling, then swap it in atomically so an
    // interrupted save can never destroy the previous good container.
    const std::string lTempPath = std::string(lpcPath) + ".tmp";

    HANDLE lhFile = ::CreateFileA(lTempPath.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, 0);
    if (lhFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool lbOk = WriteAll(lhFile, &lHeader, static_cast<u32>(sizeof(lHeader))) &&
                WriteAll(lhFile, lpImage, luImageSize) &&
                (luMugshotsSize == 0 || WriteAll(lhFile, lpMugshots, luMugshotsSize));
    lbOk = ::FlushFileBuffers(lhFile) != 0 && lbOk;
    ::CloseHandle(lhFile);

    if (!lbOk)
    {
        ::DeleteFileA(lTempPath.c_str());
        return false;
    }

    if (::MoveFileExA(lTempPath.c_str(), lpcPath, MOVEFILE_REPLACE_EXISTING) == 0)
    {
        ::DeleteFileA(lTempPath.c_str());
        return false;
    }
    return true;
}

bool WriteContainer(const char* lpacName,
                    const void* lpImage, u32 luImageSize,
                    const void* lpMugshots, u32 luMugshotsSize,
                    const char* lpacTitle, const char* lpacDescription)
{
    char lacPath[MAX_PATH];
    if (!BuildContainerPath(lacPath, sizeof(lacPath), lpacName)) return false;
    try
    {
        return WriteContainerAtPath(MemcardDir(), lacPath, lpImage, luImageSize,
                                    lpMugshots, luMugshotsSize, lpacTitle, lpacDescription);
    }
    catch (const std::exception&) { return false; }
}

namespace
{
    struct PendingWrite
    {
        WriteTicket muTicket = 0;
        std::string mDirectory, mPath, mTitle, mDescription;
        std::vector<u8> mImage, mMugshots;
        WriteReport mReport;
        bool mbComplete = false, mbSucceeded = false;
    };

    bool CaptureWrite(PendingWrite& lrWrite, const char* lpcName,
        const void* lpImage, u32 luImageBytes, const void* lpMugshots, u32 luMugshotBytes,
        const char* lpcTitle, const char* lpcDescription)
    {
        char lacRelative[MAX_PATH];
        if (!lpImage || !luImageBytes || !BuildContainerPath(lacRelative, sizeof(lacRelative), lpcName))
            return false;
        // Capture the directory before the caller can change process working directory.
        const DWORD luRequired = GetFullPathNameA(MemcardDir(), 0, nullptr, nullptr);
        if (!luRequired) return false;
        std::vector<char> lDirectory(luRequired);
        const DWORD luLength = GetFullPathNameA(MemcardDir(), luRequired, lDirectory.data(), nullptr);
        if (!luLength || luLength >= luRequired) return false;
        lrWrite.mDirectory.assign(lDirectory.data(), luLength);
        lrWrite.mPath = lrWrite.mDirectory + "\\" + lpcName + KACP_SAVE_EXTENSION;
        lrWrite.mTitle = lpcTitle ? lpcTitle : "";
        lrWrite.mDescription = lpcDescription ? lpcDescription : "";
        lrWrite.mImage.assign(static_cast<const u8*>(lpImage), static_cast<const u8*>(lpImage) + luImageBytes);
        if (lpMugshots && luMugshotBytes)
            lrWrite.mMugshots.assign(static_cast<const u8*>(lpMugshots), static_cast<const u8*>(lpMugshots) + luMugshotBytes);
        CopyStringField(lrWrite.mReport.macName, sizeof(lrWrite.mReport.macName), lpcName);
        lrWrite.mReport.muImageSize = luImageBytes;
        lrWrite.mReport.muMugshotsSize = static_cast<u32>(lrWrite.mMugshots.size());
        return true;
    }

    // One file writer preserves submission order, including saves to the same
    // container. Its mutex protects queue/state only and never covers file IO.
    class AsyncWriter
    {
        static constexpr size_t KU_PENDING_LIMIT = 32;
        std::mutex mMutex;
        std::condition_variable mChanged;
        std::thread mThread;
        std::unordered_map<WriteTicket, std::unique_ptr<PendingWrite>> mWrites;
        std::deque<PendingWrite*> mQueue;
        WriteTicket muNextTicket = 1;
        bool mbStopping = false;

        void Run()
        {
            for (;;)
            {
                PendingWrite* lpWrite;
                {
                    std::unique_lock<std::mutex> lLock(mMutex);
                    mChanged.wait(lLock, [&] { return mbStopping || !mQueue.empty(); });
                    if (mQueue.empty()) return;
                    lpWrite = mQueue.front();
                    mQueue.pop_front();
                }
                const auto lBegin = std::chrono::steady_clock::now();
                bool lbSucceeded = false;
                try
                {
                    lbSucceeded = WriteContainerAtPath(lpWrite->mDirectory.c_str(), lpWrite->mPath.c_str(),
                        lpWrite->mImage.data(), static_cast<u32>(lpWrite->mImage.size()),
                        lpWrite->mMugshots.data(), static_cast<u32>(lpWrite->mMugshots.size()),
                        lpWrite->mTitle.c_str(), lpWrite->mDescription.c_str());
                }
                catch (const std::exception&) {}
                // Release the large snapshots on the IO worker as well. Polling
                // completion on the game thread only copies the small report.
                std::vector<u8>().swap(lpWrite->mImage);
                std::vector<u8>().swap(lpWrite->mMugshots);
                const double lfMilliseconds = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - lBegin).count();
                {
                    std::lock_guard<std::mutex> lLock(mMutex);
                    lpWrite->mbSucceeded = lbSucceeded;
                    lpWrite->mReport.mfWriteMilliseconds = lfMilliseconds;
                    lpWrite->mbComplete = true;
                }
                // A poller may destroy the completed request after the unlock.
                // Do not touch lpWrite again, including while notifying waiters.
                mChanged.notify_all();
            }
        }

    public:
        ~AsyncWriter() { Stop(); }
        bool Start()
        {
            std::lock_guard<std::mutex> lLock(mMutex);
            if (mbStopping) return false;
            if (mThread.joinable()) return true;
            try { mThread = std::thread([this] { Run(); }); }
            catch (const std::exception&) { return false; }
            return true;
        }
        WriteTicket Submit(std::unique_ptr<PendingWrite> lpWrite)
        {
            if (!Start()) return 0;
            std::lock_guard<std::mutex> lLock(mMutex);
            if (mbStopping || !muNextTicket || mWrites.size() >= KU_PENDING_LIMIT) return 0;
            const WriteTicket luTicket = muNextTicket++;
            lpWrite->muTicket = luTicket;
            auto lInserted = mWrites.emplace(luTicket, std::move(lpWrite));
            try { mQueue.push_back(lInserted.first->second.get()); }
            catch (...) { mWrites.erase(lInserted.first); throw; }
            mChanged.notify_one();
            return luTicket;
        }
        EWriteStatus Poll(WriteTicket luTicket, WriteReport& lrReport)
        {
            std::lock_guard<std::mutex> lLock(mMutex);
            const auto lIt = mWrites.find(luTicket);
            if (lIt == mWrites.end()) return E_WRITE_UNKNOWN;
            if (!lIt->second->mbComplete) return E_WRITE_PENDING;
            lrReport = lIt->second->mReport;
            const bool lbSucceeded = lIt->second->mbSucceeded;
            mWrites.erase(lIt);
            return lbSucceeded ? E_WRITE_SUCCEEDED : E_WRITE_FAILED;
        }
        void Finish(WriteTicket luTicket)
        {
            std::unique_lock<std::mutex> lLock(mMutex);
            mChanged.wait(lLock, [&] {
                const auto lIt = mWrites.find(luTicket);
                return lIt == mWrites.end() || lIt->second->mbComplete;
            });
            mWrites.erase(luTicket);
        }
        void Stop()
        {
            {
                std::lock_guard<std::mutex> lLock(mMutex);
                mbStopping = true;
            }
            mChanged.notify_all();
            if (mThread.joinable()) mThread.join();
        }
    };
    AsyncWriter& GetAsyncWriter()
    {
        static AsyncWriter sWriter;
        return sWriter;
    }
}

bool InitializeAsyncWrites()
{
    // Resolve the harness directory on the caller before any worker can use it.
    (void)MemcardDir();
    return GetAsyncWriter().Start();
}
WriteTicket BeginWriteContainer(const char* lpacName,
    const void* lpImage, u32 luImageSize, const void* lpMugshots, u32 luMugshotsSize,
    const char* lpacTitle, const char* lpacDescription)
{
    try
    {
        auto lWrite = std::make_unique<PendingWrite>();
        if (!CaptureWrite(*lWrite, lpacName, lpImage, luImageSize, lpMugshots,
                          luMugshotsSize, lpacTitle, lpacDescription)) return 0;
        return GetAsyncWriter().Submit(std::move(lWrite));
    }
    catch (const std::exception&) { return 0; }
}
EWriteStatus PollWriteContainer(WriteTicket luTicket, WriteReport& lrReport)
{
    return GetAsyncWriter().Poll(luTicket, lrReport);
}
void FinishWriteContainer(WriteTicket luTicket)
{
    if (luTicket) GetAsyncWriter().Finish(luTicket);
}


EContainerReadResult ReadContainer(const char* lpacName,
                                   void* lpImage, u32 luImageSize,
                                   void* lpMugshots, u32 luMugshotsSize)
{
    if (lpImage == 0 || luImageSize == 0)
    {
        return E_CONTAINERREAD_MISMATCH;
    }
    if (lpMugshots == 0)
    {
        luMugshotsSize = 0;
    }

    char lacPath[MAX_PATH];
    if (!BuildContainerPath(lacPath, sizeof(lacPath), lpacName))
    {
        return E_CONTAINERREAD_MISSING;
    }

    HANDLE lhFile = ::CreateFileA(lacPath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, 0);
    if (lhFile == INVALID_HANDLE_VALUE)
    {
        return E_CONTAINERREAD_MISSING;
    }

    ContainerHeader lHeader;
    if (!ReadAll(lhFile, &lHeader, static_cast<u32>(sizeof(lHeader))))
    {
        ::CloseHandle(lhFile);
        return E_CONTAINERREAD_CORRUPT;
    }
    if (lHeader.muMagic != KU_CONTAINER_MAGIC || lHeader.muVersion != KU_CONTAINER_VERSION)
    {
        ::CloseHandle(lhFile);
        return E_CONTAINERREAD_CORRUPT;
    }
    // The stored payload must be exactly the running build's layout; a size drift means
    // a different build wrote it (the console analogue is the version-manifest reject).
    if (lHeader.muImageSize != luImageSize ||
        (luMugshotsSize != 0 && lHeader.muMugshotsSize != 0 &&
         lHeader.muMugshotsSize != luMugshotsSize))
    {
        ::CloseHandle(lhFile);
        return E_CONTAINERREAD_MISMATCH;
    }

    const bool lbReadMugshots = luMugshotsSize != 0 && lHeader.muMugshotsSize == luMugshotsSize;

    if (!ReadAll(lhFile, lpImage, luImageSize) ||
        (lbReadMugshots && !ReadAll(lhFile, lpMugshots, luMugshotsSize)))
    {
        ::CloseHandle(lhFile);
        return E_CONTAINERREAD_CORRUPT;
    }

    u32 luHash = HashBytes(KU_HASH_SEED, lpImage, luImageSize);
    if (lbReadMugshots)
    {
        luHash = HashBytes(luHash, lpMugshots, luMugshotsSize);
    }
    else if (lHeader.muMugshotsSize != 0)
    {
        // A stored mugshot payload the caller did not request still participates in
        // the checksum: stream it through the hash without keeping it.
        u8  laChunk[4096];
        u32 luRemaining = lHeader.muMugshotsSize;
        while (luRemaining > 0)
        {
            const u32 luChunk = luRemaining < static_cast<u32>(sizeof(laChunk))
                                    ? luRemaining : static_cast<u32>(sizeof(laChunk));
            if (!ReadAll(lhFile, laChunk, luChunk))
            {
                ::CloseHandle(lhFile);
                return E_CONTAINERREAD_CORRUPT;
            }
            luHash = HashBytes(luHash, laChunk, luChunk);
            luRemaining -= luChunk;
        }
    }
    ::CloseHandle(lhFile);

    if (luHash != lHeader.muPayloadHash)
    {
        return E_CONTAINERREAD_CORRUPT;
    }
    return E_CONTAINERREAD_OK;
}
}
}
