#include <Windows.h>
#undef DrawText
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks, failures;
static void Check(bool value, const char* name) {
    ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++failures; return 0; }
void* EndAssert() { return nullptr; }
} }
int main() {
    using namespace CgsDev;
    std::atomic<int> phase{0};
    std::thread first([&] {
        *Log::gpDebugPrint << "hex=" << E_PRINTMODE_HEXONCE;
        phase = 1;
        while (phase.load() != 2) std::this_thread::yield();
        *Log::gpDebugPrint << u32(26) << ", next=" << u32(26) << "\n";
    });
    std::thread second([&] {
        while (phase.load() != 1) std::this_thread::yield();
        *Log::gpDebugPrint << "decimal=" << u32(26) << "\n";
        phase = 2;
    });
    first.join(); second.join();
    std::vector<std::thread> workers;
    for (unsigned t=0;t<4;++t) workers.emplace_back([t] {
        for (unsigned i=0;i<1000;++i) {
            *Log::gpDebugPrint << "thread=" << t << ", row=";
            std::this_thread::yield();
            *Log::gpDebugPrint << i << ", end=" << t << "\n";
        }
    });
    for (auto& worker : workers) worker.join();
    *Log::gpDebugPrint << "partial "; Log::WriteToLog("direct\n");
    std::string longLine(10000, 'x'); longLine += '\n';
    *Log::gpDebugPrint << longLine.c_str();
    char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string logPath(path); logPath.resize(logPath.find_last_of("\\/")+1); logPath += "BrnGame.log";
    FILE* input = std::fopen(logPath.c_str(), "rb");
    std::string log;
    if (input) {
        char buffer[4096]; size_t count;
        while ((count=std::fread(buffer,1,sizeof(buffer),input)) != 0) log.append(buffer,count);
        std::fclose(input);
    }
    log.erase(std::remove(log.begin(), log.end(), '\r'), log.end());
    Check(log.find("decimal=26\n") != std::string::npos, "decimal thread cannot consume another thread's HEXONCE");
    Check(log.find("hex=0x0000001A, next=26\n") != std::string::npos, "formatting state and partial line stay on their thread");
    unsigned complete = 0;
    for (unsigned t=0;t<4;++t) for (unsigned i=0;i<1000;++i) {
        std::string line="thread="+std::to_string(t)+", row="+std::to_string(i)+", end="+std::to_string(t)+"\n";
        auto at=log.find(line); if (at!=std::string::npos && log.find(line,at+line.size())==std::string::npos) ++complete;
    }
    Check(complete==4000, "all concurrent multi-piece lines are present exactly once");
    Check(log.find("partial direct\n") != std::string::npos, "direct write flushes the caller's partial line first");
    Check(log.find(longLine) != std::string::npos, "full line buffer flushes without truncation or overflow");
    std::printf("PCLogThreading: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
