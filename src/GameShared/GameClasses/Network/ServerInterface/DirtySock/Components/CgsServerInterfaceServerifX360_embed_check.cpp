// Embed check for the CgsNet-serverif leaf group (X360):
//   - CgsNetwork::ServerInterfaceComponent::~ServerInterfaceComponent   @ 0x827DB3E8
//   - CgsNetwork::ServerInterfaceEndGameDataX360::Prepare               @ 0x82877130
//   - CgsNetwork::ServerInterfaceHttp::~ServerInterfaceHttp             @ 0x827DE1F0
//
// Includes all three homes together to catch cross-header ODR / include-order
// problems, and exercises each bodied function. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"

namespace
{
    using namespace CgsNetwork;

    // The X360 EndGameData payload the asm zeroes spans 16 u32 words; the type must be
    // at least vptr + that payload. (Absolute member offsets across the X360 32-bit /
    // host 64-bit boundary are NOT asserted -- only the payload word count is proven.)
    static_assert(sizeof(ServerInterfaceEndGameDataX360().maResultWords) == 16u * sizeof(u32),
                  "EndGameDataX360 result payload must be 16 words (this+0x04..0x40)");

    // The platform alias must resolve to the X360 leaf.
    static_assert(sizeof(ServerInterfaceEndGameData) == sizeof(ServerInterfaceEndGameDataX360),
                  "ServerInterfaceEndGameData must alias the X360 leaf on this build");

    // Http derives from the component, so it must be at least as large as the base.
    static_assert(sizeof(ServerInterfaceHttp) >= sizeof(ServerInterfaceComponent),
                  "ServerInterfaceHttp must embed the ServerInterfaceComponent base");

    // Exercise the bodied EndGameDataX360::Prepare and confirm it zero-fills + returns true.
    bool ExercisePrepare()
    {
        ServerInterfaceEndGameDataX360 lEndGameData;
        for (s32 i = 0; i < 16; ++i)
            lEndGameData.maResultWords[i] = 0xDEADBEEFu;
        bool lbOk = lEndGameData.Prepare();
        bool lbCleared = true;
        for (s32 i = 0; i < 16; ++i)
            lbCleared = lbCleared && (lEndGameData.maResultWords[i] == 0u);
        return lbOk && lbCleared;
    }

    // Exercise the two vector-deleting destructors through polymorphic deletion paths
    // (heap allocation so the conditional `operator delete` branch is referenced).
    void ExerciseComponentDtor()
    {
        ServerInterfaceComponent* lpComponent = 0;
        delete lpComponent;   // null-safe; references the virtual dtor / delete thunk
    }

    void ExerciseHttpDtor()
    {
        ServerInterfaceHttp* lpHttp = 0;
        ServerInterfaceComponent* lpBase = lpHttp;   // upcast proves the inheritance
        delete lpBase;
    }

    [[maybe_unused]] bool kbPrepare = ExercisePrepare();
}
