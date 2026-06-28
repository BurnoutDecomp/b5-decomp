#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3Plus (rw::math::vpu::Vector3Plus, 16B)

// Renderable.h -- the global ::Renderable dispatch object.
//
// Reconstructed from the DecFIGS DWARF (GameShared/GameClasses/Graphics/Dispatch/Renderable.h)
// plus the X360 asm for Renderable::UsesTexture @ 0x827E9A68 (bodied in renderable.cpp).
// Only the data layout and the UsesTexture accessor needed to compile that function are
// reconstructed here; the remaining DWARF methods (Initialize / Release /
// GetResourceDescriptor / GetObjectScopeTexture / HasObjectScopeTextures) are left to their
// own TUs.
//
// ::Renderable is at namespace (global) scope: the X360 ledger name is `Renderable::UsesTexture`
// and the DWARF declares the struct unqualified. NOTE: CgsModel.h currently forward-declares
// it as `CgsGraphics::Renderable`, which looks like a stale namespace guess to reconcile when
// that TU is next touched (Model holds Renderable** by pointer, so it compiles either way).
//
// X360 member layout (32-bit) confirmed against the UsesTexture asm; the PC host pointer
// width differs, so members are accessed BY NAME, never by a pinned byte offset:
//   +0x00 mBoundingSphere           Vector3Plus              (16B)
//   +0x10 mu16VersionNumber         u16
//   +0x12 mu16NumMeshes             u16                       (lhz 0x12(this))
//   +0x14 mppMeshes                 RenderableMesh**          (lwz 0x14(this))
//   +0x18 mpObjectScopeTextureInfo  ObjectScopeTextureInfo*   (lwz 0x18(this))
//   +0x1C mu16Flags                 u16

namespace renderengine { class Texture; }

struct RenderableMesh;   // complete type in renderablemesh.h (consumers dereference it)

struct Renderable
{
    // DWARF Renderable.h:119 -- the object-scope texture set bound to this renderable,
    // indexed by purpose. UsesTexture reads mu16NumTextures (+0x00) and mppTextures (+0x08).
    struct ObjectScopeTextureInfo
    {
        u16                     mu16NumTextures;            // +0x00
        u16                     mu16MaxNumTextures;         // +0x02
        s16*                    mpi16PurposeToIndexMapping; // +0x04
        renderengine::Texture** mppTextures;                // +0x08
        char**                  mppcTextureNames;           // +0x0C
    };

    // DWARF Renderable.h:164 @ 0x827E9A68 -- true if lpTexture is bound at this renderable's
    // object scope or sampled by any of its mesh material assemblies.
    bool UsesTexture(const renderengine::Texture* lpTexture) const;

    Vector3Plus             mBoundingSphere;          // +0x00
    u16                     mu16VersionNumber;        // +0x10
    u16                     mu16NumMeshes;            // +0x12
    RenderableMesh**        mppMeshes;                // +0x14
    ObjectScopeTextureInfo* mpObjectScopeTextureInfo; // +0x18
    u16                     mu16Flags;                // +0x1C
};
