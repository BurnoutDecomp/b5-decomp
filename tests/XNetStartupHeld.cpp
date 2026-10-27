// Unevaluated hosted proposal. Real startup/provider/Construct bodies are
// extracted by the proposed runner. There is no real Winsock/network call.
#undef private
#undef protected
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

static unsigned guChecks, guFailures, guAssertBegin, guAssertEnd, guUnexpected;
static unsigned guExpectedAsserts;
static std::string gScenario;
struct Assertion { std::string message; int line; };
static std::vector<Assertion> gAssertions;
static void Check(bool pass, const std::string& id)
{
    ++guChecks; guFailures += !pass;
    std::printf("CHECK %s %s.%s\n", pass ? "PASS" : "FAIL", gScenario.c_str(), id.c_str());
}
namespace CgsDev { namespace Assert {
// Assert UI/allocation boundary: actual production message type and StrStream;
// private fixture storage substitutes only the native assert-system lifetime.
MessageBufferPC::MessageBufferPC() : mpcBuffer(macBuffer), miSize(sizeof(macBuffer)) { macBuffer[0] = 0; }
int BeginAssert() { ++guAssertBegin; return 0; }
int FireAssert(const char* message, const char*, int line) { gAssertions.push_back({message, line}); return 0; }
void* EndAssert() { ++guAssertEnd; return nullptr; }
} }

static std::atomic<int> giStarts{0}, giCleans{0}, giLastErrors{0}, giModelRefs{0};
static int giStartupError, giCleanupError, giLastError = WSAEINPROGRESS;
static WORD guNegotiated = MAKEWORD(2, 2), guRequested;
extern "C" int WSAAPI FixtureWSAStartup(WORD requested, LPWSADATA output)
{
    ++giStarts; guRequested = requested;
    if (requested != MAKEWORD(2, 2) || output == nullptr) { ++guUnexpected; return WSAEFAULT; }
    if (giStartupError != 0) return giStartupError;
    std::memset(output, 0, sizeof(*output)); // Entire real SDK WSADATA, not a shortened record.
    output->wVersion = guNegotiated; output->wHighVersion = MAKEWORD(2, 2);
    ++giModelRefs; return 0;
}
extern "C" int WSAAPI FixtureWSACleanup()
{
    ++giCleans;
    if (giCleanupError != 0) return SOCKET_ERROR;
    --giModelRefs; return 0;
}
extern "C" int WSAAPI FixtureWSAGetLastError() { ++giLastErrors; return giLastError; }
static_assert(std::is_same<decltype(&FixtureWSAStartup), decltype(&::WSAStartup)>::value, "native startup ABI");
static_assert(std::is_same<decltype(&FixtureWSACleanup), decltype(&::WSACleanup)>::value, "native cleanup ABI");
static_assert(std::is_same<decltype(&FixtureWSAGetLastError), decltype(&::WSAGetLastError)>::value, "native last-error ABI");
// Aliases apply only after the real native header and fixture declarations.
// The exact production provider implementation/state follows, unchanged.
#define WSAStartup FixtureWSAStartup
#define WSACleanup FixtureWSACleanup
#define WSAGetLastError FixtureWSAGetLastError
#include "xnet_provider_production.inc"
#undef WSAStartup
#undef WSACleanup
#undef WSAGetLastError

static unsigned guObservedRequests;
static bool gbRequestAligned;
static std::array<u8, 13> gRequestBytes;
static int ObserveXNetStartup(XNetStartupParams* params)
{
    ++guObservedRequests; gbRequestAligned = reinterpret_cast<std::uintptr_t>(params) % 4 == 0;
    std::memcpy(gRequestBytes.data(), params, gRequestBytes.size());
    return ::XNetStartup(params); // Observe arguments, then execute the real provider.
}
static int ObserveXNetStartup(const void* params)
{
    // Original held caller's const-void pointer declaration; no old body is changed.
    return ObserveXNetStartup(reinterpret_cast<XNetStartupParams*>(const_cast<void*>(params)));
}
struct TimerBoundary { unsigned calls = 0; f32 last = 0; void Prepare(f32 value) { ++calls; last = value; } };
// Header-derived scalar/enum/socket declarations only; not the real aggregate
// layout, vtable or timer/service lifetime. No production object is cast to it.
#include "xnet_caller_context.inc"
#define XNetStartup ObserveXNetStartup
#define WSAStartup FixtureWSAStartup
#include "xnet_caller_production.inc"
#undef WSAStartup
#undef XNetStartup

alignas(4) static std::array<u8, 13> gValid{{13, 1}};
static XNetStartupParams* Valid() { return reinterpret_cast<XNetStartupParams*>(gValid.data()); }
static bool Status(int expected, WORD expectedVersion)
{
    unsigned short version = 0x7777;
    return CgsPcGameTalkWinsockStartupStatus(&version) == expected && version == expectedVersion;
}
static void Invalid()
{
    unsigned short version = 0x7777;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSANOTINITIALISED, "status_before_error");
    Check(version == 0, "status_before_version");
    Check(CgsPcGameTalkWinsockStartupStatus(nullptr) == WSAEFAULT, "null_status");
    Check(XNetStartup(nullptr) == WSAEFAULT, "null_input");
    u8 shortRequest[1] = {12};
    Check(XNetStartup(reinterpret_cast<XNetStartupParams*>(shortRequest)) == WSAEINVAL, "short_size");
    auto bad = gValid; bad[1] = 0;
    Check(XNetStartup(reinterpret_cast<XNetStartupParams*>(bad.data())) == WSAEOPNOTSUPP, "secure_flag");
    bad[1] = 2;
    Check(XNetStartup(reinterpret_cast<XNetStartupParams*>(bad.data())) == WSAEOPNOTSUPP, "unknown_flag");
    for (unsigned i = 2; i < 13; ++i)
    {
        bad = gValid; bad[i] = 1;
        Check(XNetStartup(reinterpret_cast<XNetStartupParams*>(bad.data())) == WSAEOPNOTSUPP, "tail_" + std::to_string(i));
    }
    Check(giStarts == 0, "no_wsa_calls"); Check(giCleans == 0, "no_cleanup_calls");
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSANOTINITIALISED, "status_unchanged"); Check(version == 0, "version_unchanged");
}
static void Success()
{
    Check(XNetStartup(Valid()) == 0, "startup_result"); Check(guRequested == MAKEWORD(2, 2), "request_version");
    unsigned short version = 0;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == 0, "status"); Check(version == MAKEWORD(2, 2), "version");
    Check(XNetStartup(Valid()) == 0, "repeat_result"); Check(giStarts == 1, "startups1"); Check(giCleans == 0, "cleanups0"); Check(giModelRefs == 1, "model_refs1");
    auto bad = gValid; bad[1] = 0;
    Check(XNetStartup(reinterpret_cast<XNetStartupParams*>(bad.data())) == WSAEOPNOTSUPP, "invalid_after_ready");
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == 0, "status_preserved"); Check(version == MAKEWORD(2, 2), "version_preserved");
    Check(XNetStartup(Valid()) == 0, "repeat_after_invalid");
}
static void StartupRetry()
{
    giStartupError = WSASYSNOTREADY;
    Check(XNetStartup(Valid()) == WSASYSNOTREADY, "first_error"); Check(giCleans == 0, "no_rollback"); Check(giModelRefs == 0, "model_refs0");
    unsigned short version = 0x7777;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSASYSNOTREADY, "status_error"); Check(version == 0, "version0");
    giStartupError = 0;
    Check(XNetStartup(Valid()) == 0, "retry_success"); Check(giStarts == 2, "startups2"); Check(giCleans == 0, "cleanups0"); Check(giModelRefs == 1, "model_refs1");
    Check(XNetStartup(Valid()) == 0, "repeat_success"); Check(giStarts == 2, "no_extra_startup");
}
static void VersionRetry()
{
    guNegotiated = MAKEWORD(2, 0);
    Check(XNetStartup(Valid()) == WSAVERNOTSUPPORTED, "bad_version_result"); Check(giCleans == 1, "cleanup1"); Check(giModelRefs == 0, "model_refs0");
    unsigned short version = 0;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSAVERNOTSUPPORTED, "status_bad_version"); Check(version == MAKEWORD(2, 0), "version_observed");
    guNegotiated = MAKEWORD(2, 2);
    Check(XNetStartup(Valid()) == 0, "retry_success"); Check(giStarts == 2, "startups2"); Check(giCleans == 1, "cleanups1"); Check(giModelRefs == 1, "model_refs1");
    Check(XNetStartup(Valid()) == 0, "repeat_success"); Check(giStarts == 2, "no_more_acquire");
}
static void CleanupFailure()
{
    guNegotiated = MAKEWORD(1, 1); giCleanupError = 1;
    Check(XNetStartup(Valid()) == WSAEINPROGRESS, "cleanup_error_result"); Check(giCleans == 1, "cleanup_attempt1"); Check(giLastErrors == 1, "last_error1");
    unsigned short version = 0;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSAEINPROGRESS, "status_error"); Check(version == MAKEWORD(1, 1), "version_observed");
    Check(XNetStartup(Valid()) == WSAEINPROGRESS, "repeat_error"); Check(giStarts == 1, "no_second_acquire"); Check(giCleans == 1, "no_second_cleanup");
    Check(XNetStartup(nullptr) == WSAEFAULT, "invalid_after_latched_failure"); Check(Status(WSAEINPROGRESS, MAKEWORD(1, 1)), "status_still_error");
    // Deliberately no native reference-state assertion after a failed cleanup.
}
static void Concurrent()
{
    std::atomic<unsigned> ready{0}; std::atomic<bool> go{false}; std::array<int, 8> results{}; std::vector<std::thread> threads;
    for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i] { ++ready; while (!go.load()) std::this_thread::yield(); results[i] = XNetStartup(Valid()); });
    while (ready.load() != 8) std::this_thread::yield(); go = true;
    for (auto& thread : threads) thread.join();
    bool all = true; for (const auto value : results) all &= value == 0;
    Check(all, "all_results0"); Check(giStarts == 1, "startup1"); Check(giCleans == 0, "cleanup0"); Check(giModelRefs == 1, "model_refs1");
    unsigned short version = 0;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == 0, "status0"); Check(version == MAKEWORD(2, 2), "version202");
}
static void Caller(bool old)
{
    CgsGameTalk::GameTalkProtocol protocol{}; protocol.Construct(true, 9876);
    Check(guObservedRequests == 1, "xnet_call1"); Check(gRequestBytes == gValid, "request_bytes13");
    if (!old) Check(gbRequestAligned, "request_alignment4");
    Check(giStarts == 1, "single_wsa_acquisition"); Check(giModelRefs == 1, "one_retained_model_reference"); Check(giCleans == 0, "no_cleanup");
    Check(protocol.miListenPort == 9876, "port"); Check(protocol.mListeningSocket == INVALID_SOCKET && sizeof(protocol.mListeningSocket) == sizeof(::SOCKET), "native_socket_sentinel");
    Check(protocol.muBufferMsgSize == 0 && protocol.muBufferMsgStart == 0, "initial_cursors");
    Check(protocol.meConnectionState == CgsGameTalk::GameTalkProtocol::E_CONNECTIONSTATE_LISTENING, "listening_state"); Check(protocol.mrInactivityTimeout == 10.0f, "timeout");
    Check(protocol.sInactivityTimer.calls == 2 && protocol.sInactivityTimer.last == 0.016666668f, "timer_calls_and_step");
}
static void CallerFailure()
{
    guExpectedAsserts = 3; giStartupError = WSASYSNOTREADY;
    CgsGameTalk::GameTalkProtocol protocol{}; protocol.Construct(true, 9876);
    Check(giStarts == 1, "one_startup_attempt"); Check(giCleans == 0, "no_cleanup"); Check(giModelRefs == 0, "no_acquired_model_reference");
    Check(gAssertions.size() == 3 && gAssertions[0].line == 90 && gAssertions[1].line == 101 && gAssertions[2].line == 107, "original_assert_lines");
    Check(gAssertions.size() == 3 && gAssertions[0].message == "XNetStartup failed, liError 10091\n"
        && gAssertions[1].message == "WSAStartup failed (error 10091).\n"
        && gAssertions[2].message == "Failed to get proper version of Winsock, got 0.0\n", "original_assert_messages");
    Check(protocol.miListenPort == 9876 && protocol.meConnectionState == CgsGameTalk::GameTalkProtocol::E_CONNECTIONSTATE_LISTENING, "initial_state_retained");
    Check(protocol.sInactivityTimer.calls == 2, "original_assert_continuation");
    unsigned short version = 0x7777;
    Check(CgsPcGameTalkWinsockStartupStatus(&version) == WSASYSNOTREADY, "status_error"); Check(version == 0, "version0");
}
int main()
{
    const char* scenario = std::getenv("BRN_XNET_FIXTURE_SCENARIO"); gScenario = scenario ? scenario : "missing";
    if (gScenario == "invalid") Invalid(); else if (gScenario == "success") Success();
    else if (gScenario == "startup_retry") StartupRetry(); else if (gScenario == "version_retry") VersionRetry();
    else if (gScenario == "cleanup_failure") CleanupFailure(); else if (gScenario == "concurrent") Concurrent();
    else if (gScenario == "caller_success") Caller(false); else if (gScenario == "old_caller") Caller(true);
    else if (gScenario == "caller_failure") CallerFailure(); else { std::printf("UNEXPECTED scenario\n"); return 2; }
    Check(gAssertions.size() == guExpectedAsserts, "assert_count"); Check(guAssertBegin == guAssertEnd && guAssertEnd == guExpectedAsserts, "assert_balance"); Check(guUnexpected == 0, "native_arguments_valid");
    std::printf("XNetStartupHeld: %u checks, %u failures\n", guChecks, guFailures); return guFailures ? 1 : 0;
}
