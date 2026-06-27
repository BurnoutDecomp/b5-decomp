#pragma once

// ===========================================================================
//  Nicotine/DMixIO.hpp  --  EATech "Nicotine" audio middleware: dynamic-mixer
//  I/O accessor object.
//
//  A DMixIO is the per-connection handle the game's sound logic uses to read
//  and write a single dynamic-mixer (DMix) node's parameter slots. It carries
//  a packed 32-bit state word (SFX id / instance / object-class flags) and two
//  opaque parameter arrays:
//    - the INPUT array  (m_pDMixInputBlock,  this+0x08): game -> mixer values,
//      indexed directly by slot (4*slot from the array base).
//    - the OUTPUT array (m_pDMixOutputBlock, this+0x0C): mixer -> game values,
//      packed two 16-bit fields per int (slot/2 selects the int, slot&1
//      selects the high/low half) plus an enable word at +0x3C.
//
//  HOME RATIONALE: the PC DecFIGS DWARF carries only a PARTIAL view of this
//  type (references/DecFIGS/dwarfdump/SDKs/EATech/include/Nicotine/DMixIO.hpp:
//  the DMX_PRESET_TYPE enum and three accessor declarations). The full member
//  layout below is reconstructed STRICTLY from the X360 binary
//  (BURNOUT_X360_ARTIST.XEX) accessor bodies -- X360 asm overrides DWARF. The
//  DMX_PRESET_TYPE enum values are taken verbatim from the DWARF and match the
//  GetDMixOutput switch exactly.
//
//  Reconstructed from X360 functions:
//    Nicotine::DMixIO::DMixIO                @ 0x82B448B0  (ctor)
//    Nicotine::DMixIO::SetDMixInput          @ 0x82B448D0
//    Nicotine::DMixIO::GetDMixInput          @ 0x82B448E8
//    Nicotine::DMixIO::GetDMixOutput         @ 0x82B44908
//    Nicotine::DMixIO::TurnOffMixOutput      @ 0x82B44988
//    Nicotine::DMixIO::TurnOnMixOutput       @ 0x82B449A0
//    Nicotine::DMixIO::GetStateID            @ 0x82B449B8
//    Nicotine::DMixIO::GetSFX_ID             @ 0x82B449C8
//    Nicotine::DMixIO::GetInstanceNum        @ 0x82B449D8
//    Nicotine::DMixIO::IsSFXObj              @ 0x82B449E8
//    Nicotine::DMixIO::`vector deleting destructor' @ 0x82B44A08
// ===========================================================================

#include "types.hpp"

namespace Nicotine
{

// Forward decl from the same Nicotine header family (DWARF: DMixIO.hpp:7).
struct IDynamicMixer;

struct DMixIO
{
    // -- DMX_PRESET_TYPE: verbatim from the PC DecFIGS DWARF (DMixIO.hpp:14).
    //    Selects how GetDMixOutput interprets the packed mixer-output field.
    enum DMX_PRESET_TYPE
    {
        DMX_VOL   = 0,
        DMX_PITCH = 1,
        DMX_FREQ  = 2,
        DMX_AZIM  = 3,
        DMX_DEPTH = 4,
        DMX_COUNT = 5,
    };

    // ctor @ 0x82B448B0: installs vptr, nulls both array pointers.
    DMixIO();

    // Virtual dtor: the X360 object carries a vtable slot at +0x00 and a
    // compiler-generated `vector deleting destructor' (@0x82B44A08) that
    // re-stores the vptr then optionally frees. Modelled as a virtual dtor.
    virtual ~DMixIO() {}

    // -- INPUT array (game -> mixer). Slot-indexed (4*iSlot from base). --------

    // SetDMixInput @ 0x82B448D0: store iValue at input slot iSlot (no-op if
    // the input array is null). Returns this (X360 returns the object ptr).
    DMixIO* SetDMixInput(int iSlot, int iValue);

    // SetDMixInputUnsafe (DWARF DMixIO.hpp:45): unchecked store variant.
    void SetDMixInputUnsafe(int iSlot, int iValue);

    // GetDMixInput @ 0x82B448E8: read input slot iSlot (0 if array null).
    int GetDMixInput(int iSlot);

    // GetDMixInputPtr (DWARF DMixIO.hpp:59): raw input-array base.
    int* GetDMixInputPtr();

    // -- OUTPUT array (mixer -> game). Two 16-bit fields packed per int. -------

    // GetDMixOutput @ 0x82B44908: read mixer-output slot iSlot, interpreting
    // the packed field per ePreset (0 if array null / unknown preset).
    int GetDMixOutput(int iSlot, int ePreset);

    // GetDMixOutputPtr (DWARF DMixIO.hpp:60): raw output-array base.
    int* GetDMixOutputPtr();

    // TurnOffMixOutput @ 0x82B44988 / TurnOnMixOutput @ 0x82B449A0: clear/set
    // the output-array enable word (+0x3C). No-op if the output array is null.
    DMixIO* TurnOffMixOutput();
    DMixIO* TurnOnMixOutput();

    // -- packed state-word accessors (all read miState @ +0x04). --------------

    // GetStateID @ 0x82B449B8: low 8 bits of the state word.
    int GetStateID();
    // GetSFX_ID  @ 0x82B449C8: bits [10:4] (7-bit SFX id).
    int GetSFX_ID();
    // GetInstanceNum @ 0x82B449D8: bits [20:16] (5-bit instance index).
    int GetInstanceNum();
    // IsSFXObj @ 0x82B449E8: top 3 bits == 0b010 (object-class tag == SFX).
    int IsSFXObj();

    // FLAG (Nicotine PDB reconcile -- ProStreet08Milestone.pdb, Nicotine::DMixIO [sizeof=16]):
    // MATCHES exactly (vfptr/u32/int*/int*), so the PDB names are authoritative:
    // muState->m_ID, mpiDMixInput->m_pDMixInputBlock, mpiDMixOutput->m_pDMixOutputBlock. The
    // "enable word at +0x3C" is inside the output BLOCK (m_pDMixOutputBlock points to it), not
    // a DMixIO member -- consistent with sizeof=16.
    // [+0x00] vptr (the virtual dtor above).
    // [+0x04] packed state word: bit0-7 stateId, bit4-10 sfxId,
    //         bit16-20 instanceNum, bit29-31 object-class tag.
    u32  m_ID;
    // [+0x08] input parameter array base (int per slot); null until allocated.
    int* m_pDMixInputBlock;
    // [+0x0C] output parameter array base (two 16-bit fields per int, plus an
    //         enable word at +0x3C); null until allocated.
    int* m_pDMixOutputBlock;
};

} // namespace Nicotine
