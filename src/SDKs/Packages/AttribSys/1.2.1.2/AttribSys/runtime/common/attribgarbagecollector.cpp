#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"

// Home TU for class:Attrib::IGarbageCollector -- the AttribSys host garbage-collector
// callback interface (declared in attribloadandgo.h). This TU owns the single X360
// ledger function attested for the class:
//
//   Attrib::IGarbageCollector::`scalar deleting destructor'  @ 0x827DBA70
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 asm is the compiler's
// deleting-destructor thunk (MSVC ??_G) for the virtual base destructor:
//
//     Attrib::IGarbageCollector::~IGarbageCollector();   // run the real dtor
//     if (deleteFlag & 1)                                // scalar `delete this` variant
//         operator delete(this);
//     return this;
//
// (asm @ 0x827DBA70: bl ~IGarbageCollector; clrlwi r11,r30,31 / beq; bl operator_delete;
//  the result register r3 is restored to `this` on both the free and no-free paths.)
//
// In C++ this thunk is synthesised automatically by MSVC from an OUT-OF-LINE virtual
// destructor -- the vptr install is implicit in the destructor prologue and the
// conditional `operator delete` is the flag-bit-0 path of the compiler-emitted thunk.
// Defining ~IGarbageCollector() out-of-line here (the header declares it non-inline)
// makes the compiler emit exactly the @0x827DBA70 thunk and pins the interface's
// vtable to this TU. No hand-written vtable reinterpret is written (and none is
// allowed). Mirrors the committed sibling homes CgsNetwork::ServerInterfaceEndGameDataBase
// (CgsServerInterfaceEndGameData.cpp @ 0x82558ED8) and Attrib::Vault_ScalarDeletingDtor
// (attribloadandgo.cpp @ 0x8280F098).
//
// The interface owns no members and holds no resources, so the destructor body is
// empty; the only observable effect of the ledger function is the base-vptr install
// and the conditional free that the synthesised thunk supplies. ReleaseData is pure
// virtual (bodied by concrete hosts such as AttribSysGarbageCollector in their own TUs).

Attrib::IGarbageCollector::~IGarbageCollector()
{
}
