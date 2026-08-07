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

#include <cstddef>   // offsetof (_AssertLayout)
#include <cstdint>

class EAStringC;                  // SDKs/EATech/include/Apt/AptString/EAString.h
typedef EAStringC AptNativeString;
struct AptNativeHash;            // SDKs/EATech/include/Apt/AptNativeHash.h

// The concrete value types the conversion / c_* cast layer returns (full defs in
// AptValue/AptInteger.h, AptFloat.h, AptBoolean.h, AptString.h).
class AptInteger;
class AptFloat;
class AptBoolean;
class AptString;

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

    // ---- leak reference-count virtuals (AptValue.h:171-172) --------------
    // Bodies owned by other Apt TUs; declared so the operand-stack sibling can
    // Release popped values by name.
    virtual void AddRef();
    virtual void Release();

    // ---- object-model virtuals (leak AptValue.h:435-482) -----------------
    // The full AptValue polymorphic interface, in the leak's declaration order
    // (so the value/object/CIH subclasses can override by name and MSVC builds a
    // consistent vtable). Defaults are the leak's: a plain AptValue holds no
    // native-hash property table and has no class/members. AptValueWithHash/
    // AptObject/AptCIH override these.
    virtual AptNativeHash* GetNativeHashVirtual()        { return 0; }
    virtual bool           ContainsNativeHashVirtual() const { return false; }
    virtual bool           GetHasClass() const           { return false; }
    virtual void           SetHasClass(int)              {}
    virtual AptValue*      objectMemberLookup(AptValue* const, const AptNativeString* const) const { return 0; }
    virtual bool           objectMemberSet(AptValue* const, const AptNativeString* const, AptValue* const) { return false; }

    // ---- out-of-line owned bodies (this TU; defined in AptValue.cpp) -----
    virtual void DeleteThis();

    virtual void PreDestroy()
    {
    }

    virtual void DestroyGCPointers()
    {
    }

    virtual void ForceDelete();

    // ---- GC mark / deferred-release flag mutators (under the Apt GC lock) -
    // setGCMark @0x82AD8020 -- set the register-reference mark
    // (mbHasRegisterReferenceMark); paired with getGCMark(). Used by the GC mark
    // walk (AptGC::sReferenceRegistrationCb).
    void setGCMark(bool bMark);
    // ClearReleaseAtEnd @0x82AD82A8 -- clear the "in deferred-release vector"
    // flag (mbIsInDeferredVector); used by the deferred-release vector flush.
    void ClearReleaseAtEnd();
    // incGCRoot @0x82AD8120 / decGCRoot @0x82AD81A8 -- adjust mnGCRootCount (clamped
    // [0,MAX_GCROOT]) under the GC flag lock. SetReleaseAtEnd @0x82AD8230 -- set
    // mbIsInDeferredVector under the lock (the inverse of ClearReleaseAtEnd).
    void incGCRoot();
    void decGCRoot();
    void SetReleaseAtEnd();

    // When set, AptGC::CleanAll suspends refcount-driven deletions while it runs
    // PreDestroy()/DestroyGCPointers() over the live value graph. (X360 byte_8324E38E.)
    static bool sbSuspendRefcountDeletions;

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

    // ---- leak setters (AptValue.h:228-236) -- lock-guarded bitfield writes.
    // The X360 out-of-line bodies (setRefCount @0x82AD7E90, setVtblIndex
    // @0x82AD7F20, setGCRoot @0x82AD80A8, SetAllowDelayedDeletion @0x82AD7E08)
    // each take the Apt GC flag lock (unk_8324E75C) around the store, exactly
    // like setGCMark. Bodies in AptValue.cpp.
    void setRefCount(uint32_t n);
    void setVtblIndex(AptVirtualFunctionTable_Indices n);
    void setGCRoot(uint32_t n);
    void SetAllowDelayedDeletion(bool bAllowed);

    void setIsDefined(bool bDefined) { mValueBitfield.mbIsDefined = bDefined ? 1u : 0u; }

    // ---- value-type predicates (leak AptValue.h is*/CanCreateScriptObject) ----
    // Each tests meValueType (+ mbIsDefined where the asm reads it). Reconstructed
    // from the X360 ARTIST pseudocode/asm; bit math folded back to named members.
    bool isString() const
    {
        return (getVtblIndex() == AptVFT_StringValue || getVtblIndex() == AptVFT_StringObject)
            && getIsDefined();
    }

    bool isInteger() const
    {
        return getVtblIndex() == AptVFT_Integer && getIsDefined();
    }

    bool isFloat() const
    {
        return getVtblIndex() == AptVFT_Float && getIsDefined();
    }

    bool isRegister() const
    {
        return getVtblIndex() == AptVFT_Register && getIsDefined();
    }

    bool isLookup() const
    {
        return getVtblIndex() == AptVFT_Lookup && getIsDefined();
    }

    bool isNativeFunction() const
    {
        return getVtblIndex() == AptVFT_NativeFunction && getIsDefined();
    }

    bool isFrameStack() const
    {
        return getVtblIndex() == AptVFT_FrameStack && getIsDefined();
    }

    bool isExtern() const
    {
        return getVtblIndex() == AptVFT_Extern && getIsDefined();
    }

    // x64 ?isCIH@AptValue@@QEBA_N_N@Z @0x1400C1540: ONLY type 12 (CharacterInstHandle)
    // gated on (bUndefOK || defined). The earlier CIHNone(37) second clause does not
    // exist in the x64 body -- callers that accept CIHNone test it themselves.
    bool isCIH(bool bUndefOK = false) const
    {
        return getVtblIndex() == AptVFT_CharacterInstHandle && (bUndefOK || getIsDefined());
    }

    bool isSound() const
    {
        return getVtblIndex() == AptVFT_Sound && getIsDefined();
    }

    bool isArray() const
    {
        return getVtblIndex() == AptVFT_Array && getIsDefined();
    }

    bool isMath() const
    {
        return getVtblIndex() == AptVFT_Math && getIsDefined();
    }

    bool isKey() const
    {
        return getVtblIndex() == AptVFT_Key && getIsDefined();
    }

    bool isMouse() const
    {
        return getVtblIndex() == AptVFT_Mouse && getIsDefined();
    }

    bool isXmlNode() const
    {
        return getVtblIndex() == AptVFT_XmlNode;
    }

    bool isXml() const
    {
        return getVtblIndex() == AptVFT_Xml;
    }

    bool isXmlAttributes() const
    {
        return getVtblIndex() == AptVFT_XmlAttributes;
    }

    bool isLoadVars() const
    {
        return getVtblIndex() == AptVFT_LoadVars;
    }

    bool isPrototype() const
    {
        return getVtblIndex() == AptVFT_Prototype && getIsDefined();
    }

    bool isObject() const
    {
        return getVtblIndex() == AptVFT_Object && getIsDefined();
    }

    bool isDate() const
    {
        return getVtblIndex() == AptVFT_Date && getIsDefined();
    }

    bool isMovieClip() const
    {
        return getVtblIndex() == AptVFT_MovieClip && getIsDefined();
    }

    bool isStage() const
    {
        return getVtblIndex() == AptVFT_Stage && getIsDefined();
    }

    // isTextFormat @0x82AD4D60 -- a defined TextFormat value (meValueType 0x1C/28
    // AND mbIsDefined). Leak (AptValue.h:409): getVtblIndex()==AptVFT_TextFormat
    // && !isUndefined(); the X360 reads (field & 0x7F)==0x1C then (field>>27)&1.
    bool isTextFormat() const
    {
        return getVtblIndex() == AptVFT_TextFormat && getIsDefined();
    }

    // isMCInParentChain @0x82AD8458 -- walk this value's MovieClip parent chain
    // (vtbl GetParent slot) and report whether the active "current target" /
    // "highlighted" MovieClip (X360 dword_8324D818 / dword_8324D830) appears in it.
    // Leak (AptValue.h:490) declares it out-of-line returning int; body in
    // AptValue.cpp. Xrefed by findChild + AptActionInterpreter::_FunctionAptActionCallMethod.
    int isMCInParentChain() const;

    bool isNone() const
    {
        return getVtblIndex() == AptVFT_None;
    }

    bool isScriptFunction() const
    {
        return (getVtblIndex() == AptVFT_ScriptFunction1
             || getVtblIndex() == AptVFT_ScriptFunction2
             || getVtblIndex() == AptVFT_ScriptFunctionByteCodeBlock)
            && getIsDefined();
    }

    // CanCreateScriptObject @0x82AD84E0 -- can this value type be used as the base
    // for a script Object? meValueType in {StringValue,NativeFunction,Array,ScriptColour,
    // Object,Date,MovieClip,Xml,TextFormat,Error,StringObject,ScriptFunction1,
    // ScriptFunction2}. (X360 accepts numeric tags {1,9,14,18,19,21,22,25,28,32,33,34,35};
    // no mbIsDefined check.) Xrefed by AptActionInterpreter::_createObject.
    bool CanCreateScriptObject() const
    {
        switch (getVtblIndex())
        {
        case AptVFT_StringValue:          // 1
        case AptVFT_NativeFunction:       // 9
        case AptVFT_Array:                // 14
        case AptVFT_ScriptColour:         // 18
        case AptVFT_Object:               // 19
        case AptVFT_Date:                 // 21
        case AptVFT_MovieClip:            // 22
        case AptVFT_Xml:                  // 25
        case AptVFT_TextFormat:           // 28
        case AptVFT_Error:                // 32
        case AptVFT_StringObject:         // 33
        case AptVFT_ScriptFunction1:      // 34
        case AptVFT_ScriptFunction2:      // 35
            return true;
        default:
            return false;
        }
    }

    // ---- value conversions (leak AptValue.h:158-162) ---------------------
    // Convert this value to an ActionScript primitive, dispatching on the value
    // type (meValueType). Bodies in AptValue/AptValueConvert.cpp, decompiled from
    // the PS3 EXTERNAL ELF (toBool @0x7EF024, toInteger @0x7F2CF4, toFloat
    // @0x7E8FE4).
    int   toInteger() const;
    float toFloat() const;
    bool  toBool() const;

    // Render this value into pScratch as an ActionScript string (the meValueType-
    // dispatched renderer). Body in AptValue/AptValueConvert.cpp (@0x82AF8FA0).
    void toString(EAStringC* pScratch) const;

    // urlEncodeCustomRender @0x82AF9410 -- render this value to its URL-encoded
    // string form (returned by value; the X360 sret form `urlEncodeCustomRender(
    // this, &out)`). Used by AptError::sMethod_toString. Body in
    // AptValue/AptValueConvert.cpp.
    EAStringC urlEncodeCustomRender() const;

    // Append this value's string form to pOut (the toString sibling the ToString
    // opcode uses to render into a fresh AptString). Body in
    // AptValue/AptValueConvert.cpp (@0x82AF9668).
    void Append_ToString(EAStringC* pOut) const;

    // Get_ToString @0x82AD8558 -- coerce a value to an EAStringC name: a
    // string-typed value (meValueType 1 / the boxed tag 33) returns its own
    // embedded EAStringC; any other value is rendered into pScratch via toString.
    // Body in AptActionInterpreterVarOps.cpp (homed with its first callers).
    static const EAStringC* Get_ToString(AptValue* pValue, EAStringC* pScratch);

    // ---- typed casts (leak AptValue.h:243-269) ---------------------------
    // c_<type>() reinterprets this value as the concrete type for the matching
    // meValueType. The numeric/bool casts are address-identity (single inheritance
    // through AptValueNoGC), so they are the leak's trivial reinterpret. c_string()
    // is the one with real dispatch (boxed-string indirection) -- its body is in
    // AptValueConvert.cpp (@0x7E4E18).
    AptInteger* c_integer() const { return reinterpret_cast<AptInteger*>(const_cast<AptValue*>(this)); }
    AptFloat*   c_float()   const { return reinterpret_cast<AptFloat*>(const_cast<AptValue*>(this)); }
    AptBoolean* c_boolean() const { return reinterpret_cast<AptBoolean*>(const_cast<AptValue*>(this)); }
    AptString*  c_string()  const;

    // findChild @0x82B01298 -- resolve a child/property/special-name (_root/
    // _parent/_global/_level<N>/this/...) of this value by name. Body (with the
    // AptResolveSpecialName special-name dispatch) in AptValue/AptValueFindChild.cpp.
    AptValue* findChild(const EAStringC* pName, AptValue* pTarget);

    // ---- GC mark callback ------------------------------------------------
    // The Apt garbage collector installs a reference-registration callback the GC
    // value types invoke from RegisterReferences (once per held AptValue ref:
    // cb(owner, &slot, debugName, 0)). The callback body is AptGC::
    // sReferenceRegistrationCb (AptGC.cpp @0x82AD9C80). FLAG (parked): its
    // installer -- the partial-GC mark/sweep tier (XB1 sub_140832E70) -- is
    // un-homed, so the slot stays null and the mark walk is inert until it lands.
    typedef void* (*ReferenceRegistrationCb)(const AptValue* pOwner, void* pSlot,
                                             const char* pDebugName, int);
    static ReferenceRegistrationCb sReferenceRegistrationCb;

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

    // Layout pinned against the x64 XB1 accessors (never called; member body gives
    // complete-class offsetof context). The LSB-first bitfield order above matches
    // the x64 truth bit-for-bit (getVtblIndex @0x1400C1510 `sar eax,19h`,
    // getIsDefined @0x1400C1520 `shr 4`, getRefCount @0x14084E5C0 `shr 6, and 0xFFF`).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptValue, mnValueData) == 0x08, "x64: flags dword at [this+8] (getVtblIndex @0x1400C1510)");
        static_assert(sizeof(AptValue) == 0x10, "x64: memberless AptExtern allocates 0x10 (AptValueInitialize @0x140830BA0); all leaf payloads at +0x10");
    }

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
