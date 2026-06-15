#pragma once

// Resource bundle-loader IO event payloads. Reconstructed from the DecFIGS DWARF.
// Events derive from an empty per-module Event base (CgsModule event convention).
#include "types.hpp"

namespace CgsModule { class BaseEventReceiverQueue; }   // referenced by pointer only

namespace CgsResource
{
namespace Events
{
    struct Event {};

    struct BundleLoaderEvent : public Event
    {
        CgsModule::BaseEventReceiverQueue* mpUser;
        s32  miEventId;
        bool mbLiveUpdateReplace;
        char macFileName[128];
        s32  miPoolId;
    };

    struct LoadBundleRequest : public BundleLoaderEvent
    {
        bool mbAllowFailiure;
        bool mbUseHDCache;
    };

    struct LoadBundleResponse : public BundleLoaderEvent
    {
        enum EResult
        {
            E_RESULT_SUCCESS       = 0,
            E_RESULT_OUT_OF_MEMORY = 1
        };
        EResult meResult;
    };

    struct UnloadBundleResponse : public BundleLoaderEvent
    {
    };
}
}
