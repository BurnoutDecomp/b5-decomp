// Embed check for the CgsNet-serverif-3 leaf group (X360):
//   - CgsNetwork::ServerInterfaceBroadcastMessages::~ServerInterfaceBroadcastMessages @ 0x827DE1A8
//   - CgsNetwork::ServerInterfaceTelemetry::EnableTemetry                             @ 0x82541968
//   - CgsNetwork::ServerInterfaceUsersets::~ServerInterfaceUsersets                   @ 0x827DE280
//
// Includes all three homes together to catch cross-header ODR / include-order
// problems, and exercises each bodied function. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceBroadcastMessages.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"

namespace
{
    using namespace CgsNetwork;

    // Each leaf must embed the shared ServerInterfaceComponent base (vptr + 3 words),
    // so it cannot be smaller than that base. (Absolute member offsets across the X360
    // 32-bit / host 64-bit boundary are NOT asserted -- only the embed relationship.)
    static_assert(sizeof(ServerInterfaceBroadcastMessages) >= sizeof(ServerInterfaceComponent),
                  "ServerInterfaceBroadcastMessages must embed the ServerInterfaceComponent base");
    static_assert(sizeof(ServerInterfaceUsersets) >= sizeof(ServerInterfaceComponent),
                  "ServerInterfaceUsersets must embed the ServerInterfaceComponent base");
    static_assert(sizeof(ServerInterfaceTelemetry) >= sizeof(ServerInterfaceComponent),
                  "ServerInterfaceTelemetry must embed the ServerInterfaceComponent base");

    // The Telemetry component carries, after the base, a u32 + bool head and then five
    // pointers (the three telemetry handles + server-interface + event-mapping). It must
    // be large enough to reach past the two telemetry-handle pointers the asm reads
    // (this+0x18 and this+0x1C). Conservative lower bound: base + two words + 5 pointers.
    static_assert(sizeof(ServerInterfaceTelemetry) >= sizeof(ServerInterfaceComponent) + 2u * sizeof(u32) + 5u * sizeof(void*),
                  "ServerInterfaceTelemetry must hold its post-base member set");

    // Exercise the trivial constructors + scalar/vector deleting destructors of the two
    // trivial leaves (constructed and destroyed on the stack).
    void ExerciseTrivialDtors()
    {
        ServerInterfaceBroadcastMessages lBroadcast;
        ServerInterfaceUsersets         lUsersets;
        (void)&lBroadcast;
        (void)&lUsersets;
    }

    // Exercise the bodied Telemetry::EnableTemetry on a heap object (the destructor body
    // is owned by a dedicated dossier, so the object is intentionally not destroyed in
    // this compile-only check).
    void ExerciseEnableTelemetry()
    {
        ServerInterfaceTelemetry* lpTelemetry =
            static_cast<ServerInterfaceTelemetry*>(::operator new(sizeof(ServerInterfaceTelemetry)));
        lpTelemetry->EnableTemetry(true);
        lpTelemetry->EnableTemetry(false);
    }

    void (*const kpfnTrivial)()  = &ExerciseTrivialDtors;
    void (*const kpfnTelemetry)() = &ExerciseEnableTelemetry;
}
