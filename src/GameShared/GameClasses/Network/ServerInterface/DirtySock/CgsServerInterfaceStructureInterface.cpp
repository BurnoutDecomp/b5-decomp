#include "CgsServerInterfaceStructureInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceStructureInterface::`vector deleting destructor' @ 0x82541120
//
// ---------------------------------------------------------------------------
// `vector deleting destructor'  @ 0x82541120
//
//   stw  off_8207C88C, 0(r31)      ; install the (only) vptr at this+0
//   clrlwi r10, r4, 31             ; r10 = a2 & 1   (the "delete me too" flag)
//   if ( (a2 & 1) != 0 )
//   {
//       operator delete(this);     ; bl operator_delete @ 0x82C08FB0
//       return this;
//   }
//   return this;
//
// This is the MSVC compiler-emitted deleting-destructor thunk for a vptr-only
// polymorphic base: it (1) reinstalls this class's vtable pointer as the
// destructor chain unwinds to this sub-object, then (2) conditionally calls the
// global operator delete when bit0 of the hidden flag is set (delete-expression
// path) versus a plain scalar destructor (stack / placement path).
//
// There are NO observable member stores -- the class has no data members beyond
// the vptr -- so the only hand-written source for this TU is the out-of-line
// virtual destructor body (empty). MSVC regenerates the vptr-install + the
// conditional `operator delete` half of the thunk automatically from the
// virtual-destructor declaration in the header; that half is compiler codegen,
// not source, so it is intentionally not hand-reproduced (matches the
// established deleting-destructor convention in this tree).
//
// Defining the destructor out-of-line here also anchors the vtable emission to
// this TU, mirroring where the X360 build placed off_8207C88C.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    ServerInterfaceStructureInterface::ServerInterfaceStructureInterface()
    {
    }

    ServerInterfaceStructureInterface::~ServerInterfaceStructureInterface()
    {
    }

    // GetPatternLength @ 0x82876410 (DWARF CgsServerInterfaceStructureInterface.cpp:41).
    // The default pattern-buffer length every structure subclass inherits: the X360
    // vtable slot +0x08 of off_8207C88C holds a real `li r3, 0x14 ; blr` body
    // (COMDAT-folded with unrelated `return 20` getters), not __purecall -- observed
    // in the waveE rodata dump. 20 == sizeof the shared pattern buffers (e.g.
    // BrnNetwork::PlayerParamsBase::macPattern[20]).
    s32 ServerInterfaceStructureInterface::GetPatternLength() const
    {
        return 20;
    }
}
