#ifndef BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H
#define BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H

#include <cstddef>   // offsetof (buffer layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base; lock state machine)
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h" // RootOutputBuffer::AttribSysRequestInterface / ::SoundResourceRequestInterface (member types)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<N,16> (LogicOutputBuffer::Construct)
#include <cstring>                                       // memset (trailing-state clear)

// =============================================================================
// BrnSound::Module::Io::LogicOutputBuffer
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h (assert-cited home) +
//   GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// LogicOutputBuffer is the per-frame payload the sound LOGIC module produces for the
// ROOT module to consume. Like every CgsModule::IOBuffer it carries a 1-byte status
// flag at offset 0; the lock-guarded Get* accessors assert the right lock (read-lock
// bit 4 for const readers, write-lock bit 3 for the mutable writer) then return
// &member-at-X360-offset. Same shape as the committed BrnRootSoundModuleIo.h buffers.
//
// This group bodies the TWO wave-7 TU functions (the GetAttribSysRequestInterface
// overloads); the DWARF-listed sibling methods (Construct, GetResourceRequestInterface
// const/non-const) are DECLARED for shape completeness but bodied in their own TU.
//   * GetAttribSysRequestInterface() const (X360 0x82695518) -- read-lock ("Not locked
//     for reading\n"), returns &mAttribSysRequestInterface at this+0x04 (DWARF :69).
//   * GetAttribSysRequestInterface()       (X360 0x82695668) -- write-lock ("Not locked
//     for writing\n"), returns the SAME &mAttribSysRequestInterface (DWARF :77).
//
// LOCK-BIT MAP (from the asm `extrwi` field extracts on the lbz'd status byte @0):
//   GetAttribSysRequestInterface const 0x82695518: extrwi ...,1,27 -> bit 4 read-lock
//   GetAttribSysRequestInterface       0x82695668: extrwi ...,1,28 -> bit 3 write-lock
// These match CgsModule::IOBuffer::eStatusLockedForRead/Write, so the bodies reuse
// IsBufferLockedForReading()/IsBufferLockedForWriting() by NAME.
//
// RECONCILIATION (wave 7): the DWARF (dwarfdump BrnSoundLogicModuleIo.h:50-82) gives
// LogicOutputBuffer a request-interface shape, NOT the results/replay shape the prior
// minimal slice guessed. The prior GetResults/GetReplay + maResultsStorage/maReplayStorage
// model was contradicted by the DWARF and is REPLACED here:
//   struct LogicOutputBuffer : public OutputBuffer {
//     RootOutputBuffer::AttribSysRequestInterface     mAttribSysRequestInterface; // @ +0x04 FIRST (:81)
//     RootOutputBuffer::SoundResourceRequestInterface mResourceRequestInterface;  //         (:82)
//     void Construct();                                        // :60
//     const/non-const GetResourceRequestInterface();           // :65 / :73
//     const/non-const GetAttribSysRequestInterface();          // :69 / :77
//   }
// The DWARF base is "OutputBuffer" (a thin CgsModule::IOBuffer subclass, the OutputBuffer
// side of the InputBuffer/OutputBuffer pair). That intermediary is un-homed in-tree, so
// this keeps the direct CgsModule::IOBuffer base -- byte-identical (status byte @0, first
// member @ +0x04, which funcs 04/05's `addi r3,this,4` attest). FLAG(low): the
// OutputBuffer intermediary is elided; if it ever carries members it must be interposed.
//
// The two members are DWARF-named, correctly-sized opaque storage borrowed from
// RootOutputBuffer (AttribSysRequestInterface<2048> = 0x810; SoundResourceRequestInterface
// = RequestInterface<4096> = 0x1010). Only mAttribSysRequestInterface's +0x04 start is
// X360-attested by this batch; mResourceRequestInterface's start (+0x814) follows from the
// AttribSys span and is NOT independently attested (FLAG). offsetof(mAttribSysRequest-
// Interface) == 0x04 is byte-faithful (only the 1-byte IOBuffer base precedes it).
// =============================================================================

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // BrnSound::Module::Io::LogicOutputBuffer -- the per-frame logic OUTPUT payload the
    // sound logic module fills for the root module. DWARF shape (request interfaces).
    struct LogicOutputBuffer : public CgsModule::IOBuffer
    {
        // BrnSoundLogicModuleIo.h:60 (DWARF). X360 body @0x826C9A28, reached from the
        // CreateIOBuffer<LogicOutputBuffer> instantiation @0x826DCD30 (Alloc size 6224):
        //     *a1 = 1;                                              -- IOBuffer::Construct
        //     VariableEventQueue<4096,16>::Construct(a1 + 2068); Clear(a1 + 2068);
        //     VariableEventQueue<2048,16>::Construct(a1 + 4);    Clear(a1 + 4);
        //     for (11 words at a1 + 6180) *w++ = 0;
        // 2068 == 0x814 == mResourceRequestInterface, 4 == mAttribSysRequestInterface (note the
        // reversed call order vs RootOutputBuffer -- reproduced), 6180 == 0x1824 == the trailing
        // state run below. The two request interfaces derive from those queues, so the
        // Construct+Clear pair is the queue's own, reached through the members' named storage.
        // FLAG PC: homed inline here (the console emits it out-of-line) so the symbol exists for
        // every CreateIOBuffer<LogicOutputBuffer> instantiation regardless of build-list state.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();

            CgsModule::VariableEventQueue<4096, 16>* lpResourceQueue =
                reinterpret_cast<CgsModule::VariableEventQueue<4096, 16>*>(mResourceRequestInterfaceStorage);
            lpResourceQueue->Construct();
            lpResourceQueue->Clear();

            CgsModule::VariableEventQueue<2048, 16>* lpAttribSysQueue =
                reinterpret_cast<CgsModule::VariableEventQueue<2048, 16>*>(mAttribSysRequestInterfaceStorage);
            lpAttribSysQueue->Construct();
            lpAttribSysQueue->Clear();

            memset(mau8TrailingState, 0, sizeof(mau8TrailingState));
        }

        // BrnSoundLogicModuleIo.h:65 / :73 (DWARF; own TU, declared-only here) --
        // &mResourceRequestInterface. Not bodied by this wave.
        const RootOutputBuffer::SoundResourceRequestInterface* GetResourceRequestInterface() const;
        RootOutputBuffer::SoundResourceRequestInterface*       GetResourceRequestInterface();

        // X360 0x82695518 (read-lock, DWARF :69) / 0x82695668 (write-lock, DWARF :77) --
        // &mAttribSysRequestInterface at this+0x04 (this wave).
        const RootOutputBuffer::AttribSysRequestInterface* GetAttribSysRequestInterface() const;
        RootOutputBuffer::AttribSysRequestInterface*       GetAttribSysRequestInterface();

    private:
        // Byte widths mirror RootOutputBuffer's attested request-interface spans
        // (AttribSysRequestInterface<2048> = 0x810, SoundResourceRequestInterface =
        // RequestInterface<4096> = 0x1010). Opaque storage because the request-interface
        // template return-type tags are incomplete forward decls (same discipline as
        // RootOutputBuffer); the getters reinterpret_cast &storage to the typedef pointer.
        static const int KI_AttribSysInterfaceBytes = 0x0810; // @ +0x04 .. +0x814
        static const int KI_ResourceInterfaceBytes  = 0x1010; // @ +0x814 ..

        u8 maStatusPad[0x04 - sizeof(CgsModule::IOBuffer)];             // base end -> +0x04
        u8 mAttribSysRequestInterfaceStorage[KI_AttribSysInterfaceBytes]; // @ +0x04 FIRST (DWARF :81)
        u8 mResourceRequestInterfaceStorage[KI_ResourceInterfaceBytes];  // @ +0x814     (DWARF :82; start not independently attested)

        // X360-attested trailing state: Construct @0x826C9A28 ends with `v5 = 11; do *v4++ = 0;
        // while (--v5);` over 11 words starting at +0x1824, and the CreateIOBuffer
        // instantiation @0x826DCD30 allocates 6224 == 0x1824 + 44. The member set behind those
        // 44 bytes is not recovered (the DWARF slice stops at the two request interfaces), so it
        // is modelled as named opaque storage -- WITHOUT it sizeof(LogicOutputBuffer) would be
        // 6180 and the buffer would be under-allocated by 44 bytes relative to the console.
        u8 mau8TrailingState[44];                                        // @ +0x1824

        static void _AssertLayout()
        {
            static_assert(offsetof(LogicOutputBuffer, mAttribSysRequestInterfaceStorage) == 0x04,
                          "LogicOutputBuffer.mAttribSysRequestInterface @ +0x04");
            static_assert(offsetof(LogicOutputBuffer, mau8TrailingState) == 0x1824,
                          "LogicOutputBuffer trailing state @ +0x1824 (X360 Construct @0x826C9A28)");
            static_assert(sizeof(LogicOutputBuffer) == 6224,
                          "LogicOutputBuffer == 6224 (the X360 CreateIOBuffer Alloc literal @0x826DCD30)");
        }
    };

} // namespace Io
} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_LOGIC_MODULE_BRN_SOUND_LOGIC_MODULE_IO_H
