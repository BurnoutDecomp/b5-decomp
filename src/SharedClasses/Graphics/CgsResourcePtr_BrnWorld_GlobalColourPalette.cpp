#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"   // complete BrnWorld::GlobalColourPalette

// Per-instantiation TU for CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->()  @ 0x822C9A48  (baked assert line 544, non-const,
//                                "Can not instance resource pointer - it has no
//                                main memory resource"; asserts mpResourceMemory
//                                non-null then returns it as GlobalColourPalette*)
//
// The X360 body is: if (!mpResourceMemory) Begin/Fire/EndAssert(...); return
// mpResourceMemory;  -- i.e. the generic ResourcePtr<Type>::operator->() with
// Type = BrnWorld::GlobalColourPalette. Eleven call sites in
// BrnWorld::RaceCarEntityModule dereference this palette pointer.
template BrnWorld::GlobalColourPalette*
CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>::operator->();

// operator->() const  @ 0x824BD480  (BrnGui::WorldDataController::GetColourPaletteFromType, a
// CONST method -> const-qualified accessor). X360 body: if (!mpResourceMemory) fire the
// 'Can not instance resource pointer - it has no main memory resource' assert (baked
// CgsResourcePtr.h:563), then `lwz r3,0(this)` returns mpResourceMemory as
// const GlobalColourPalette*. Baked line 563 + the 'instance' message + the const caller pin this
// to operator->() const (the non-const operator*() cannot be called from a const method; and
// operator*() const bakes to line 637 with the 'dereference' message). Generic body inline in
// CgsResourcePtr.h; this is the thin explicit instantiation.
template const BrnWorld::GlobalColourPalette*
CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>::operator->() const;

// BLOCKED: BrnWorld::GlobalColour @ 0x82368FC8 is NOT a thin instantiation of the generic
// ResourcePtr<GlobalColourPalette>::ResourcePtr(const ResourceHandle&). The X360 body zero-inits
// the BaseResourcePtr fields then calls CreateFromHandle with the argument ADJUSTED BY +0x14
// (`addi r4,r4,0x14` at 0x82368FE0, INSIDE the callee). CreateFromHandle consumes a bare 8-byte
// const ResourceHandle* at +0/+4, so the +0x14 means the incoming argument is an enclosing object
// whose ResourceHandle member sits at +0x14 -- a ctor/factory over that owner type, which the
// generic 8-byte-handle ctor cannot express. Blocked pending the +0x14-embedding owner type
// (caller BrnGameState::GameStateModule::Prepare passes a resource-request struct). Not emitted --
// a `template ...::ResourcePtr(const ResourceHandle&);` line would emit CreateFromHandle(&h) at
// offset 0, dropping the attested +0x14 and contradicting the asm.
