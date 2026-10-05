#include <cstdio>
#include <cstring>
#include <string>
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks, failures, displays, handlers, flushes, criticalDepth;
static std::string output;
static uintptr_t resolvedAddress;
static void Check(bool value, const char* name) {
    ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsDev {
namespace Log {
CriticalLogScopePC::CriticalLogScopePC() { ++criticalDepth; }
CriticalLogScopePC::~CriticalLogScopePC() { --criticalDepth; }
void FlushLog() { ++flushes; }
StrStreamBase& DebugPrint::operator<<(const char* text) { if (!criticalDepth) ++failures; output += text; return *this; }
static DebugPrint print;
DebugPrint* gpDebugPrint = &print;
}
namespace MapFile {
Reader::Reader() : mpCallstack(nullptr) {}
void Reader::Prepare(const char*, StackUnpickBase* stack) { mpCallstack=stack; resolvedAddress=stack->GetStackAddress(0); }
void Reader::Update() {}
const char* Reader::GetStackEntryName(s32) { return "captured_worker_frame"; }
}
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++failures; return 0; }
void* EndAssert() { return nullptr; }
static MapFile::Reader gMinimalMemoryReader;
static const char* GetDefaultMapFilePath() { return "test.cgsmap"; }
static s32 NoteAssertSite(const char*, s32) { return 0; }
void Manager::DoAssert() { ++displays; }
void Manager::ExecuteAssertHandlers() { if (flushes <= handlers) ++failures; ++handlers; }
#include "assert_captured.inc"
} }
int main() {
    CgsDev::Assert::Manager manager;
    CgsDev::StackUnpick captured;
    captured.miNumStackAddresses=1; captured.maAdjustedStack[0]=0x12345678;
    manager.HandleAssertCapturedPC("worker failure", "worker.cpp", 7, captured, false);
    Check(displays==0 && !manager.HasAssert(), "headless report never opens or latches a dialog");
    Check(output.find("worker failure")!=std::string::npos && output.find("captured_worker_frame")!=std::string::npos &&
          output.find("EndCallstack")!=std::string::npos, "headless report includes the captured callstack");
    Check(resolvedAddress==0x12345678 && handlers==1, "resolution uses the failing worker's capture and runs handlers");
    manager.mbGotAssert=true;
    std::strcpy(manager.mCurrentAssert.macAssertMessage,"earlier failure");
    manager.mCurrentAssert.mStack.miNumStackAddresses=1;
    manager.mCurrentAssert.mStack.maAdjustedStack[0]=0x87654321;
    manager.HandleAssertCapturedPC("shutdown worker", "worker.cpp", 8, captured, false);
    Check(manager.HasAssert() && std::strcmp(manager.mCurrentAssert.macAssertMessage,"earlier failure")==0 &&
          manager.mCurrentAssert.mStack.GetStackAddress(0)==0x87654321, "shutdown logging preserves the first pending assertion");
    Check(resolvedAddress==0x12345678 && handlers==2, "shutdown report does not substitute the stale pending stack");
    Check(output.find("press END")==std::string::npos, "headless report does not ask for an unavailable dialog key");
    Check(flushes == 2, "each captured stack is drained before assert handlers run");
    std::printf("PCAssertCaptured: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
