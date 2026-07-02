#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CameraStatusMessage::Construct            @ 0x8257AD20
//   BrnNetwork::CameraStatusMessage::GetPackedMessageSize @ 0x8257C378
//   BrnNetwork::CameraStatusMessage::PackOrUnpack         @ 0x8257AD58
//
// A network message carrying a single 32-bit camera-status word (at offset 32).
// Construct/GetPackedMessageSize delegate to the CgsNetwork::Message base after
// resetting that word. PackOrUnpack serialises the word through the message's
// pack/unpack primitive, OR-ing in the PVS debug "is simple" flag (always false).

namespace CgsNetwork
{
    // Base message (other TU); declared for the compile-only gate.
    struct Message
    {
        void* Construct();
        int   GetPackedMessageSize();
    };
}

namespace BrnNetwork
{
    namespace
    {
        // sub_82881370: the message field pack/unpack primitive (other TU).
        int PackField(void* /*pMessage*/, int* /*pField*/, int /*liMode*/, int /*liBytes*/)
        {
            __debugbreak();
            return 0;
        }
    }

    struct CameraStatusMessage : CgsNetwork::Message
    {
        void* Construct();
        int   GetPackedMessageSize();
        int   PackOrUnpack();
    };

    void* CameraStatusMessage::Construct()
    {
        void* lpResult = CgsNetwork::Message::Construct();
        *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 32) = 0;
        return lpResult;
    }

    int CameraStatusMessage::GetPackedMessageSize()
    {
        reinterpret_cast<u32*>(this)[8] = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    int CameraStatusMessage::PackOrUnpack()
    {
        // asm calls BrnWorld__PVSDebugComponent__IsSimple with `this` as the argument, but
        // CameraStatusMessage does not derive from PVSDebugComponent (whose IsSimple() is
        // `protected` besides). The X360 linker identical-code-folds trivial `return false`
        // leaf functions together; this call site landed on PVSDebugComponent::IsSimple's
        // address purely by ICF, not by any real relationship. IsSimple()'s body is a
        // constant `return false` (BrnPVSDebugComponent.h:102), so the call's observable
        // result -- 0 -- is reproduced directly rather than performing the (illegal, and
        // semantically bogus) cross-class cast.
        u8 lbIsSimple = 0;

        int liField = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 32);
        int liResult = PackField(this, &liField, 0, 4) | lbIsSimple;
        *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 32) = liField;
        return liResult;
    }
}
