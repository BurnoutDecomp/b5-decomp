// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h
//
// Canonical (DWARF) home for the BrnWorld::PropEntityIO prop-entity IO types
// (BrnPropEntityModuleIO.h). This is a MINIMAL-COMPLETE slice: it currently homes only
// PropVFXLocatorEvent -- the element type of the EventQueue<PropVFXLocatorEvent,10>
// instantiation whose Construct the X360 emitted out-of-line (0x8228DB20). The many
// other PropEntityIO types/buffers in the DWARF (OutputBuffer_*, the other event
// structs, the queue typedefs) are intentionally NOT reproduced here yet; their own
// TUs grow this header ADDITIVELY when they land.
//
// Member names/types and the EEventType enumerators are from the DecFIGS DWARF
// (BrnPropEntityModuleIO.h:228/268..270), X360-gated. Layout (DWARF-faithful):
//   +0   Matrix44Affine mTransform     (rw::math::vpu::Matrix44Affine, 64B, 16-aligned)
//   +64  u32            muTypeId
//   +68  EEventType     meEventType    (s32)
// => sizeof 80 (16-aligned because Matrix44Affine forces alignas(16)); the event has
// no trailing pad beyond natural alignment. The owning EventQueue<...,10> places its
// inline maEvents[10] at the 16-aligned offset +16 inside the queue (mpEvents@0,
// miMaxLength@4, miLength@8, 4B pad, maEvents@16) -- matching X360 Construct's
// `lpEventBuffer = this + 0x10` and the stores mpEvents=&maEvents, miMaxLength=10,
// miLength=0.
#pragma once

#include "types.hpp"          // u32, s32
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu::Matrix44Affine)

namespace BrnWorld
{
namespace PropEntityIO
{
    // BrnPropEntityModuleIO.h:66 -- the PropVFXLocator output queue capacity.
    const u32 KU_PROP_VFX_QUEUE_SIZE = 10;

    // BrnPropEntityModuleIO.h:225 -- a VFX-locator hit/smash event published per prop
    // impact: the prop's affine world transform, its type id, and which event fired.
    struct PropVFXLocatorEvent
    {
        // BrnPropEntityModuleIO.h:228
        enum EEventType : s32
        {
            E_EVENTTYPE_PROPHIT  = 0,
            E_EVENTTYPE_PROPSMASH = 1,
            E_EVENTTYPE_MAX      = 2
        };

        // BrnPropEntityModuleIO.h:242 -- own TU (declared-only here).
        void Construct(Matrix44Affine lTransform, u32 luTypeId, EEventType leEventType);
        // BrnPropEntityModuleIO.h:250 -- own TU.
        const Matrix44Affine& GetTransform() const;
        // BrnPropEntityModuleIO.h:256 -- own TU.
        u32 GetPropType() const;
        // BrnPropEntityModuleIO.h:262 -- own TU.
        EEventType GetEventType() const;

    private:
        Matrix44Affine mTransform;   // :268
        u32            muTypeId;     // :269
        EEventType     meEventType;  // :270
    };
}
}
