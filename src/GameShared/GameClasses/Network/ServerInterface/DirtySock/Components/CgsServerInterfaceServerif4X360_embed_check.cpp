// Embed check for the CgsNet-serverif-4 leaf group (X360 + base):
//   - CgsNetwork::ServerInterfaceGameSearchParamsX360::Prepare              @ 0x82878B20
//   - CgsNetwork::ServerInterfaceQuickJoinParamsX360::Prepare               @ 0x8287A4A8
//   - CgsNetwork::ServerInterfaceQuickJoinParamsBase::~...(vec del dtor)    @ 0x82541550
//
// Includes both grown homes together to catch cross-header ODR / include-order
// problems, and exercises each bodied function. Compile-only.
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include <utility>   // std::declval (for sizeof of members on an abstract type)

namespace
{
    using namespace CgsNetwork;

    // --- Layout proofs (relative sizes / payload word counts only; absolute member
    //     offsets across the X360 32-bit / host 64-bit boundary are NOT asserted) ---

    // The platform aliases must resolve to the X360 leaves.
    static_assert(sizeof(ServerInterfaceGameSearchParams) == sizeof(ServerInterfaceGameSearchParamsX360),
                  "ServerInterfaceGameSearchParams must alias the X360 leaf on this build");
    static_assert(sizeof(ServerInterfaceQuickJoinParams) == sizeof(ServerInterfaceQuickJoinParamsX360),
                  "ServerInterfaceQuickJoinParams must alias the X360 leaf on this build");

    // The X360 leaves embed their respective bases.
    static_assert(sizeof(ServerInterfaceGameSearchParamsX360) >= sizeof(ServerInterfaceGameSearchParamsBase),
                  "GameSearchParamsX360 must embed the base");
    static_assert(sizeof(ServerInterfaceQuickJoinParamsX360) >= sizeof(ServerInterfaceQuickJoinParamsBase),
                  "QuickJoinParamsX360 must embed the base");

    // The X360 payloads the asm zeroes are 80 bytes each. (declval: the GameSearch leaf
    // is abstract here -- the custom-flags getters live in their own un-assigned TU.)
    static_assert(sizeof(std::declval<ServerInterfaceGameSearchParamsX360&>().maX360Payload) == 80u,
                  "GameSearchParamsX360 payload is 80 bytes (this+0x18..0x67)");
    static_assert(sizeof(std::declval<ServerInterfaceQuickJoinParamsX360&>().maX360Payload) == 80u,
                  "QuickJoinParamsX360 payload is 80 bytes (this+0x4C..0x9B)");

    // --- GameSearchParamsX360: the custom-flags getters are NOT in this TU; they stay
    //     pure virtual on the leaf. Exercise the bodied Prepare via a flagged-placeholder
    //     test subclass that supplies the StructureInterface + custom-flags slots so the
    //     type is instantiable here. The getter return values are placeholders for the
    //     test only -- they are NOT the reconstructed behaviour (which lives in its own
    //     un-assigned custom-flags TU). ---
    struct GameSearchTestProbe : public ServerInterfaceGameSearchParamsX360
    {
        const char* GetPattern() const         { return ""; }   // placeholder (test only)
        s32         GetPatternLength() const    { return 1; }    // placeholder (test only)
        u32         GetDataSize() const         { return 0; }    // placeholder (test only)
        void*       GetData()                   { return 0; }    // placeholder (test only)
        const void* GetData() const             { return 0; }    // placeholder (test only)
        u32         GetCustomFlagsMask() const  { return 0; }    // placeholder (test only)
        u32         GetCustomFlagsValue() const { return 0; }    // placeholder (test only)
    };

    bool ExerciseGameSearchPrepare()
    {
        GameSearchTestProbe lProbe;
        // Dirty the payload so the zero-fill is observable.
        for (s32 i = 0; i < 80; ++i)
            lProbe.maX360Payload[i] = 0xFFu;
        lProbe.muX360Field_68 = 0xDEADBEEFu;

        bool lbOk = lProbe.Prepare();

        bool lbCleared = (lProbe.muX360Field_68 == 0u);
        for (s32 i = 0; i < 80; ++i)
            lbCleared = lbCleared && (lProbe.maX360Payload[i] == 0u);
        return lbOk && lbCleared;
    }

    bool ExerciseQuickJoinPrepare()
    {
        ServerInterfaceQuickJoinParamsX360 lParams;
        for (s32 i = 0; i < 80; ++i)
            lParams.maX360Payload[i] = 0xFFu;
        lParams.muX360Field_9C = 0xDEADBEEFu;

        bool lbOk = lParams.Prepare();

        bool lbCleared = (lParams.muX360Field_9C == 0u);
        for (s32 i = 0; i < 80; ++i)
            lbCleared = lbCleared && (lParams.maX360Payload[i] == 0u);
        return lbOk && lbCleared;
    }

    // Exercise the QuickJoin base vector-deleting destructor through a polymorphic
    // deletion path (heap allocation references the conditional operator-delete branch).
    void ExerciseQuickJoinDtor()
    {
        ServerInterfaceQuickJoinParamsBase* lpParams = new ServerInterfaceQuickJoinParamsX360();
        delete lpParams;   // virtual dtor -> vector deleting destructor thunk
    }

    [[maybe_unused]] bool kbGameSearch = ExerciseGameSearchPrepare();
    [[maybe_unused]] bool kbQuickJoin  = ExerciseQuickJoinPrepare();
}
