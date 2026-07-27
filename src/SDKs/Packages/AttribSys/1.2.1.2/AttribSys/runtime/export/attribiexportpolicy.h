#pragma once

// ===========================================================================
// Attrib::IExportPolicy -- the abstract export-policy interface in the AttribSys
// export subsystem (a vendor library boundary in BURNOUT_X360_ARTIST.XEX,
// AttribSys v1.2.1.2). It is the polymorphic base the AttribSys ExportManager
// hands around; the concrete per-granularity policies (the ExportPolicy /
// ClassExportPolicy / CollectionExportPolicy / DatabaseExportPolicy family in
// attribexportmanager.{h,cpp}) provide the actual export behaviour.
//
// This header is the canonical OWNING home for:
//     Attrib::IExportPolicy::`scalar deleting destructor'  @ 0x82805760
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed from the X360 asm of that deleting-destructor thunk:
//
//   mflr/stw/std/stwu              ; standard frame
//   lis/addi r11, off_820D8E08     ; load IExportPolicy's vtable address
//   mr   r31, r3                   ; save this
//   clrlwi r10, r4, 31             ; r10 = should-free bit (a2 & 1)
//   stw  r11, 0(r31)               ; *this = vtable  (vptr store at +0)
//   beq  cr6, skip_free            ; if (should-free)
//   bl   operator_delete           ;   ::operator delete(this)   <-- GLOBAL, 1-arg
//   mr   r3, r31                   ;   return this
//   ...epilogue... ; blr
//
// This is MSVC's standard scalar deleting destructor synthesised from an empty
// `virtual ~IExportPolicy()`: it re-stamps the vptr then, on the low bit of the
// flag argument, frees the object through the GLOBAL `::operator delete(void*)`.
// (That GLOBAL single-argument free distinguishes IExportPolicy from the sibling
// concrete ExportPolicy family, whose own deleting-destructor thunks free through
// the AttribSys package allocator's census helper instead -- see
// attribexportmanager.cpp. They are separate classes; their identical vtables
// were merely folded to the same address on the X360.)
//
// `Attrib` is a vendor (AttribSys) library boundary, so its identifiers are
// preserved per the naming convention. The object owns no data members of its
// own (vptr only); concrete payload lives in the derived policy TUs.
// ===========================================================================

#include "types.hpp"

namespace Attrib
{
    class Vault;   // the vault a policy operates over (attribloadandgo.h)

    // The 64-bit granularity/type + export-instance keys (the attribloadandgo.h
    // typedefs; spelled here so this header stays standalone).
    typedef u64 TypeID;
    typedef u64 ExportID;

    class IExportPolicy
    {
    public:
        // @ 0x82805760 (scalar deleting destructor synthesised from this dtor) --
        // trivial: the interface owns no members. Defined out-of-line in the .cpp
        // so the vtable + the deleting-destructor thunk are emitted here.
        virtual ~IExportPolicy();

        // The full DWARF interface, in X360 vtable-slot order (DecFIGS
        // attribloadandgo.h:46-88; slot numbers verified against the concrete
        // policy vtables off_820D9630/654/700 dumped from the ARTIST .i64):
        //   [1] IsExported            [2] Initialize        [3] AnyReferences
        //   [4] IsReferenced          [5] PrepareToClean    [6] Clean
        //   [7] PrepareToDeinitialize [8] Deinitialize
        // A Vault dispatches [1] while counting its export slots (ctor
        // @0x8280A2E8), [2] while initializing its serialised exports
        // (Initialize @0x8280A660) and [8] while tearing them down
        // (Deinitialize @0x8280E6F0); the ExportManager fans out [7]
        // (@0x828031A8). Pure here -- the bodies live in the concrete
        // per-granularity policy TUs.
        virtual bool IsExported(const TypeID& lrType) = 0;
        virtual void Initialize(Vault& lrVault, const TypeID& lrType,
                                const ExportID& lrExport, void* lpData,
                                unsigned int luSize) = 0;
        virtual bool AnyReferences(const Vault& lrVault) = 0;
        virtual bool IsReferenced(const Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport) = 0;
        virtual void PrepareToClean(Vault& lrVault) = 0;
        virtual void Clean(Vault& lrVault, const TypeID& lrType,
                           const ExportID& lrExport) = 0;
        virtual void PrepareToDeinitialize(Vault& lrVault) = 0;
        virtual void Deinitialize(Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport) = 0;
    };
}
