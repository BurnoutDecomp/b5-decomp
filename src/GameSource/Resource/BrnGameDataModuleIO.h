#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                 // CgsModule::IOBuffer base + IsBufferLockedFor{Reading,Writing}()
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"      // BrnResource::GameDataIO::RequestInterface<N>
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"     // BrnResource::GameDataIO::AllocatorList (OutputBuffer member)
// CgsGraphics::Im2dRenderBuffer is a typedef (== CgsGraphics::Im2d), not a struct, so its canonical
// home header is included rather than forward-declared (a `struct Im2dRenderBuffer;` fwd-decl would
// clash with the typedef). InputBuffer::mpDebug2dRenderBuffer is pointer-only.
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer

namespace rw { namespace core { struct GeneralResourceAllocator; } }   // OutputBuffer::GetAllocator return type
namespace BrnResource { rw::core::GeneralResourceAllocator* GetGameDataGeneralAllocator(); }  // allocator-gate leaf

// BrnResource::GameDataIO::InputBuffer / OutputBuffer -- the per-frame IO payload buffers the
// game-data streaming module (BrnResource::GameDataModule) exchanges with the rest of the game.
// Both derive the committed CgsModule::IOBuffer (a status-flag-guarded read/write lock base: a
// 1-byte FlagSet8 padded to 4 bytes).
//
// DWARF home: GameSource/Resource/BrnGameDataModuleIO.h
// (references/DecFIGS/dwarfdump/GameSource/Resource/BrnGameDataModuleIO.h). The DWARF declares:
//   struct InputBuffer : public IOBuffer {
//     RequestInterface<32768>                                     mRequestInterface;          // h:123
//     CgsAttribSys::AttribSysIO::AttribSysRequestInterface<32768> mAttribSysRequestInterface; // h:127
//     CgsGraphics::Im2dRenderBuffer*                              mpDebug2dRenderBuffer;      // h:130
//     ... Construct/Prepare/Release/Destruct/Clear,
//         GetRequestInterface() const (h:99) / GetRequestInterface() (h:103),
//         GetAttribSysRequestInterface() (h:108) / GetAttribSysRequestInterface() const (h:112),
//         SetIm2dDebugRenderBuffer() (h:116) / GetIm2dDebugRenderBuffer() (h:119) ...
//   };
//   struct OutputBuffer : public IOBuffer {
//     const AllocatorList*  mpAllocatorList;     // h:184  (POINTER, verified against the X360 asm)
//     u8                    mbLiveUpdateStatus;  // h:189 storage  (X360 = 1 status byte)
//     ... Construct/Prepare/Release/Destruct/Clear, GetAllocatorList() const (h:167) /
//         SetAllocatorList() (h:171), GetLiveUpdateStatus() / SetLiveUpdateStatus() ...
//   };
//
// SOURCES (X360 ARTIST) bodied in BrnGameDataModuleIO.cpp:
//   InputBuffer::GetRequestInterface() const           @ 0x82663F90  (read,  h:211)  -> this+4
//   InputBuffer::GetRequestInterface()                 @ 0x823B1788  (write, h:218)  -> this+4
//   InputBuffer::GetAttribSysRequestInterface()        @ 0x823B1830  (write, h:226)  -> this+32788
//   InputBuffer::GetAttribSysRequestInterface() const  @ 0x82664038  (read,  h:233)  -> this+32788
//   OutputBuffer::GetAllocatorList() const             @ 0x823B18D8  (read,  h:167)  -> *(this+4)
//   OutputBuffer::SetAllocatorList()                   @ 0x826640E0  (write, h:171)  ->  this+4
//   OutputBuffer::GetLiveUpdateStatus()                @ 0x823B1638  (read)          -> &(this+8)
//   OutputBuffer::SetLiveUpdateStatus()                @ 0x82663E30  (write)         ->  this+8
// The +32788 (0x8014) == base(1, padded to 4) + sizeof(RequestInterface<32768>) (32784 = 32768 payload +
// 16-byte VEQ header), so mAttribSysRequestInterface lands at this+4+32784 = this+32788, matching both
// AttribSys accessors' `return a1 + 32788`.
//
// AttribSysRequestInterface<32768> is CgsAttribSys::AttribSysIO's templated request interface. The DWARF
// .cpp (BrnGameDataModuleIO.cpp:53/55) confirms its Construct/Clear delegate to
// CgsModule::VariableEventQueue<32768,16>::Construct/Clear -- i.e. it embeds one VariableEventQueue<32768,16>
// (== 32768 payload + 16-byte VEQ header == 32784 bytes), exactly like the committed NON-templated
// CgsAttribSys::AttribSysIO::AttribSysRequestInterface (a single AttribSysEventQueue). Because only the
// non-templated form is currently committed (CgsAttribSysModuleIO.h), and no committed templated <N>
// AttribSys type exists yet, mAttribSysRequestInterface is declared here as explicitly-sized opaque storage
// (32784 bytes, DWARF-attested) to keep this header self-consistent and correctly sized WITHOUT inventing a
// template body or forking the committed AttribSys home. FLAG: swap for the real
// CgsAttribSys::AttribSysIO::AttribSysRequestInterface<32768> (and tighten the AttribSys accessor return
// types from void*) once that shared template lands.
//
// OUTPUTBUFFER LAYOUT FIX (verified this batch): the +4 member is a POINTER, not an embedded list.
// GetAllocatorList() const @ 0x823B18D8 emits `lwz r3,4(r28)` (LOAD a pointer VALUE); SetAllocatorList
// @ 0x826640E0 emits `stw r27,4(r28)` (STORE a pointer). An embedded member would compile to
// `addi r3,r28,4` (compute an address). So mpAllocatorList is `const AllocatorList*` @+4, matching the
// DecFIGS DWARF (h:184). The +8 slot: the X360 symbols are Get/SetLiveUpdateStatus and the asm reads/
// writes a SINGLE BYTE there; the PS3 DWARF drifted it into FileSystemStatusInterface (h:189) with
// Get/SetFileSystemStatusInterface -- we follow the X360 asm+symbols (authoritative for the X360 ledger).
//
// FLAG (MINIMAL SLICE / additive header): lifecycle methods (Construct/Prepare/Release/Destruct/Clear) and
// the Im2d setter-getters beyond what the reconstructed accessors need are DEFERRED. Semantic parity (real
// DWARF names/types/order for the declared members), NOT a byte-exact object on the 64-bit host. GROW
// additively; do NOT fork.
namespace BrnResource
{
namespace GameDataIO
{
    // DWARF h:59 -- struct BrnResource::GameDataIO::InputBuffer : public IOBuffer (IOBuffer here is
    // CgsModule::IOBuffer, the committed shared base).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // DWARF h:63/h:69 -- the request-interface queue sizes (both 32768 entries). Declared as the
        // committed in-class constants the template instantiation uses.
        static const s32 knRequestInterfaceQueueSize         = 32768;   // DWARF h:63
        static const s32 kiAttribSysRequestInterfaceQueueSize = 32768;  // DWARF h:69

        // Byte size of AttribSysRequestInterface<32768> == VariableEventQueue<32768,16>: payload +
        // 16-byte VEQ header (DWARF .cpp:53/55). Opaque placeholder size (see header note).
        static const s32 kiAttribSysRequestInterfaceStorageBytes = kiAttribSysRequestInterfaceQueueSize + 16;

        // GetRequestInterface() const @ 0x82663F90 / fires assert at DWARF h:211 -- assert the buffer
        // is read-locked, then hand back the embedded request interface (read side). Returns this+4
        // (= &mRequestInterface).
        const RequestInterface<knRequestInterfaceQueueSize>* GetRequestInterface() const;   // DWARF h:99

        // GetRequestInterface() @ 0x823B1788 / fires assert at DWARF h:218 -- assert the buffer is
        // write-locked (the writer builds requests into the interface), then hand back the embedded
        // request interface (write side). Returns this+4 (= &mRequestInterface).
        RequestInterface<knRequestInterfaceQueueSize>* GetRequestInterface();               // DWARF h:103

        // GetAttribSysRequestInterface() @ 0x823B1830 (fires at DWARF h:226) -- WRITE-lock; returns
        // this+32788 (= &mAttribSysRequestInterface). DWARF h:108. FLAG: DWARF return type is
        // AttribSysRequestInterface<32768>*; opaque storage -> void* (see header note).
        void* GetAttribSysRequestInterface();                                                // DWARF h:108

        // GetAttribSysRequestInterface() const @ 0x82664038 (fires at DWARF h:233) -- READ-lock; returns
        // this+32788 (= &mAttribSysRequestInterface). DWARF h:112. FLAG: DWARF return type is
        // const AttribSysRequestInterface<32768>*; opaque storage -> const void* (see header note).
        const void* GetAttribSysRequestInterface() const;                                    // DWARF h:112

    private:
        // DWARF h:123 -- lands at this+4 (after the 1-byte IOBuffer base padded to 4-byte queue
        // alignment) -- matching the asm's `return a1 + 4` in both GetRequestInterface overloads.
        RequestInterface<knRequestInterfaceQueueSize> mRequestInterface;
        // DWARF h:127 -- lands at this+32788 (= 4 + sizeof(mRequestInterface)). Opaque, size-exact
        // (VariableEventQueue<32768,16> == 32784 bytes; see header note).
        u8 mAttribSysRequestInterface[kiAttribSysRequestInterfaceStorageBytes];
        // DWARF h:130 -- the optional debug Im2d render buffer (SetIm2dDebugRenderBuffer stashes it).
        // Pointer-only; forward-declared type.
        CgsGraphics::Im2dRenderBuffer* mpDebug2dRenderBuffer;
    };

    // BrnResource::GameDataIO::OutputBuffer : public IOBuffer -- the GameData module's per-frame output
    // payload. DWARF home BrnGameDataModuleIO.h:142. It carries a POINTER to the AllocatorList (the
    // bank->allocator registry that GameDataModule::CreateAllocators populates) plus a 1-byte live-update
    // status. The loading-screen module loads read the module's RW general allocator through
    // OutputBuffer::GetAllocator (e.g. LoadSoundModule @0x823E75A8: passes GetAllocatorList() to
    // RootSoundModule::Prepare). Faithful X360 layout (LAYOUT FIX, see header note):
    //   IOBuffer base         (this+0, 4-byte FlagSet8 status)
    //   const AllocatorList*  mpAllocatorList     (this+4, DWARF h:184 -- POINTER, not embedded)
    //   u8                    mbLiveUpdateStatus  (this+8, DWARF h:189 storage; X360 = 1 status byte)
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // DWARF h:147 -- clears the pointer/status members (the old embedded-list Construct was the
        // superseded structural approximation).
        void Construct() { mpAllocatorList = 0; mbLiveUpdateStatus = 0; }

        // Return the module's RW general resource allocator. X360: OutputBuffer::GetAllocator(this)
        // returns the GameData general allocator the module loads construct from; pinned to the single
        // standalone GameData general allocator pending the CreateAllocators memory-map population
        // (which assigns the real bank ids). The faithful path once mpAllocatorList is populated is
        // `mpAllocatorList->GetRWGeneralResourceAllocator(<bank>)`.
        rw::core::GeneralResourceAllocator* GetAllocator() const
        {
            return BrnResource::GetGameDataGeneralAllocator();
        }

        // @ 0x823B18D8 -- read-locked accessor. DWARF h:167. Returns *(this+4) (asm `lwz r3,4(r28)`).
        const AllocatorList* GetAllocatorList() const;

        // @ 0x826640E0 -- write-locked mutator. DWARF h:171. Stores the pointer at this+4
        // (asm `stw r27,4(r28)`).
        void SetAllocatorList(const AllocatorList* lpAllocatorList);

        // @ 0x823B1638 -- read-locked accessor; returns the address of the 1-byte live-update status
        // (asm `addi r3,r28,8`; IDA return type unsigned __int8*). X360 symbol GetLive[UpdateStatus];
        // PS3 DWARF drift = GetFileSystemStatusInterface (h:178).
        u8* GetLiveUpdateStatus();

        // @ 0x82663E30 -- write-locked mutator; copies one status byte in
        // (asm `lbz r11,0(r27); stb r11,8(r28)`). X360 symbol SetLiveUpdateStatus; PS3 DWARF drift =
        // SetFileSystemStatusInterface (h:179).
        void SetLiveUpdateStatus(const u8* lpStatus);

    private:
        const AllocatorList* mpAllocatorList;    // DWARF h:184 -- this+4 (POINTER, not embedded)
        u8                   mbLiveUpdateStatus; // DWARF h:189 storage -- this+8 (X360: 1 status byte)
        // FileSystemStatusInterface follows in the PS3 DWARF; on X360 the +8 slot is a status byte.
    };
}
}
