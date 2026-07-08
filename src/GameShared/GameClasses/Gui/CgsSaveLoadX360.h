#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"   // CgsGui::ESaveLoadCif, ContentInformationFileInterface, MessageDisplay, SaveLoadTaskResultHandler

#include <Windows.h>   // HANDLE / OVERLAPPED (the X360 stream/async ops wrap Win32; host Win32 on PC)

namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
class LanguageManager;
class SystemUserProfile;

// RealmcIface - the Realm memory-card (mc) interface family the X360 save/load path drives.
// These are the SDK-shaped types the in-scope SaveLoadSystem functions call into. The memory-card
// interface object itself (RealmcIface::MemcardInterface -- the four vtable slots this front-end
// dispatches: Update @+0x08, WriteSave @+0x2C, CheckSave @+0x30, SetActive @+0x34) is homed in its
// own SDK TU; include it so the type is defined once.
#include "SDKs/Realmc/RealmcMemcardInterface.h"   // RealmcIface::MemcardInterface

namespace RealmcIface
{
    // Forward-declared SDK helpers used by Save/Prepare; full bodies live in their own TUs.
    class GameInfo;
    class MemcardInterfaceFactory;
}

// CgsGui::XenonFileInputStream - a read-only file input stream over a Win32 file HANDLE,
// used by the Xenon (X360) save/load path (CgsGui::SaveLoadSystem::Prepare opens a stream
// and reads its header through this). Recovered from the X360 ARTIST binary:
//   GetSize @ 0x8284BD88   Read @ 0x8284BE28
// (assert sites cite the X360 build's gui/CgsSaveLoadX360.h).
//
// Layout (X360 authoritative, from the GetSize/Read member loads):
//   mhFile  [+0x0]  HANDLE  -- the open file handle; INVALID_HANDLE_VALUE (-1) until/if open
//                              fails (both methods assert mhFile != -1, "File open failed!").
//   miSize  [+0x4]  s32     -- the file size (GetSize returns `this[1]` == +4).
namespace CgsGui
{
    class XenonFileInputStream
    {
    public:
        // X360 0x8284BD88. Asserts the file is open (mhFile != INVALID_HANDLE_VALUE),
        // returns the cached file size.
        s32 GetSize() const;

        // X360 0x8284BE28. Asserts the file is open, then reads luBytesToRead bytes from the
        // handle into lpBuffer via Win32 ReadFile (lpOverlapped == NULL). On a ReadFile
        // failure (returns FALSE) it fires "File read failed!". Returns the number of bytes
        // actually read.
        s32 Read(void* lpBuffer, u32 luBytesToRead);

    private:
        HANDLE mhFile; // +0x0 -- open Win32 file handle (-1 == INVALID_HANDLE_VALUE)
        s32    miSize; // +0x4 -- cached file size in bytes
    };

    // CgsGui::MemcardAllocator - a polymorphic allocator front-end the X360 (Xenon) save/load
    // path hands to the memory-card I/O so its dynamic buffers come out of a dedicated
    // HeapMalloc rather than the global heap. Recovered from the X360 ARTIST binary (assert
    // sites cite gui/CgsSaveLoadX360.h):
    //   Alloc                      @ 0x827DBB08  -> mpHeap->Malloc(luSize, /*align*/4); asserts non-null
    //   Free                       @ 0x827DBBC8  -> mpHeap->Free(lpBlock) (tail call)
    //   `vector deleting destructor'@ 0x827DBC00  -> set vtable, then operator delete if (flag & 1)
    //
    // Layout (X360 authoritative, from the member loads):
    //   vptr   [+0x0]  -- leading vtable pointer (the object is polymorphic; Alloc/Free are
    //                     virtuals, and the vector-deleting-destructor writes off_8200F5B4 here).
    //   mpHeap [+0x4]  HeapMalloc* -- the heap every allocation is serviced from (`lwz r3,4(r3)`
    //                     loads it as the `this` for both HeapMalloc::Malloc and HeapMalloc::Free).
    // The KI_DEFAULT_ALIGNMENT(4) the X360 passes to Malloc matches CgsMemory::HeapMalloc's own
    // default alignment. Alloc/Free are declared virtual (X360 dispatch + the destructor's vtable
    // store); the host vtable slot reproduces the +0x0 vptr without a raw offset cast.
    class MemcardAllocator
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;   // X360 `li r5,4` alignment argument

        virtual ~MemcardAllocator();

        // X360 0x827DBB08. Allocate luSize bytes from mpHeap (4-byte aligned). Asserts the
        // HeapMalloc::Malloc result is non-null ("CgsMemory::HeapMalloc::Malloc failed.").
        virtual void* Alloc(s32 luSize);
        // X360 0x827DBBC8. Free a block previously returned by Alloc (forwards to mpHeap->Free).
        virtual void  Free(void* lpBlock);

    private:
        CgsMemory::HeapMalloc* mpHeap;   // +0x4 -- the heap allocations are serviced from
    };

    // CgsGui::SaveLoadSystem - the Xenon (X360) save/load front-end. Drives the memory-card
    // I/O (RealmcIface::MemcardInterface), a Win32 overlapped async op, a mugshot/image
    // buffer, the on-screen MessageDisplay prompts, and reports completion to a
    // SaveLoadTaskResultHandler. It IS the ContentInformationFileInterface the memory-card
    // layer queries for its CIF blobs (LoadCifFile et al). Reconstructed from the X360 ARTIST
    // binary; the assert sites cite gui/CgsSaveLoadX360.cpp.
    //
    // The byte offsets in the comments below are X360-AUTHORITATIVE (32-bit target): they are the
    // exact displacements the ASM loads/stores (mpMessageDisplay @this+0x134, mpMemcardInterface
    // @this+0x184, the overlapped @this+0x228, the autosave-icon flag @this+0x248, ...). Members
    // are declared in that order; gaps the in-scope 12 functions don't touch are reserved with
    // explicit padding. NOTE: this project compiles host x64, where pointers are 8 bytes, so the
    // realised host offsets are NOT byte-identical to the 32-bit X360 ones -- the offset comments
    // document the X360 layout (the source of truth for which member each ASM displacement names),
    // not the host struct layout. The PS3 DecFIGS DWARF lists a different (PS3-only) member set
    // (Thread/SaveDataSystem/sys_memory_container_t) that does NOT match these X360 offsets, so it
    // is used for names/intent only, never as offset authority.
    //
    // NOTE on the 0x194/0x198/0x19C region (three distinct words; the X360 stores prove the
    // split):
    //   +0x194  a single word. ShowMessage's 8-byte `std r10,0x194` writes the option-handler
    //           func into this word AND zeroes the next word (+0x198). Prepare's
    //           `stw r27,0x194` stores the SystemUserProfile pointer here. These two uses alias
    //           the same word by design (one is overwritten by the other at runtime), so the
    //           word is modelled once (mField194) and aliased with mpSystemUserProfile.
    //   +0x198  the ACTIVE member-function-pointer's FUNC word, and +0x19C its this-DELTA word
    //           (mMessageDisplayOptionFunc). Construct/Load/BootupShowAutosaveWarning write the
    //           pair via `std ...,0x198` ({func@+0x198, delta@+0x19C}); HandleOption null-checks
    //           and dispatches ONLY through this pair (`lwz r11,0x198`).
    // ShowMessage's +0x194 store's second word and this +0x198 func word alias the same address
    // by design -- ShowMessage zeroing +0x198 while writing +0x194 is the binary's actual
    // behaviour, reproduced here (no extra guard).
    class SaveLoadSystem : public ContentInformationFileInterface
    {
    public:
        // CgsSaveLoadX360.cpp:155. X360 0x8284C050. Wire up the message display, language
        // manager, the wide-char title (lpcTitle), the save/content file paths, the image
        // dimensions and the extra-file (mugshot) size. Asserts lpMessageDisplay/lpLanguageManager
        // are non-null. luExtraFilesSizeBytes is the last (28th) argument.
        void Construct(MessageDisplay* lpMessageDisplay, LanguageManager* lpLanguageManager,
                       const char* lpcTitle, const char* lpcContentInfoFilePath,
                       const char* lpcSaveFilePath, s32 liImageWidth, s32 liImageHeight,
                       u32 luExtraFilesSizeBytes);

        // CgsSaveLoadX360.cpp:214. X360 0x82851FC0. Create the memory-card interface, allocate
        // the mugshot buffer from the linear allocator, and slurp the content-information file
        // into a buffer via XenonFileInputStream. Returns true.
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, CgsMemory::LinearMalloc* lpLinearMalloc,
                     SystemUserProfile* lpSystemUserProfile);

        // CgsSaveLoadX360.cpp:255. X360 0x8284C158. Free the content-information file buffer
        // (through the shared ContentInformation provider). Returns true.
        bool Release();

        // X360 0x8284C4E0. Pump the overlapped async op; if the memory-card interface exists,
        // forward Update to it.
        void Update();

        // X360 0x82859B70. Begin a load: bind the result handler, push the metadata, prompt the
        // user (SAVELOAD_CONFIRM_LOAD) routing the choice to LoadHandleConfirmLoad.
        void Load(SaveLoadTaskResultHandler* lpResultHandler, const void* lpMetadata);

        // X360 0x82856040. Write the current save (title + mugshot entries) to the memory card.
        void Save();

        // X360 0x828521E0. Copy each requested image's mugshot buffer into the caller's image
        // file records, then report the load result.
        void LoadImageFiles(SaveLoadTaskResultHandler* lpResultHandler, s32 liNumberOfImageFiles,
                            const void* lpImageFile);

    private:
        // X360 0x8284C240. Copy the metadata's title (wide + ascii) and the save-info comment
        // strings into the fixed buffers (asserting each fits) and cache the two size fields.
        void SetMetadata(const void* lpMetadata, const void* lpSaveInfo);

        // X360 0x8284CA78. Show a yes/no/etc. message, routing the user's choice back through
        // HandleMemcardOption. The X360 forwards (handler, this-4, a2, a4, a3) -- the a3/a4
        // arguments are swapped on the way out -- and MessageDisplay::ShowMessage's true order is
        // (handler, message, options, count). So this method's own params are
        // (message=a2, count=a3, options=a4); the forward reproduces the swap.
        void ShowMessage(const char* lpcMessage, u32 luNumberOfOptions, const char** lpacOptions);

        // X360 0x8284CA18. Toggle the autosave icon (only when its desired visibility differs
        // from the cached state).
        void ShowAutosaveIcon(bool lbVisible);

        // X360 0x8284C730. The MessageDisplay::OptionHandler entry point: hide the prompt and
        // dispatch to the pending message-display-option member function. (Reached through the
        // embedded OptionHandler sub-object the engine offsets to as `this - 4` at the call site.)
        virtual void HandleOption(u32 luOption);

        // X360 0x828599A0. Show the autosave warning prompt (SAVELOAD_AUTOSAVE_WARNING), routing
        // confirmation to BootupStart.
        void BootupShowAutosaveWarning();

        // ---- X360 layout (byte offsets are authoritative) -------------------------------------
        // +0x00  : inherited ContentInformationFileInterface vptr
        u8   mPad04[0x08 - 0x04];                 // +0x04 .. +0x07

        char macSaveInfoComment[32];              // +0x08  SaveInfo comment (strncpy, 32)
        char macSaveInfoDescription[256];         // +0x28  SaveInfo description (strncpy, 256)

        u64  mField128;                           // +0x128 (Construct zeroes, 8 bytes)

        MessageDisplay* mpActiveMessageDisplay;   // +0x130 the display driving the live operation
        MessageDisplay* mpMessageDisplay;         // +0x134 the display passed to Construct
        wchar_t macwTitle[32];                    // +0x138 wide title (ConvertAsciiToWideCharSafe)

        const char* mpacContentInfoFilePath;      // +0x178 (Construct arg a5 == lpcContentInfoFilePath)
        const char* mpacSaveFilePath;             // +0x17C save-file path (CreateFileA)
        bool   mbField180;                        // +0x180 (Construct sets true)
        bool   mbField181;                        // +0x181 (Construct sets false)
        u8     mPad182;                           // +0x182 alignment
        bool   mbField183;                        // +0x183 (Construct sets false)
        RealmcIface::MemcardInterface* mpMemcardInterface; // +0x184
        LanguageManager* mpLanguageManager;       // +0x188
        // +0x18C : ContentInformation provider (a vtable object). Its address (this+0x18C) is
        // published to the file-scope provider pointer in Prepare and used through its vtable
        // (Alloc @+8 / Free @+12). Modelled as its leading vptr; the in-scope code only takes
        // its address and calls those two slots.
        void*  mpContentInfoVptr;                 // +0x18C ContentInformation vptr (provider)
        CgsMemory::HeapMalloc* mpHeapMalloc;      // +0x190 (Prepare arg)

        // +0x194 : a single word, used two ways at different times (they alias the same address
        //          on purpose -- see the region note above). ShowMessage stores the
        //          HandleMemcardOption func word here (and its 8-byte store zeroes +0x198);
        //          Prepare stores the SystemUserProfile pointer here.
        union
        {
            u32 muField194;                       // +0x194 (ShowMessage's func word)
            SystemUserProfile* mpSystemUserProfile; // +0x194 (Prepare)
        };
        // +0x198 : the ACTIVE pending-option member-function pointer { func @+0x198, delta @+0x19C }.
        //          Construct/Load/BootupShowAutosaveWarning store it via `std ...,0x198`;
        //          HandleOption null-checks and dispatches through it. Modelled as two 4-byte
        //          words to keep the X360's natural 4-byte alignment (a u64 would force 8-byte
        //          alignment and shift later members).
        struct { u32 muFunc; u32 muDelta; } mMessageDisplayOptionFunc; // +0x198 / +0x19C

        wchar_t macwMetadataTitle[32];            // +0x1A0 wide metadata title (ConvertAscii dest)

        char   macTitle[32];                      // +0x1E0 metadata title (strncpy, 32)

        u32    muSaveDataSizeKb;                  // +0x200 (metadata +48)
        u32    muGameDataSizeKb;                  // +0x204 (metadata +52)
        s32    miContentInfoFileSize;             // +0x208 content-info file size
        void*  mpContentInfoFileBuffer;           // +0x20C content-info file buffer
        s32    miField210;                        // +0x210 (Construct sets -1)
        s32    miImageWidth;                      // +0x214 (Construct arg a7)
        s32    miImageHeight;                     // +0x218 (Construct arg a8)
        s32    miExtraFilesSizeBytes;             // +0x21C (Construct last arg)
        void*  mpMugshotBufferData;               // +0x220 mugshot buffer (LinearMalloc)
        u8     mbAsyncOpState;                    // +0x224 overlapped-op state (1 == in flight)
        u8     mPad225[0x228 - 0x225];            // +0x225 .. +0x227
        u8     maOverlapped[0x248 - 0x228];       // +0x228 X360 XOVERLAPPED (XGetOverlappedResult)
        bool   mbAutosaveIconVisible;             // +0x248 cached autosave-icon visibility
        u8     mPad249[0x24C - 0x249];            // +0x249 .. +0x24B
        u8     mField24C;                         // +0x24C (Construct zeroes)
    };
}
