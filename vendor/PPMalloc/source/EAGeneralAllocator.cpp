// EA::Allocator::GeneralAllocator implementation.
//
// CANONICAL TU: ../src/EAGeneralAllocator.cpp.
//
// This file (vendor/PPMalloc/source/EAGeneralAllocator.cpp) used to hold a SECOND, partial copy
// of the GeneralAllocator bodies (ctor/dtor + PPMAutoMutex + MallocAligned). Keeping two .cpp
// files that each define those methods is an ODR violation, so this duplicate was emptied: every
// body it carried now lives in the canonical src/ TU (the PPMAutoMutex lock and the
// lock+delegate MallocAligned were merged there; the ctor/dtor are the canonical full versions).
//
// Intentionally empty - do not re-add definitions here. Edit ../src/EAGeneralAllocator.cpp.
