#include "GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.h"

// FLAG PC-platform leaf (whole TU): the PC realisation of the console memory-card
// storage edge -- see the header banner. Deliberately self-contained (types.hpp +
// Win32 only) so the container round-trip is unit-testable standalone.

#include <cstring>
#include <cstdio>

#include <Windows.h>

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
                                            KACP_MEMCARD_DIR, lpacName, KACP_SAVE_EXTENSION);
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

bool WriteContainer(const char* lpacName,
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

    char lacPath[MAX_PATH];
    if (!BuildContainerPath(lacPath, sizeof(lacPath), lpacName))
    {
        return false;
    }

    // The container directory may not exist on a fresh install; ERROR_ALREADY_EXISTS
    // is the normal case afterwards.
    ::CreateDirectoryA(KACP_MEMCARD_DIR, 0);

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
    char lacTempPath[MAX_PATH];
    const int liWritten = std::snprintf(lacTempPath, sizeof(lacTempPath), "%s.tmp", lacPath);
    if (liWritten <= 0 || static_cast<u32>(liWritten) >= sizeof(lacTempPath))
    {
        return false;
    }

    HANDLE lhFile = ::CreateFileA(lacTempPath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
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
        ::DeleteFileA(lacTempPath);
        return false;
    }

    if (::MoveFileExA(lacTempPath, lacPath, MOVEFILE_REPLACE_EXISTING) == 0)
    {
        ::DeleteFileA(lacTempPath);
        return false;
    }
    return true;
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
