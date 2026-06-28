#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptValue.
//
// Reconstructed from the Feb-2007 leak SHAPE
// (SDKs/Packages/Apt/2.00.00/include/Apt/AptValue/AptValue.h) for the type
// layout, accessor inlines and the enum, and from the X360 ARTIST.XEX
// pseudocode for the two out-of-line virtual BODIES this TU owns:
//     AptValue::DeleteThis    @ 0x824ED4B8
//     AptValue::ForceDelete   @ 0x824ED4D8
//
// In the leak DeleteThis()/ForceDelete() are inline virtuals (`delete this`
// and PreDestroy()/DestroyGCPointers()/`delete this`). The X360 compiler
// emitted them out-of-line; the pseudocode confirms the leak bodies verbatim:
//   DeleteThis : if(this) <scalar deleting destructor>(this, /*delete*/1)
//                (the codegen of `delete this`; vtbl slot +0x38)
//   ForceDelete: PreDestroy() [vtbl +0x24]; DestroyGCPointers() [vtbl +0x28];
//                delete this [vtbl +0x38].
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptValue, the
// AptVirtualFunctionTable_Indices enumerators, getRefCount, mValueBitfield,
// ...) are an external/middleware API and kept verbatim.
//
// Only the surface needed by the owned bodies + the GC sibling TUs is
// reconstructed here; the very large remainder of the leak class (the dozens of
// c_*/is*/toX members) is left out as an honest minimal owning header rather
// than fabricated.
// ===========================================================================

#include <cstdint>

// ---------------------------------------------------------------------------
// AptValue virtual-function-table object-type indices (leak AptValue.h:40).
// Values pinned by the leak's DWARF (explicit = ... in the dossier).
// ---------------------------------------------------------------------------
enum AptVirtualFunctionTable_Indices
{
    AptVFT_xxx = 0,
    AptVFT_StringValue = 1,
    AptVFT_Property = 2,
    AptVFT_None = 3,
    AptVFT_Register = 4,
    AptVFT_Boolean = 5,
    AptVFT_Float = 6,
    AptVFT_Integer = 7,
    AptVFT_Lookup = 8,
    AptVFT_NativeFunction = 9,
    AptVFT_FrameStack = 10,
    AptVFT_Extern = 11,
    AptVFT_CharacterInstHandle = 12,
    AptVFT_Sound = 13,
    AptVFT_Array = 14,
    AptVFT_Math = 15,
    AptVFT_Key = 16,
    AptVFT_Global = 17,
    AptVFT_ScriptColour = 18,
    AptVFT_Object = 19,
    AptVFT_Prototype = 20,
    AptVFT_Date = 21,
    AptVFT_MovieClip = 22,
    AptVFT_Mouse = 23,
    AptVFT_XmlNode = 24,
    AptVFT_Xml = 25,
    AptVFT_XmlAttributes = 26,
    AptVFT_LoadVars = 27,
    AptVFT_TextFormat = 28,
    AptVFT_Extension = 29,
    AptVFT_GlobalExtension = 30,
    AptVFT_Stage = 31,
    AptVFT_Error = 32,
    AptVFT_StringObject = 33,
    AptVFT_ScriptFunction1 = 34,
    AptVFT_ScriptFunction2 = 35,
    AptVFT_ScriptFunctionByteCodeBlock = 36,
    AptVFT_CIHNone = 37,

    AptVFT_NumVFTs = 38
};

// ---------------------------------------------------------------------------
// AptValue -- the polymorphic ActionScript value base.
//
// Layout (leak AptValue.h:554-571): a single vtable pointer followed by the
// 32-bit packed mValueBitfield / mnValueData union. The bitfield order is the
// leak's (low bit first); kept verbatim because it is the on-binary layout.
// ---------------------------------------------------------------------------
class AptValue
{
public:

    // ---- leak reference-count virtuals (AptValue.h:482-483) --------------
    // Bodies owned by other Apt TUs; declared so the operand-stack sibling can
    // Release popped values by name. vtbl slots 0 / +4.
    virtual void AddRef();
    virtual void Release();

    // ---- out-of-line owned bodies (this TU; defined in AptValue.cpp) -----
    virtual void DeleteThis();

    virtual void PreDestroy()
    {
    }

    virtual void DestroyGCPointers()
    {
    }

    virtual void ForceDelete();

    // ---- leak inline accessors (kept verbatim for the GC siblings) -------
    uint32_t getRefCount() const
    {
        return mValueBitfield.mnReferenceCount;
    }

    AptVirtualFunctionTable_Indices getVtblIndex() const
    {
        return mValueBitfield.meValueType;
    }

    bool getIsDefined() const
    {
        return mValueBitfield.mbIsDefined != 0;
    }

    bool getGCMark() const
    {
        return mValueBitfield.mbHasRegisterReferenceMark != 0;
    }

    uint32_t getGCRoot() const
    {
        return mValueBitfield.mnGCRootCount;
    }

    bool GetMaxRefCountHit() const
    {
        return mValueBitfield.mnMaxRefCountHit;
    }

    void SetMaxRefCountHit(bool bFlag)
    {
        mValueBitfield.mnMaxRefCountHit = (uint32_t)(bFlag ? 1 : 0);
    }

    bool IsReleaseAtEnd() const
    {
        return mValueBitfield.mbIsInDeferredVector != 0;
    }

    bool getAllowsDelayedDeletion() const
    {
        return mValueBitfield.mbAllowsDelayedDeletion != 0;
    }

    // ---- leak setters (AptValue.h:228-236) -- trivial bitfield writes (the
    // inverse of the getters above). The leak declares them out-of-line; they are
    // the obvious field stores, kept inline here alongside the inline getters.
    void setRefCount(uint32_t n)
    {
        if (n >= MAX_REFCOUNT)
        {
            mValueBitfield.mnReferenceCount = MAX_REFCOUNT;
            mValueBitfield.mnMaxRefCountHit = 1;
        }
        else
        {
            mValueBitfield.mnReferenceCount = n;
        }
    }

    void setVtblIndex(AptVirtualFunctionTable_Indices n) { mValueBitfield.meValueType = n; }
    void setIsDefined(bool bDefined) { mValueBitfield.mbIsDefined = bDefined ? 1u : 0u; }

    // ---- GC-required pure virtuals (leak AptValue.h:547-548) -------------
    virtual bool IsGarbageCollected() const = 0;
    virtual void RegisterReferences() = 0;

    static const uint32_t MAX_REFCOUNT = 0xfff;
    static const uint32_t MAX_GCROOT = 0x3f;

    union
    {
        struct
        {
            uint32_t mbIsAllocated:1;
            uint32_t mbHasRegisterReferenceMark:1;
            uint32_t mbIsInDeferredVector:1;
            uint32_t mbDestroyedGC:1;
            uint32_t mbIsDefined:1;
            uint32_t mbAllowsDelayedDeletion:1;
            uint32_t mnReferenceCount:12;
            uint32_t mnGCRootCount:6;
            uint32_t mnMaxRefCountHit:1;
            enum AptVirtualFunctionTable_Indices meValueType:7;
        } mValueBitfield;

        uint32_t mnValueData;
    };

protected:

    enum CIH_ONLY
    {
        CO_CIH,
    };

    AptValue(AptVirtualFunctionTable_Indices eType);
    AptValue(AptVirtualFunctionTable_Indices eType, const CIH_ONLY eCIH);

    virtual ~AptValue()
    {
    }
};

// ---------------------------------------------------------------------------
// AptValueGC / AptValueNoGC -- the two AptValue bases the concrete value types
// derive from (leak AptValue.h:626-687). AptValueGC participates in the Apt
// garbage collector (concrete GC types still supply RegisterReferences, so it
// stays abstract); AptValueNoGC opts out (both GC virtuals satisfied here, so
// the non-GC leaves -- AptBoolean/AptFloat/AptInteger/AptString -- only add
// their payload + pool new/delete). Bodies are the leak's inline definitions.
// ---------------------------------------------------------------------------
class AptValueGC : public AptValue
{
protected:
    AptValueGC(AptVirtualFunctionTable_Indices eType) : AptValue(eType) {}
    AptValueGC(AptVirtualFunctionTable_Indices eType, const CIH_ONLY eCIH) : AptValue(eType, eCIH) {}

    virtual bool IsGarbageCollected() const { return true; }

    static void VerifyAptValueGC() {}
};

class AptValueNoGC : public AptValue
{
protected:
    AptValueNoGC(AptVirtualFunctionTable_Indices eType) : AptValue(eType) {}

    virtual bool IsGarbageCollected() const { return false; }
    virtual void RegisterReferences() {}

    static void VerifyAptValueNoGC() {}
};
