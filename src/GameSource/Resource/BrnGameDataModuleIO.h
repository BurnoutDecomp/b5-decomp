#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                 // CgsModule::IOBuffer base + IsBufferLockedFor{Reading,Writing}()
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"      // BrnResource::GameDataIO::RequestInterface<N>
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"     // BrnResource::GameDataIO::AllocatorList (OutputBuffer member)

namespace rw { namespace core { struct GeneralResourceAllocator; } }   // OutputBuffer::GetAllocator return type

// BrnResource::GameDataIO::InputBuffer / OutputBuffer -- the per-frame IO payload buffers the
// game-data streaming module (BrnResource::GameDataModule) exchanges with the rest of the game.
// Both derive the committed CgsModule::IOBuffer (a status-flag-guarded read/write lock base: a
// 1-byte FlagSet8 padded to 4 bytes).
//
// DWARF home: GameSource/Resource/BrnGameDataModuleIO.h
// (references/DecFIGS/dwarfdump/GameSource/Resource/BrnGameDataModuleIO.h). The DWARF declares:
//   struct InputBuffer : public IOBuffer {
//     RequestInterface<32768>          mRequestInterface;          // h:123
//     AttribSysRequestInterface<32768> mAttribSysRequestInterface; // h:127
//     CgsGraphics::Im2dRenderBuffer*   mpDebug2dRenderBuffer;      // h:130
//     ... Construct/Prepare/Release/Destruct/Clear,
//         GetRequestInterface() const (h:99) / GetRequestInterface() (h:103),
//         GetAttribSysRequestInterface()/SetIm2dDebugRenderBuffer()/GetIm2dDebugRenderBuffer() ...
//   };
//
// SOURCES (X360 ARTIST): the two GetRequestInterface overloads bodied in BrnGameDataModuleIO.cpp:
//   GetRequestInterface() const  @ 0x82663F90  (asserts read-lock,  fires at h:211)
//   GetRequestInterface()        @ 0x823B1788  (asserts write-lock, fires at h:218)
// Both return &mRequestInterface, which (with the 4-byte IOBuffer base) lands at this+4 -- matching
// the asm's `return a1 + 4`.
//
// FLAG (MINIMAL SLICE): only the members up to and including mRequestInterface are declared here --
// that is all the two GetRequestInterface overloads (this pass) need. The DWARF's trailing members
// (mAttribSysRequestInterface: AttribSysRequestInterface<32768>, NOT yet committed anywhere; and
// mpDebug2dRenderBuffer: CgsGraphics::Im2dRenderBuffer*) plus all the other accessors / lifecycle
// methods (Construct/Prepare/Release/Destruct/Clear, the AttribSys/Im2d getters and setters) and the
// entire OutputBuffer (AllocatorList + FileSystemStatusInterface) are DEFERRED to their own TUs and
// intentionally omitted. This is semantic parity (real names/types/order for the declared prefix),
// NOT a byte-exact full object. GROW this header additively when the deferred members/methods land --
// do NOT fork it.
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

        // GetRequestInterface() const @ 0x82663F90 / fires assert at DWARF h:211 -- assert the buffer
        // is read-locked, then hand back the embedded request interface (read side). Returns this+4
        // (= &mRequestInterface).
        const RequestInterface<knRequestInterfaceQueueSize>* GetRequestInterface() const;   // DWARF h:99

        // GetRequestInterface() @ 0x823B1788 / fires assert at DWARF h:218 -- assert the buffer is
        // write-locked (the writer builds requests into the interface), then hand back the embedded
        // request interface (write side). Returns this+4 (= &mRequestInterface).
        RequestInterface<knRequestInterfaceQueueSize>* GetRequestInterface();               // DWARF h:103

    private:
        // DWARF h:123 -- the only member declared in this slice. With the committed CgsModule::IOBuffer
        // base (a single 1-byte FlagSet8 status field, padded to 4), mRequestInterface lands at this+4
        // -- matching the asm's `return a1 + 4` in both overloads. FLAG: the trailing
        // mAttribSysRequestInterface (h:127) + mpDebug2dRenderBuffer (h:130) are DEFERRED (see header
        // note); the +4 offset is implied by the base size, not asserted byte-exactly here.
        RequestInterface<knRequestInterfaceQueueSize> mRequestInterface;
    };

    // BrnResource::GameDataIO::OutputBuffer : public IOBuffer -- the GameData module's per-frame output
    // payload. It owns the AllocatorList (the bank->allocator registry that GameDataModule::Create-
    // Allocators populates) and the FileSystemStatusInterface. The loading-screen module loads read the
    // module's RW general allocator through OutputBuffer::GetAllocator (e.g. LoadSoundModule @0x823E75A8:
    // `GameDataIO::OutputBuffer::GetAllocator(outputBuffer)` -> passes it to SoundModule::Construct).
    //
    // STRUCTURAL SLICE: the type + the AllocatorList member + GetAllocator are defined here so the module
    // loads can name them; mFileSystemStatusInterface (DWARF, deferred) is omitted (minimal slice). The
    // AllocatorList stays EMPTY (every bank unregistered) until GameDataModule::CreateAllocators (still a
    // stub) populates it + the module gives Prepare a real output-buffer instance (currently Prepare(0,0)
    // -- null buffers) -- that boot-critical population is the next slice. GROW additively; do NOT fork.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        void Construct() { mAllocatorList.Construct(); }

        // Return the module's RW general resource allocator. X360: OutputBuffer::GetAllocator(this)
        // returns the GameData general allocator the module loads construct from. The bank is the
        // GameData root general-heap bank; pinned to bank 0 here pending the CreateAllocators memory-map
        // population (which assigns the real bank ids). Asserts (via the AllocatorList accessor) until
        // CreateAllocators registers that bank -- so this must not be called before the population slice.
        rw::core::GeneralResourceAllocator* GetAllocator() const
        {
            return mAllocatorList.GetRWGeneralResourceAllocator(0);
        }

        AllocatorList& GetAllocatorList() { return mAllocatorList; }

    private:
        AllocatorList mAllocatorList;
        // FileSystemStatusInterface mFileSystemStatusInterface;  // DWARF -- deferred (minimal slice)
    };
}
}
