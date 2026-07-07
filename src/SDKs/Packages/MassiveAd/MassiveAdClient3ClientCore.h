#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveClientCore -- minimal owning home (vendor
// middleware).
//
// The full CMassiveClientCore body (CMassiveClientCore ctor, RequestSessionOpen/
// Close, RequestLocateService, SetIsPaused, ...) is a SEPARATE ledger TU. This
// header only pins the surface the committed CRequestObject TU
// (MassiveAdClient3Request.h/.cpp) is attested to touch, so the class can be
// grown here later:
//
//   - Instance(): direct bl from CRequestObject::FinishBaseBlock @ 0x82BD04E8
//   - GetTime(core): direct bl on the Instance() result; its 64-bit return
//     (PPC std of r3) is the request timestamp appended to the wire buffer
//
// (BrnMassive.h forward-declares this same class and reaches the core through
// game-side wrapper declarations; this vendor-side home is the class's owning
// header.)
// ===========================================================================

namespace MassiveAdClient3
{

class CMassiveClientCore
{
public:
    // Direct bl target: returns the live client-core singleton. Body in the
    // CMassiveClientCore TU.
    static CMassiveClientCore* Instance();

    // Direct (non-virtual) bl target: the client clock, returned 64-bit (the
    // X360 stores the result with std). Body in the CMassiveClientCore TU.
    long long GetTime();
};

} // namespace MassiveAdClient3
