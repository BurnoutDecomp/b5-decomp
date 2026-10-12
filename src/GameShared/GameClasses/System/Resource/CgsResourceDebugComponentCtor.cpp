#include "GameShared/GameClasses/System/Resource/CgsResourceDebugComponent.h"

// CgsResource::DebugComponent::DebugComponent @ 0x827E08D0
//
// Brings the resource subsystem's in-game debug component up. The X360 constructor's observable
// stores, read from the ARTIST disassembly:
//   * stw  off_820D0014, 0(this)        -- install the component's own vtable (+0)
//   * stw  off_820CDFE4, 0x1C(this)     -- install an embedded debug sub-component's vtable (+0x1C)
//   * stw  off_820CEF38, 0x3F8(this)    -- install an embedded debug sub-component's vtable (+0x3F8)
//   * stw {0,1,0,1,0,1}, 0x468..0x47C(this) -- seed three running-total pairs in the +0x3F8 sub-
//                                              object ({m_size=0, m_alignment=1} couples)
//   * bl   CgsResource::DebugPoolTypeList::DebugPoolTypeList(this + 0x480) -- construct the embedded
//                                              per-pool-type accounting table
//   * stw  off_820CDFFC, 0x4D94(this)   -- install an embedded debug sub-component's vtable (+0x4D94)
//   * stw  off_820CE014, 0x4DE8(this)   -- install an embedded debug sub-component's vtable (+0x4DE8)
//
// MODELLED HERE (semantic parity, NOT byte-matched): the component's own polymorphic construction
// (the +0 vtable is compiler-emitted from this class's vtable) chains the CgsDev::DebugComponent
// base, then the embedded DebugPoolTypeList member (mPoolTypeList at +0x480) is default-constructed
// -- which the X360 DebugPoolTypeList ctor seeds to its empty-but-initialised {0,1}-couple state.
//
// All five original windows are now named embedded objects. Their C++ construction
// installs the corresponding vtables. DebugPoolHistogram::mSaved supplies the
// three real {size=0,alignment=1} descriptor constructors. Window's own constructor
// leaves its data untouched, as in this original store sequence.
namespace CgsResource
{

DebugComponent::DebugComponent()
{
    // Original by-value window and descriptor construction runs before this body.
}

}
