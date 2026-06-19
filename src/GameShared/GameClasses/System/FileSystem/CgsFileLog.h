#pragma once

// CgsFileLog.h — canonical home for CgsFileSystem::FileLog (+ FileLogEntry and
// the ELogEntryType / ELogEntryMode enums).
// Reconstructed from BURNOUT_X360_ARTIST.XEX (FileLog::FileLog @0x828DCE98) +
// DecFIGS DWARF (CgsFileLog.h, internal PS3 build — used for member names/types
// and enum values only; not offset authority).
//
// MINIMAL SLICE: the only X360-attested function in this build is the
// constructor, which touches *only* the static singleton _mpThis. It does not
// read or write any instance data. So this header establishes the canonical
// home for the public vocabulary (enums + FileLogEntry + the singleton + the
// declared API) and DEFERS the FileLog *instance* layout (the entry ring buffer
// + its head/tail indices and counts), which the ctor never touches. No
// committed code embeds FileLog by value — it is a pointer-only singleton — so
// the deferred instance block costs nothing to other TUs. (Mirrors the
// minimal-slice style of CgsDeviceManager.h.)

#include "types.hpp"
// CgsAssert.h provides CGS_ASSERT (used by the ctor in CgsFileLog.cpp).
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Forward-declared: Construct's signature references this by pointer only, so no
// rw header cascade is pulled in here. The full rw layout lives in the
// RenderWare vendor headers and is not needed for the constructor TU.
namespace rw
{
    struct IResourceAllocator;  // vendor canonical (rwcore_structs.h) declares it `struct`
}

namespace CgsFileSystem
{
    // ---- DWARF CgsFileLog.h:37 ----
    enum ELogEntryType
    {
        E_LOGTYPE_OPEN         = 0,
        E_LOGTYPE_CLOSE        = 1,
        E_LOGTYPE_READ         = 2,
        E_LOGTYPE_WRITE        = 3,
        E_LOGTYPE_FSUPDATE     = 4,
        E_LOGTYPE_LOAD         = 5,
        E_LOGTYPE_UNLOAD       = 6,
        E_LOGTYPE_STREAMDATA   = 7,
        E_LOGTYPE_STREAMOPSIZE = 8,
        E_LOGTYPE_COUNT        = 9
    };

    // ---- DWARF CgsFileLog.h:52 ----
    enum ELogEntryMode
    {
        E_LOGMODE_BEGIN           = 0,
        E_LOGMODE_END             = 1,
        E_BUNDLELOG_BEGINLOAD     = 2,
        E_BUNDLELOG_HEADERLOADED  = 3,
        E_BUNDLELOG_ENTRIESLOADED = 4,
        E_BUNDLELOG_DATASTREAMED  = 5,
        E_BUNDLELOG_DATAFINISHED  = 6,
        E_BUNDLELOG_ENDLOAD       = 7,
        E_LOGMODE_COUNT           = 8
    };

    // ---- DWARF CgsFileLog.h:75-84 ----
    // One recorded file-system log event. POD; written by the (deferred) Log*
    // methods and read back out by ReadLogs.
    struct FileLogEntry
    {
        static const s32 KI_MAX_FILENAME_LENGTH = 256;

        ELogEntryType meType;                              // :77
        ELogEntryMode meMode;                              // :78
        char          macFileName[KI_MAX_FILENAME_LENGTH]; // :79
        u64           muTime;                              // :80
        u64           muPosition;                          // :81
        s32           miPriority;                          // :82
        u64           muSize;                              // :83
        s32           miIndex;                             // :84
    };

    // ---- DWARF CgsFileLog.h:96 ----
    // The single global file-system logger. Constructed once; registered through
    // the static singleton _mpThis by the constructor.
    class FileLog
    {
    public:
        // X360-attested @0x828DCE98. Registers this as the one global logger.
        FileLog();                                             // :99

        // ---- Lifecycle (DWARF CgsFileLog.h:105 / :109) -------------------
        // DECLARE-ONLY: bodies are separate TUs.
        void Construct(s32 liLength, rw::IResourceAllocator* lpAllocator); // :105
        void Destruct();                                       // :109

        // ---- Logging API (DWARF CgsFileLog.h:112-160) --------------------
        // DECLARE-ONLY: each body is its own TU; only the ctor is defined here.
        // Parameter NAMES are inferred (DWARF gives types only); signatures (types)
        // are DWARF-exact.
        void LogOpenBegin(const char* lpcFileName, s32 liArg1, s32 liArg2);     // :112
        void LogOpenEnd(const char* lpcFileName, s32 liArg);                    // :115
        void LogReadBegin(const char* lpcFileName, u64 luPosition, u64 luSize, s32 liPriority); // :118
        void LogReadEnd(const char* lpcFileName, u64 luSize, s32 liArg);        // :121
        void LogWriteBegin(const char* lpcFileName, u64 luPosition, u64 luSize, s32 liPriority); // :124
        void LogWriteEnd(const char* lpcFileName, u64 luSize, s32 liArg);       // :127
        void LogCloseBegin(const char* lpcFileName, s32 liArg);                 // :130
        void LogCloseEnd(const char* lpcFileName, s32 liArg);                   // :133
        void LogLoadEvent(const char* lpcFileName, ELogEntryMode leMode);       // :136
        void LogUnloadBegin(const char* lpcFileName);                          // :139
        void LogUnloadEnd(const char* lpcFileName);                            // :142
        void LogFSUpdateBegin();                                               // :145
        void LogFSUpdateEnd();                                                 // :148
        void LogStreamData(u32 luArg, s32 liArg);                              // :151
        void LogStreamOpSizeStart(u32 luArg, s32 liArg);                       // :154
        void LogStreamOpSizeEnd(u32 luArg, s32 liArg);                         // :157

        // Copy up to liBufferLength recorded entries into lpaBuffer; returns the
        // number copied. (DWARF CgsFileLog.h:160)
        s32 ReadLogs(FileLogEntry* lpaBuffer, s32 liBufferLength);

    private:
        // ---- static singleton (DWARF CgsFileLog.h:176) -------------------
        // The dword_830EA3F8 the constructor writes. Defined once in
        // CgsFileLog.cpp; external linkage so every using TU sees the one
        // instance. Kept under the DWARF name `_mpThis`. The ctor is a member,
        // so it writes this private static directly (no friend needed).
        static FileLog* _mpThis;

        // ---- private helpers (DWARF CgsFileLog.h:179 / :182) -------------
        // DECLARE-ONLY: bodies are separate TUs. The ring-buffer push/pop that
        // the Log* methods and ReadLogs route through.
        bool PushEntry(const FileLogEntry* lpEntry);          // :179
        bool PopEntry(FileLogEntry* lpEntry);                 // :182

        // ---- DEFERRED INSTANCE MEMBERS -----------------------------------
        // The FileLog instance state is a ring buffer of FileLogEntry plus its
        // head/tail indices and a count (the storage the deferred Construct
        // allocates via rw::IResourceAllocator and that PushEntry/PopEntry walk).
        // The constructor — the only X360-attested function in this build —
        // touches NONE of it, and no committed code embeds FileLog by value
        // (it is a pointer-only singleton), so the exact capacity/layout is
        // deferred until the Log*/Construct TUs are worked. The future work
        // (extending Construct/PushEntry/PopEntry/ReadLogs) MUST flesh this
        // block out IN THIS SAME FILE rather than forking the class:
        //   FileLogEntry* mpaEntries;   // ring storage (Construct-allocated)
        //   s32           miCapacity;   // ring length (Construct's liLength)
        //   s32           miHead;       // PopEntry read cursor
        //   s32           miTail;       // PushEntry write cursor
        //   s32           miCount;      // live entry count
        // (Names/types above are inferred; recover the exact members + a sizeof
        //  check from the X360 pseudocode of Construct/PushEntry when worked.)
    };

} // namespace CgsFileSystem
