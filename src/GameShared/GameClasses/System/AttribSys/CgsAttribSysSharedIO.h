#ifndef GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIO_H
#define GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIO_H

// ============================================================================
// GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h
//
// DWARF home of CgsAttribSys::AttribSysIO::AttribSysRequestQueue<N> and
// AttribSysRequestInterface<N> -- the typed vault/schema request-builder interface
// callers use to enqueue requests into the AttribSys module's input buffer (shape
// mirrored from the committed sibling BrnResource::GameDataIO::RequestInterface<N>).
//
//   AttribSysRequestInterface<N> { AttribSysRequestQueue<N> mRequestQueue; }  (only member @0x00)
//   AttribSysRequestQueue<N>     : public CgsModule::VariableEventQueue<N,16>
//
// The templated queue/interface + method DECLARATIONS actually live in
// CgsAttribSysModuleIO.h (so that InputBuffer, which embeds a complete
// AttribSysRequestInterface<2048>, can name it without a circular include: this header
// includes CgsAttribSysModuleIO.h, not the other way round). This header is the DWARF
// canonical home name -- it re-exports those declarations so request-builder TUs and the
// generic-body impl header can include the interface by its DWARF home path. The generic
// method bodies live in CgsAttribSysSharedIOImpl.h.
//
// The request EVENT structs (RegisterVaultRequest, RegisterSchemaRequest,
// UnregisterVaultRequest, SchemaRegisteredResponse), the EAttribSysVaultType enum, and the
// KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE typedef all live in CgsAttribSysModuleIO.h.
// ============================================================================

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"

#endif // GAMESHARED_GAMECLASSES_SYSTEM_ATTRIBSYS_CGSATTRIBSYSSHAREDIO_H
