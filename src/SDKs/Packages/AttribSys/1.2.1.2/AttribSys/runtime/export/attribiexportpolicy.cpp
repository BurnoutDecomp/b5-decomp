#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribiexportpolicy.h"

// ===========================================================================
// Attrib::IExportPolicy -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// The boot-reached entry point is the compiler-synthesised scalar deleting
// destructor @ 0x82805760; the C++ source for it is this empty virtual dtor,
// from which MSVC re-emits the same vptr-store + conditional GLOBAL
// `::operator delete(this)` thunk. See attribiexportpolicy.h for the asm.
//
// The dtor body is empty: IExportPolicy is a pure interface with no owned
// members (vptr only), so there is nothing to tear down.
// ===========================================================================

namespace Attrib
{
    IExportPolicy::~IExportPolicy()
    {
    }
}
