#pragma once

// ObjectToMeshJob — the renderer's "convert a dispatch object into mesh draw work" job
// (dispatched from BrnRendererModule via maObjectToMeshJob / the ObjectToMeshEntry trampoline).
//
// Only the Execute entry shim is bodied in this TU: it latches the job's input pointer into
// the job object, then runs the (heavy) conversion in ExecuteImplementation. The conversion
// body lives in its own TU; it is declared-only here so Execute can name it.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, Execute @0x82926A70):
//   this[0] @ +0x00 -- mpInput : the input pointer stored by Execute (stw r4, 0(r3))
//
// Execute is `stw r4,0(r3); b ObjectToMeshJob__ExecuteImplementation` -- it stores the
// argument at +0x00 and tail-calls ExecuteImplementation (the return value is whatever
// ExecuteImplementation returns; r3 flows through the tail-call).

#include "types.hpp"

struct ObjectToMeshJob
{
    // Execute @0x82926A70 — latch the input pointer, then run the conversion.
    int Execute(void* apInput);

    // ExecuteImplementation — the conversion body (declared here; bodied in its own TU).
    int ExecuteImplementation();

    void* mpInput; // +0x00  (asm: stw r4, 0(r3))
};
