#include "types.hpp"

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleIO.cpp
//
// RETIRED 2026-09-25 (FX-DIRECTOR2). This TU held a LOCAL fork of
// BrnDirector::DirectorIO::SceneQueryOutputBuffer (and of CgsModule::IOBuffer) so that it could
// body SceneQueryOutputBuffer::Destruct @0x8221B3F8 by raw word offsets. It was never mounted.
// The DWARF home of the director's IO-buffer Construct/Destruct bodies is this file
// (BrnDirectorModuleIO.cpp:192 / :207 are the asserts), but the real bodies now live in the
// MOUNTED BrnDirectorModuleIOSceneQuery.cpp, next to the real layouts in
// BrnDirectorModuleIOSceneQuery.h -- rather than adding a mount line for this file.
// ============================================================================
