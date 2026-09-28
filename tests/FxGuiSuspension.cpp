// Queue/manager fixtures around extracted production event types, output
// template and the COMPLETE network suspension switch case. Wire oracle:
// ARTIST 82493A88 outputs {4,45,12,type}, channel40/size16; 82578510 reads
// type from payload+0, after the GUI interpreter has removed the wrapper.
#include <cstdint>
#include <cstdio>
#include <cstring>
using s32 = int32_t;
using u32 = uint32_t;
namespace CgsModule { struct Event {}; }
struct Queue
{
    alignas(16) unsigned char data[64]{};
    int id = -1, size = 0;
    bool AddEvent(const CgsModule::Event* event, s32 type, s32 bytes)
    { id = type; size = bytes; std::memcpy(data, event, bytes); return true; }
};
namespace CgsGui
{
#include "fx_gui_suspension_types.inc"
    struct StateInterface
    {
        Queue mOutEventQueue;
        template <typename TEvent> void OutputGuiEvent(TEvent& lrEvent);
    };
#include "fx_gui_suspension_output.inc"
}
static int checks, failures, assertions;
#define CGS_ASSERT(condition, message) do { if (!(condition)) ++assertions; } while (0)
namespace CgsNetwork
{
    struct ServerInterfaceDirtySock
    {
        bool suspended = false;
        bool IsSuspended() const { return suspended; }
    };
}
struct Connection { bool logged = false; bool IsLoggedIn() const { return logged; } };
struct Server : CgsNetwork::ServerInterfaceDirtySock
{
    Connection connection;
    Connection* GetConnectionComponent() { return &connection; }
};
struct Login { bool signing = false; bool IsSigningIn() const { return signing; } };
struct SuspensionManager
{
    enum { E_SUSPENSION_TYPE_OFFLINE = 0 };
    bool busy = false;
    int suspends = 0, resumes = 0;
    bool IsSuspending() const { return busy; }
    void Suspend(int, void(*)(void*), void*) { ++suspends; }
    void Resume(void(*)(void*), void*) { ++resumes; }
};
struct Manager
{
    Server server;
    Login login;
    SuspensionManager suspension;
    bool lobby = false;
    Server* GetServerInterface() { return &server; }
    Login* GetLoginManager() { return &login; }
    SuspensionManager* GetSuspensionManager() { return &suspension; }
    bool IsDoingFreeBurnLobby() const { return lobby; }
};
struct Consumer
{
    enum { KI_GUI_EVENT_NETWORK_SUSPENSION = 45, E_STATE_WAIT_SUSPENSION = 1,
           E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND = 2 };
    Manager manager;
    Manager* mpNetworkManager = &manager;
    int meState = 0;
    static void SuspensionFinishedCallback(void*) {}
    static void ResumeFinishedCallback(void*) {}
    void Receive(const CgsModule::Event* lpRecord, const void* lpPayload)
    {
        switch (KI_GUI_EVENT_NETWORK_SUSPENSION)
        {
#include "fx_gui_suspension_consumer.inc"
        }
    }
};
static void Check(bool ok, const char* label)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", label); } }
static void Raw(Consumer& consumer, int type)
{
    // The extra three poisoned words keep the old, wrong +12 read in bounds.
    alignas(16) s32 record[4] = { type, 0x77777777, 0x77777777, 0x77777777 };
    consumer.Receive(reinterpret_cast<const CgsModule::Event*>(record), record);
}
int main()
{
    for (int type = 0; type < 2; ++type)
    {
        CgsGui::StateInterface state;
        CgsGui::GuiEventNetworkSuspension event(type == 1);
        state.OutputGuiEvent(event);
        const Queue& q = state.mOutEventQueue;
        s32 wire[4]{}; std::memcpy(wire, q.data, sizeof(wire));
        Check(q.id == 40, "output uses wrapper channel40");
        Check(q.size == 16, "one 16-byte record");
        Check(wire[0] == 4 && wire[1] == 45 && wire[2] == 12, "canonical size/type/offset header");
        Check(wire[3] == type, "payload suspension/resume value");
        Consumer c;
        int previous = assertions;
        Raw(c, type);
        Check(assertions == previous, "offline raw payload is valid");
        Check(c.meState == 0 && c.manager.suspension.suspends == 0 && c.manager.suspension.resumes == 0,
              "offline suspension event leaves network state alone");
    }
    Consumer c;
    c.manager.server.connection.logged = true;
    Raw(c, 0);
    Check(c.manager.suspension.suspends == 1 && c.meState == 1, "logged-in suspend arm");
    c.manager.suspension.busy = true;
    c.meState = 0;
    Raw(c, 0);
    Check(c.meState == 2 && c.manager.suspension.suspends == 1, "busy suspension waits");
    c.manager.suspension.busy = false;
    c.manager.server.suspended = true;
    Raw(c, 1);
    Check(c.manager.suspension.resumes == 1 && c.meState == 1, "resume arm");
    c.manager.suspension.busy = true;
    Raw(c, 1);
    Check(c.manager.suspension.resumes == 1, "busy resume remains gated");
    int previous = assertions;
    Raw(c, 9);
    Check(assertions == previous + 1, "invalid type still asserts");
    c.manager.suspension.busy = false;
    CgsGui::GuiEventNetworkSuspension wrapper(true);
    const void* payload = &wrapper.meSuspensionType;
    c.Receive(&wrapper, payload);
    Check(c.manager.suspension.resumes == 2, "existing channel40 decoder uses decoded payload");
    c.manager.server.suspended = false;
    c.manager.login.signing = true;
    Raw(c, 0);
    Check(c.manager.suspension.suspends == 1, "sign-in gate preserved");
    c.manager.login.signing = false;
    c.manager.lobby = true;
    Raw(c, 0);
    Check(c.manager.suspension.suspends == 1, "freeburn lobby gate preserved");
    std::printf("FxGuiSuspension: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
