#include "GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed-home (HONEST STUB) for BURNOUT_X360_ARTIST.XEX
//   BrnParticle::Native::BrnDebrisArrayLite::Update  @ 0x82C08B58
//
// VMX KEYSTONE -- DELIBERATELY NOT BODIED. See BrnDebrisArrayLite.h for the full
// rationale: Update's outer loop is a scalar bucket-list walk, but each iteration
// marshals hand-vectorised VMX register state (vspltw128-splatted frame-time and
// this+0x20 floats, plus the this+0x10 vector via lvx128) and invokes
// BrnDebrisArrayLite::UpdateBucket @0x82C08410 with those vectors passed BY VMX
// REGISTER. UpdateBucket has no X360 export, so its body and register contract are
// unrecovered; a scalar lowering would have to invent the parameter layout. Per project
// policy a VMX pipeline is flagged, not paraphrased. The honest floor fires the project
// assert if reached so no fabricated debris integration is silently performed.

namespace BrnParticle
{
namespace Native
{
    void BrnDebrisArrayLite::Update(int /*liJobParam2*/, int /*liJobParam3*/, int /*liJobParam5*/,
                                    int /*liJobParam6*/, int /*liJobParam7*/, float /*lfFrameTime*/)
    {
        // HONEST FLOOR: the per-bucket VMX integration + the undecompiled UpdateBucket
        // callee (@0x82C08410) are not reconstructed in this pass.
        CGS_ASSERT(false, "BrnParticle::Native::BrnDebrisArrayLite::Update: VMX bucket-update pipeline not yet reconstructed");
    }
}
}
