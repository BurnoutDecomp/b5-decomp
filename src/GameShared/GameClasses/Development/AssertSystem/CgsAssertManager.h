#pragma once

#include "types.hpp"

namespace CgsDev { namespace Assert { struct AssertHandler; } }

namespace CgsDev
{
namespace Assert
{
    // CgsDev::Assert::Manager - the assert handler. The X360 Manager (CgsAssertManager.cpp)
    // captures the callstack, runs registered AssertHandlers, halts the other threads, then
    // loops forever in an on-screen assert dialog (DisplayAssertScreen). This reconstruction
    // keeps the entry path (HandleAssert) but routes the report to a LOG FILE and continues
    // (no thread halt, no debug break) per the build configuration; the on-screen-dialog
    // members (Debug2DImmediateRender / MapFile reader / DisplayAssertScreen / callstack)
    // are reconstructed with the debug UI. Method names from the DecFIGS DWARF
    // (Development/AssertSystem/CgsAssertManager.h).
    class Manager
    {
    public:
        Manager();

        void HandleAssert(const char* lpcExpression, const char* lpcFile, s32 liLine);
        void RegisterAssertHandler(AssertHandler* lpHandler);

    private:
        void ExecuteAssertHandlers();

        s32 miAssertCount;
        // [on-screen-dialog state omitted - replaced by the log-file path]
    };

    extern Manager gAssertManager;
}
}
