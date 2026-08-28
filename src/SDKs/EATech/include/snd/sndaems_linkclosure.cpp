// ============================================================================
// sndaems_linkclosure.cpp -- FLAG PC link-closure stubs for the three
// declared-only externs Snd9::Aems::BeginRemoveModuleBank references
// (AEMS-cascade slice 2 mounted sndaems.cpp for SetSamplePlayerFactory).
//
// BeginRemoveModuleBank is the CONTENT-TEARDOWN path -- a module bank must have
// been LOADED before anything can remove one, and the content campaign has not
// started, so all three are honest unreachable stand-ins until their console
// bodies are reconstructed:
//   Snd9::RemoveModuleBankHandler        -- the deferred-ring replay handler
//   Csis::Class::UnsubscribeConstructorFast @ (CSIS SDK; the ctor-client unhook)
//   SNDAEMSI_updatedestroy               -- the C-facing per-subscriber teardown
// ============================================================================

#include "SDKs/EATech/include/snd/sndaems.h"
#include "SDKs/Csis/CsisClass.h"

namespace Snd9
{
    // The ring replay handler (paired with the RemoveModuleBankCommand record).
    // Returns the record size per the ring contract; unreachable until a bank
    // removal is queued, which needs a loaded bank first.
    int RemoveModuleBankHandler(void* /*apCommand*/)
    {
        return 0;
    }
}

namespace Csis
{
    int Class::UnsubscribeConstructorFast(ClassHandle** /*appHandle*/,
                                          ClassClientNode* /*apNode*/)
    {
        return 0;
    }
}

extern "C" int SNDAEMSI_updatedestroy(void* /*apSubscriber*/)
{
    return 0;
}
