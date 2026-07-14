#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet base)

// BrnGame::BrnGameModuleIO - the per-frame IO payload buffers the top-level game module
// exchanges on its update IO stacks (the same *ModuleIO namespace-of-InputBuffer/OutputBuffer
// shape as CgsGui::CgsGuiModuleIO / CgsGui::ModelIO / CgsInput::InputIO, whose InputBuffers all
// derive CgsModule::IOBuffer). This slice homes the single X360-emitted InputBuffer member the
// ledger attributes to class:BrnGame::BrnGameModuleIO (below); OutputBuffer and the remaining
// InputBuffer accessors are not attested in this TU and are intentionally omitted (adding them
// would be invented layout).
namespace BrnGame
{
namespace BrnGameModuleIO
{
    // The game module's per-frame input IO buffer. Derives CgsModule::IOBuffer (the 1-byte
    // status FlagSet base: bit 3 == locked-for-write, bit 4 == locked-for-read), mirroring every
    // committed *ModuleIO::InputBuffer sibling. No additional members are declared: the single
    // reconstructed function below does not touch `this`, so no member offset is attested and none
    // is invented (per the reconstruction rules).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x827E3100.  ledger identity "BrnGame::BrnGameModuleIO::InputBuffer,cla" -- the IDA
        // symbol is TRUNCATED (the trailing ",cla..." is the cut-off tail of a longer demangled
        // name, exactly like the "Crea" -> CreateStreamProducer truncation in
        // CgsCollisionGenerator.h), so the source-level method name is unrecoverable; DestroyBuffer
        // is a behaviour-derived reconstruction and is FLAGGED as such.
        //
        // ASM (store-for-store):  mr r3,r4 ; bl <Destruct> ; li r3,1 ; blr
        //   -> `this` (r3) is IGNORED; the second argument (r4) is moved into r3 and passed to an
        //      empty destructor, then the function returns 1.
        //   -> The `bl` target is resolved by IDA to
        //      CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct (@0x8284CB38), which
        //      is documented as "empty; ICF-folded". BaseCollisionGenerator IS-A CgsModule::IOBuffer
        //      and its Destruct body is empty, so it FOLDS (identical-code-folding) with the equally
        //      empty CgsModule::IOBuffer::Destruct; the linker keeps one copy and labels it with the
        //      collision name. The behaviour-faithful callee for this game-module IO context is the
        //      base CgsModule::IOBuffer::Destruct (the empty fold representative), invoked on the
        //      passed buffer -- so the argument is modelled as CgsModule::IOBuffer*.
        //
        // FLAG: (1) method name reconstructed from behaviour (symbol truncated); (2) the argument's
        // exact static type cannot be pinned because the callee is an ICF-folded empty destructor --
        // it is modelled as CgsModule::IOBuffer* (the fold representative), which reproduces the
        // emitted branch + return-1 store-for-store without inventing a collision relationship.
        int DestroyBuffer(CgsModule::IOBuffer* lpBuffer);
    };
}
}
