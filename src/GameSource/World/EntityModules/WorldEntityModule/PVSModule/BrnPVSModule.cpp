#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX -- BrnWorld::PVSModule.
//
//   PVSModule()              @ 0x827E4CC8
//   GetInputInterface()      @ 0x822BAF78
//   GetOutputInterface()     @ 0x822BAFF0
//   GetGameDataRequestInt()  @ 0x822BB068
//
// The three interface getters each read the base ModuleSingleBuffered's
// GetInputStructure()/GetOutputStructure() into r31 and, on null, fire TWO streamed
// asserts (the inlined template tripwire "lpBuffer != NULL"
// CgsModuleSingleBufferedTemplate.h:73/82, then the PVS-level "lpInputBuffer" /
// "lpOutputBuffer" BrnPVSModule.h:122/130/138). CGS_ASSERT supplies __FILE__/__LINE__
// itself; the baked file/line are not reproduced.
// ============================================================================

namespace BrnWorld
{
    // X360 0x827E4CC8. The asm: set the base vtable, construct the two base RWMutexes
    // (this+0x10, this+0x118; RWMutex(NULL, true)), set the PVSModule vtable, clear the
    // flag byte (stb 0 @ +0x1B58), then seed the empty circular list-head sentinel
    // (@ +0x1D78). The base + member constructors handle the mutexes and the buffers;
    // PVSModule's own ctor only initialises mbPvsFlag and mPvsList.
    PVSModule::PVSModule()
        : mbPvsFlag(false)   // stb 0 @ this+0x1B58
        // mPvsList default-constructs to the empty self-referential sentinel (@ this+0x1D78).
    {
    }

    // X360 0x822BAF78. r31 = GetInputStructure(); if null, fire the two tripwires; return r31.
    PVSIO::InputBuffer* PVSModule::GetInputInterface()
    {
        // SafeGetInputStructure() inline-expands the first tripwire CGS_ASSERT(lpBuffer,
        // "lpBuffer != NULL") (template line 73); then the PVS-level assert (.h:122).
        PVSIO::InputBuffer* lpInputBuffer = SafeGetInputStructure();
        CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer");
        return lpInputBuffer;
    }

    // X360 0x822BAFF0. r31 = GetOutputStructure(); if null, fire the two tripwires; return r31.
    PVSIO::OutputBuffer* PVSModule::GetOutputInterface()
    {
        // SafeGetOutputStructure() inline-expands the first "lpBuffer != NULL" tripwire
        // (template line 82); then the PVS-level assert (.h:130).
        PVSIO::OutputBuffer* lpOutputBuffer = SafeGetOutputStructure();
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        return lpOutputBuffer;
    }

    // X360 0x822BB068. r31 = GetOutputStructure(); if null, fire the two tripwires; return
    // r31 + 0x1718 (the embedded game-data request interface, &mGameDataRequestInterface).
    BrnResource::GameDataIO::RequestInterface<512>* PVSModule::GetGameDataRequestInt()
    {
        // SafeGetOutputStructure() inline-expands the first "lpBuffer != NULL" tripwire
        // (template line 82); then the PVS-level assert (.h:138).
        PVSIO::OutputBuffer* lpOutputBuffer = SafeGetOutputStructure();
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        return &lpOutputBuffer->mGameDataRequestInterface;   // addi r3, OutputStructure, 0x1718
    }
}

// ============================================================================
// Explicit instantiation of the module's SafeLockOutputForWrite() @ 0x822AA000.
//   The X360 build emitted this template method out-of-line (IDA symbol truncated
//   to "In"). asm: r31 = LockOutputForWrite(); if null, fire the single
//   "lpBuffer != NULL" tripwire (CgsModuleSingleBufferedTemplate.h:64 baked path);
//   return r31 reinterpret_cast<PVSIO::OutputBuffer*>. Called by
//   BrnWorld::PVSModule::Prepare. The generic body is inline in
//   CgsModuleSingleBufferedTemplate.h (SafeLockOutputForWrite == LockOutputForWrite +
//   one CGS_ASSERT(lpBuffer,"lpBuffer != NULL") + return); this TU forces the
//   per-instantiation emission for <PVSIO::InputBuffer, PVSIO::OutputBuffer>.
// ============================================================================
template BrnWorld::PVSIO::OutputBuffer*
CgsModule::ModuleSingleBufferedTemplate<BrnWorld::PVSIO::InputBuffer,
                                        BrnWorld::PVSIO::OutputBuffer>::SafeLockOutputForWrite();
