// Link the real Lua VM, HeapMalloc and GeneralAllocator. Only assert reporting
// is replaced; the test observes failure without opening the game assert UI.
#include "GameShared/GameClasses/Fsm/CgsLuaState.h"
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
int giChecks = 0, giFailures = 0, giAsserts = 0;
void Check(bool pass, const char* label) {
    ++giChecks;
    if (!pass) { ++giFailures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) {
    ++giAsserts; std::printf("ASSERT %s\n", message); return 0;
}
void* EndAssert() { return nullptr; }
}}
#include "gui_fsm_heap.inc"
std::vector<unsigned char> MakeResource(const char* text) {
    std::vector<unsigned char> bytes(8 + std::strlen(text));
    const u32 size = static_cast<u32>(bytes.size() - 8), offset = 8;
    std::memcpy(bytes.data(), &size, 4);
    std::memcpy(bytes.data() + 4, &offset, 4);
    std::memcpy(bytes.data() + 8, text, size);
    return bytes;
}
bool Load(CgsFsm::LuaState& state, CgsMemory::HeapMalloc& heap, const char* source) {
    auto resource = MakeResource(source);
    state.Prepare(reinterpret_cast<CgsResource::LuaCodeResource*>(resource.data()), &heap);
    lua_getfield(state.mpLuaState, LUA_GLOBALSINDEX, "GetCurrentState");
    const bool loaded = lua_isfunction(state.mpLuaState, -1) != 0;
    if (!loaded)
        std::printf("Lua load failed: %s\n", lua_tostring(state.mpLuaState, -2));
    lua_settop(state.mpLuaState, 0);
    return loaded;
}
bool Send(lua_State* state, const char* event) {
    lua_getfield(state, LUA_GLOBALSINDEX, "NextState");
    lua_pushstring(state, event);
    const bool ok = lua_pcall(state, 1, 0, 0) == 0;
    lua_settop(state, 0);
    return ok;
}
bool IsState(lua_State* state, const char* expected) {
    lua_getfield(state, LUA_GLOBALSINDEX, "GetCurrentState");
    const bool ok = lua_pcall(state, 0, 1, 0) == 0 &&
        lua_isstring(state, -1) && std::strcmp(lua_tostring(state, -1), expected) == 0;
    lua_settop(state, 0);
    return ok;
}
int main() {
    CgsMemory::HeapMalloc heap;
    heap.Construct(s_fsmLuaHeapBuffer, sizeof(s_fsmLuaHeapBuffer));
    CgsFsm::LuaState screen, overlay, hud;
    screen.Construct(); overlay.Construct(); hud.Construct();
    Check(Load(screen, heap, screen_source), "authored screen FSM loads");
    Check(Load(overlay, heap, overlay_source), "authored overlay FSM loads");
    Check(Load(hud, heap, freeburn_source), "authored freeburn FSM loads");
    bool inputOk = true;
    // Ordinary ignored GUI events still replace state/event tables in NextState.
    // Three independent VMs share the backing; one VM cannot collect another's
    // transient tables when a new HUD chunk runs out of allocation space.
    for (int i = 0; i < 300; ++i)
        inputOk = Send(screen.mpLuaState, "NOOP") && inputOk;
    Check(inputOk, "GUI input history remains valid");
    for (int cycle = 0; cycle < 2; ++cycle) {
        hud.Release();
        const bool loaded = Load(hud, heap, event_source);
        Check(loaded, "Showtime HUD defines its Lua entry points after input history");
        Check(loaded && IsState(hud.mpLuaState, "PRE_FLY_BY"), "event FSM starts at authored pre-flyby state");
        if (loaded) {
            // The authored pre-flyby completion event enters the live HUD.
            Send(hud.mpLuaState, "BF_PROCEED");
        }
        Check(loaded && IsState(hud.mpLuaState, "RACE_MAIN"), "Showtime HUD reaches RACE_MAIN");
        Check(loaded && Send(hud.mpLuaState, "PAUSE") &&
              IsState(hud.mpLuaState, "PAUSED"), "pausing hides Showtime HUD through authored state");
        Check(loaded && Send(hud.mpLuaState, "UNPAUSE") &&
              IsState(hud.mpLuaState, "RACE_MAIN"), "unpause restores Showtime HUD");
        hud.Release();
        const bool freeburn = Load(hud, heap, freeburn_source);
        Check(freeburn && IsState(hud.mpLuaState, "FBURN_MAIN"), "finishing Showtime restores freeburn HUD");
        Check(freeburn && Send(hud.mpLuaState, "PAUSE") &&
              IsState(hud.mpLuaState, "PAUSED"), "post-Showtime pause hides gameplay HUD");
        Check(freeburn && Send(hud.mpLuaState, "UNPAUSE") &&
              IsState(hud.mpLuaState, "FBURN_MAIN"), "post-Showtime unpause restores freeburn");
    }
    hud.Release(); overlay.Release(); screen.Release();
    Check(giAsserts == 0, "no allocator or Lua assertions");
    heap.Destruct();
    std::printf("GuiFsmHeap: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
