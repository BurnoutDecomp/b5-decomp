// ===========================================================================
// AptGlobals.cpp -- storage definitions for the EATech Apt global/data symbols.
//
// The Apt source references ~137 engine globals as bare `extern` across its TUs
// but never DEFINES storage for them (they are the X360 .data/.bss objects the
// Apt init routines populate at runtime). This TU is their single definition
// home: each symbol is defined with its FAITHFUL initial value so the game LINKS.
//
// FAITHFULNESS POLICY (per symbol group, see the section comments):
//   - Pointers / pool pointers / singletons / the 27 gpAptNativeFn_* entries:
//     null storage -- the Apt init routines (AptInit / the AS-builtin registry /
//     the allocator boot) fill them. Null is the faithful pre-init state.
//   - Function-pointer customization hooks: null (the hooks default OFF).
//   - bool / byte flags: false/0.  thread-id / mutex / TLS scalars: 0 / null.
//   - Identity matrix / CXForms: the REAL identity values (mul=1 / add=0).
//   - Interned AS-name string constants (EAStringC): constructed from their
//     literal AS identifier. EAStringC's ctor (InitFromBuffer/AllocateBuffer)
//     has a plain-malloc fallback when gpNonGCPoolManager is null, and the empty
//     path uses the constant-init s_EmptyInternalData, so STATIC construction
//     here is order-safe and faithful (no deferred builder needed).
//   - Name/lookup rodata tables (AS-name / property / event / descriptor /
//     analog tables): correctly-typed ZEROED storage with a // FLAG naming the
//     X360 vaddr -- the table CONTENTS are engine-generated EAStringC objects /
//     un-recovered rodata; zero storage still resolves the link and the engine
//     constant-string registration fills the EAStringC arrays at init.
//
// EXACT-TYPE-MATCH RULE: every definition's type spelling (struct/class tag,
// const, pointer levels, fn-ptr signature) matches the in-source `extern` so the
// MSVC mangled name matches (the linker is tag- and const-sensitive). Where a
// type is only stored by pointer it is forward-declared (avoids dragging the big
// header set + avoids the gpAptGlobalFallback header/cpp type divergence -- see
// that symbol). Layout-bearing value types include their real headers.
// ===========================================================================

#include "types.hpp"

// --- value-typed globals need the real layout -----------------------------
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"           // AptMatrix (identity)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"           // AptCXForm (null/identity)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"         // EAStringC (interned strings)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"       // AptActionInterpreter (VM singletons)
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"// AptGCReleaseVector (gValuesToRelease)
#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"      // AptRenderManagerQueue (gAptRenderManagerQueue)
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"              // AptValueGC_PoolManager + byte_82144A18

// --- pointer-only targets: forward declarations (exact struct/class tags) ---
class  AptValue;
class  AptString;
struct AptTarget;
struct AptCIH;
class  AptValueVector;

// ===========================================================================
// 1. AptValue* singletons + sentinels.  Owned by AptInit / the AS-builtin
//    registration boot TU (X360 .data off_8324D8xx / off_8324E2xx / E4xx).
//    Null until those inits run; the bodies short-circuit on null.
// ===========================================================================
AptValue* gpUndefinedValue            = nullptr;   // off_8324D814 (the shared AS `undefined`)
AptCIH*   gpAptEmptyCIH               = nullptr;   // dword_8324D700 (the pinned "EmptyCIH" AptCIHNone placeholder)
AptValue* gpAptNoneValue              = nullptr;   // off_8324D814 (the AS `null`/none)
AptValue* gpUndefinedCIH              = nullptr;   // the undefined-CIH sentinel (AptInit)
AptValue* gpAptDestroyedClipValue     = nullptr;   // dword_8324D818
AptValue* gpAptCurrentTargetMC        = nullptr;   // dword_8324D818
AptValue* gpAptRootTargetMC           = nullptr;   // dword_8324D830
AptValue* gpAptFunctionPrototypeRoot  = nullptr;   // dword_8324E4EC
// (the AptValue*-typed gpAptGlobalFallback duplicate was RETIRED 2026-07-05: the
// LIVE off_8324E380 symbol is AptGlobal.cpp's AptValueWithHash* -- the dead
// AptValue* twin here made findChild's _global arm look up a never-assigned null
// while the stores landed in the live object.)
AptValue* gpAptKeyObject              = nullptr;   // off_8324E2A8 (Key)
AptValue* gpAptStringObject           = nullptr;   // off_8324D82C (String)
AptValue* gpAptExternObject           = nullptr;   // off_8324E2CC (extern)
AptValue* gpAptGlobalExtensionObject  = nullptr;   // off_8324E37C (the _global extension object)
void*     gpAptRenderingContext       = nullptr;   // dword_8324E2AC (the shared AptRenderingContext; opaque view)
// (gpGlobalGlobalObject / gpGlobalExtensionObject RETIRED 2026-07-05: they duplicated
// off_8324E380 / off_8324E37C under second never-assigned names, so findChild's global
// arms silently looked up empty objects. Every reader now binds gpAptGlobalFallback /
// gpAptGlobalExtensionObject above -- the pair AptValueInitialize actually builds.)
AptValue* gpObjRegistrationFunc       = nullptr;   // the Object-registration native fn
// The AS global-function singletons AptCIH::objectMemberLookup returns for the
// setInterval/clearInterval/isNaN/unescape/escape/Boolean member names. Created
// by the deferred sub_82AF6B68 builtin-table init (see AptInit.cpp); null until
// that init homes -- the lookup's null return continues the findChild resolution.
AptValue* gpAptFnSetInterval          = nullptr;   // off_8324D828
AptValue* gpAptFnClearInterval        = nullptr;   // off_8324D81C
AptValue* gpAptFnIsNaN                = nullptr;   // off_8324D824
AptValue* gpAptFnUnescape             = nullptr;   // off_8324E1FC
AptValue* gpAptFnEscape               = nullptr;   // off_8324D80C
AptValue* gpAptFnBoolean              = nullptr;   // off_8324D74C

// ===========================================================================
// 2. The 27 gpAptNativeFn_8324E4xx per-built-in native-function singletons.
//    Owned by the AS-builtin registration boot TU (X360 off_8324E42C..0x8324E494);
//    each is an AptNativeFunction torn down via the AptValue Release virtual.
//    Null storage -- the registry installs them.
// ===========================================================================
AptValue* gpAptNativeFn_8324E42C = nullptr;
AptValue* gpAptNativeFn_8324E430 = nullptr;
AptValue* gpAptNativeFn_8324E434 = nullptr;
AptValue* gpAptNativeFn_8324E438 = nullptr;
AptValue* gpAptNativeFn_8324E43C = nullptr;
AptValue* gpAptNativeFn_8324E440 = nullptr;
AptValue* gpAptNativeFn_8324E444 = nullptr;
AptValue* gpAptNativeFn_8324E448 = nullptr;
AptValue* gpAptNativeFn_8324E44C = nullptr;
AptValue* gpAptNativeFn_8324E450 = nullptr;
AptValue* gpAptNativeFn_8324E454 = nullptr;
AptValue* gpAptNativeFn_8324E458 = nullptr;
AptValue* gpAptNativeFn_8324E45C = nullptr;
AptValue* gpAptNativeFn_8324E460 = nullptr;
AptValue* gpAptNativeFn_8324E464 = nullptr;
AptValue* gpAptNativeFn_8324E468 = nullptr;
AptValue* gpAptNativeFn_8324E46C = nullptr;
AptValue* gpAptNativeFn_8324E470 = nullptr;
AptValue* gpAptNativeFn_8324E474 = nullptr;
AptValue* gpAptNativeFn_8324E478 = nullptr;
AptValue* gpAptNativeFn_8324E47C = nullptr;
AptValue* gpAptNativeFn_8324E480 = nullptr;
AptValue* gpAptNativeFn_8324E484 = nullptr;
AptValue* gpAptNativeFn_8324E488 = nullptr;
AptValue* gpAptNativeFn_8324E48C = nullptr;
AptValue* gpAptNativeFn_8324E490 = nullptr;
AptValue* gpAptNativeFn_8324E494 = nullptr;

// ===========================================================================
// 3. Operand-stack / register / native-arg state + the deferred-release vector
//    pointer.  VM state, owned by AptInit / the interpreter boot.
// ===========================================================================
// (gpAptRegisterBase retired 2026-07-01: off_8324E3D0 is
//  AptScriptFunctionBase::spRegBlockCurrentFrameBase -- the one canonical global.)
AptValue**      gppAptNativeArgStack        = nullptr;   // off_8324E768
AptString*      gpStringPoolFreeList        = nullptr;   // off_8324E4FC (string-pool free list head)
// The ONE deferred-release vector pointer (console symbol gValuesToRelease @
// off_8324E51C; pool-allocated + family-(B)-constructed by AptCommonInitialize).
// UNIFIED 2026-08-07 -- replaces the former trio {gpAptDeferredReleaseVector,
// AptInit's file-local gpAptDeferredVecCommon, the static AptGCReleaseVector
// gValuesToRelease instance} that modelled this one console slot.
AptValueVector* gpValuesToRelease           = nullptr;   // off_8324E51C

// ===========================================================================
// 4. The interpreter VM singletons (value objects) + the GC live-value pool.
//    X360 &dword_8324E760 (the VM) and off_8324D834 (the GC pool).
//    The VM has only an implicit default ctor + a dtor: namespace-scope storage
//    is zero-initialized, faithful to the console .data object the engine inits.
// ===========================================================================
AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// FLAG: gAptValueGCPool (off_8324D834) is the live-AptValue pool manager. It has
// NO default ctor (only the 2-arg DOGMA-forwarding ctor), so it is constructed
// here with (0,0) -- an empty pool the engine RE-SIZES at AptInit via the real
// per-VFT object-size table (StaticInitialize). The base DOGMA ctor reaches the
// platform DOGMA heap (DOGMA_Malloc, an external TU); static-init order vs that
// heap is the existing Apt-allocator-boot concern, not a code defect here.
AptValueGC_PoolManager gAptValueGCPool(0u, 0u);   // off_8324D834

// gpAptValueGCPool (off_8324D834) -- the type-erased void* view the engine stamps
// to the same GC pool. Distinct C++ symbol; null until AptInit points it.
void* gpAptValueGCPool = nullptr;   // off_8324D834

// ===========================================================================
// 5. The Apt allocator pool pointers (all alias the shared fixed-size DOGMA pool
//    off_8324D808 at runtime).  Owned by the Apt allocator boot TU; null pre-init.
// ===========================================================================
DOGMA_PoolManager* gpAptOperandStackPool  = nullptr;   // off_8324D808 (operand-stack arrays)
DOGMA_PoolManager* gpAptPseudoDataPool    = nullptr;   // off_8324D808 (pseudo-data nodes)
DOGMA_PoolManager* gpAptRenderManagerPool = nullptr;   // off_8324D808 (render-manager nodes)
DOGMA_PoolManager* gpAptSharedPtrPool     = nullptr;   // off_8324D808 (shared-ptr control blocks)
DOGMA_PoolManager* gpAptSingleListPool    = nullptr;   // off_8324D808 (single-list nodes)

// ===========================================================================
// 6. Value-typed engine objects (the render manager draw queue).
//    (The deferred-release vector is the section-3 gpValuesToRelease POINTER --
//    the static instance here was one of the three retired off_8324E51C homes.)
// ===========================================================================
AptRenderManagerQueue gAptRenderManagerQueue = { nullptr, nullptr };// dword_8324E7D8

// ===========================================================================
// 7. Identity matrix / CXForms -- const data with KNOWN-correct values.
//    AptMatrix identity = [a=1 b=0 c=0 d=1 tx=0 ty=0].
//    AptCXForm "null"/identity = per-channel scale 1.0 / translate 0.0 (the SWF
//    CXFORM identity: out = in*scale + translate).  AptColorHelper holds a vptr
//    (+0) then four ARGB floats; the bracketed init fills {vptr-placeholder, ARGB}
//    -- but the vptr is set by the AptColorHelper ctor, so these are constructed
//    (not aggregate-init) to get the correct vtable + channel values.
// ===========================================================================
// NOTE: namespace-scope `const` objects have INTERNAL linkage in C++ by default;
// every const definition below is marked `extern` so it has EXTERNAL linkage and
// actually resolves the other TUs' `extern const ...` references.
AptMatrix              gIdentityMatrix    = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
extern const AptMatrix gAptIdentityMatrix = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };   // flt_8324E2B0

// CXForm identity builder: scale channels = 255.0, translate channels = 0.0.
// The CXForm channel space is FIXED-POINT 0..255 (the SWF cxform): the compose is
// current = append * current / 255 (AptRenderingContext::appendColourTransform), and
// the context's own identity init is scale = 255 ("opaque white", flt_82010C20). An
// identity of 1.0 here made every appendColourTransform level DIVIDE the colour scale
// by 255 (1.0*current/255), collapsing nested sprites to ~zero -> the whole apt scene
// rendered invisible. 255 is the multiplicative identity of this space.
static AptCXForm AptMakeIdentityCXForm()
{
    AptCXForm cx;   // ctor sets vptrs + zeroes all eight channels
    for (int i = 0; i < 4; ++i)
        cx.scale.SetValuef(static_cast<AptColorHelper::AptColorValue>(i), 255.0f);
    // translate stays 0.0 from the ctor (the additive identity)
    return cx;
}
AptCXForm              gIdentityCXForm = AptMakeIdentityCXForm();
extern const AptCXForm gAptNullCXForm  = AptMakeIdentityCXForm();   // off_82F73388

// ===========================================================================
// 8. Interned AS-name string constants (EAStringC), constructed from the literal
//    AS identifier each name encodes.  Faithful: the X360 builds these into the
//    engine constant-string table; here the EAStringC ctor builds the same value.
//    ("TypeName_X" -> the lowercase AS typeof() string; "ClassName"/"ClassKey" ->
//     the AS class name; the key names -> the AS identifier.)
// ===========================================================================
extern const EAStringC gAptKeyThis           ("this");       // unk_8324E6C0
extern const EAStringC gAptThisKey           ("this");       // dword_8324E6C0 (alias name for the same key)
extern const EAStringC gAptKeyGlobal         ("_global");    // unk_8324E59C
// ⚠ CORRECTED 2026-08-05 (the licence-card dead-super root cause): unk_8324E6B8
// is the AS "super" key, NOT "arguments". Three console consumers prove it:
//   * AptScriptFunction2::SetupBeforeExecution's flag-0x10 branch (the SWF
//     PreloadSuper bit -- flags algebra: super-calling methods carry 0x119
//     [preload], non-super ones 0x129 [suppress 0x20]) resolves this key via
//     findChild when no pre-resolved super arrives -- and findChild's
//     ObjectIndex wordlist has "super" -> special-name id 18 (the super arm).
//   * runStream resolves this key ONCE per stream into the context slot
//     (LocalContextT::mpScopeVariable, console ctx+0x10) that CallMethod's
//     supercall identity test reads.
//   * CallMethod seeds its default method-name slot with this same object.
// The old "arguments" guess sent the resolve down the generic hash path (no
// such child), so every AS `super.X()` silently no-opped -- the learner-permit
// card never left its 'invisible' frame.
extern const EAStringC gAptKeySuper          ("super");      // unk_8324E6B8
extern const EAStringC gAptKeyPrototype      ("prototype"); // dword_8324E698 -- the AS "prototype" key
                                                            // (hash 1689; AptNativeHash's fast-slot gates
                                                            // key it against mpPrototype. The old literal
                                                            // "__proto__" (hash 27581) made every
                                                            // fn.prototype Lookup return null -> no class
                                                            // methods ever landed. "__proto__" itself is
                                                            // StringPool::saConstant[0].)
extern const EAStringC gAptKeyControllerKey  ("controller"); // unk_8324E614
extern const EAStringC gAptEmptyMethodName   ("super");      // unk_8324E6B8 -- the SAME console object as
                                                             // gAptKeySuper ("super"); CallMethod seeds its
                                                             // default name slot with it and NameEquals-tests
                                                             // against it ("is the slot still the default").
                                                             // Kept as a distinct PC symbol, but the CONTENT
                                                             // must match gAptKeySuper (2026-08-05 fix).
extern const EAStringC gAptObjectClassName   ("Object");     // &dword_8324E650
extern const EAStringC gAptStringClassName   ("String");     // &dword_8324E6B4
extern const EAStringC gAptSpriteClassKey    ("MovieClip");  // dword_8324E640 (the Sprite/MovieClip class key)
extern const EAStringC gAptTypeName_Undefined("undefined");  // dword_8324E6C8
extern const EAStringC gAptTypeName_Number   ("number");     // dword_8324E64C
extern const EAStringC gAptTypeName_Boolean  ("boolean");    // dword_8324E608
extern const EAStringC gAptTypeName_String   ("string");     // dword_8324E6B4
extern const EAStringC gAptTypeName_Object   ("object");     // dword_8324E650
extern const EAStringC gAptTypeName_MovieClip("movieclip");  // dword_8324E640
extern const EAStringC gAptTypeName_Null     ("null");       // dword_8324E648
extern const EAStringC gAptTypeName_Function ("function");   // dword_8324E624

// gpAptThisKey -- the pointer the AptActionRun path reads. The reference
// (AptActionRun.cpp:39 `extern const EAStringC* gpAptThisKey`) is a NON-const
// pointer-to-const (mangles ?...@@3PEBVEAStringC@@EB); a top-level-const pointer
// would mangle Q (?...@@3QEB...) and not resolve. Points at interned gAptKeyThis.
const EAStringC* gpAptThisKey = &gAptKeyThis;

// ===========================================================================
// 9. Name / lookup rodata TABLES.  FLAG: zeroed correctly-typed storage with the
//    X360 vaddr noted.  The AS-name / property-name EAStringC arrays are filled
//    by the engine constant-string registration at init (dword_8324E580); the
//    descriptor / remap / event-bit int tables are un-recovered rodata.  Zero
//    storage resolves the link; CONTENTS are a data-segment follow-on.
//    Sizes are conservative upper bounds (indices are bounded by the descriptor
//    walk + the property-id space) so the indexed reads stay in-bounds.
// ===========================================================================

// The runtime AS-name / property-name EAStringC table (X360 dword_8324E580). Both
// names below alias the SAME console table; defined once each as zeroed EAStringC
// storage the engine populates.  gAptEventNameTable is the non-const pointer view.
// FLAG: contents engine-generated (dword_8324E580); X360 vaddr 0x8324E580.
extern const EAStringC gAptASNameTable[256]       = {};   // dword_8324E580 (AS-name table)
extern const EAStringC gAptPropertyNameTable[256] = {};   // dword_8324E580 (property-name table, same console addr)
EAStringC*             gAptEventNameTable         = nullptr;   // event-name table pointer (engine-pointed)

// gAptPropertyIndexRemap: property-id -> property-name-table index (X360 dword_82F73010).
// FLAG: rodata contents un-recovered; X360 vaddr 0x82F73010. Zeroed (-> index 0).
extern const int gAptPropertyIndexRemap[256] = {};   // dword_82F73010

// gAptMemberIndexToEventBit: AS member-index (-200 biased) -> event bitmask
// (X360 dword_82143BA8; 19 entries EXTRACTED from the ARTIST .asm rodata,
// big-endian .long/.byte run 0x82143BA8..0x82143BF3). Each maps a
// SpriteMembersIndex on<Event> id (200..218) to its clip-event mask bit --
// consistent with KaAptEventDescriptors (onLoad 207 -> 0x1, onEnterFrame 203 ->
// 0x2, onKeyDown 204 -> 0x40, onKeyUp 205 -> 0x80, onUnload 217 -> 0x4, onData
// 200 -> 0x100). -1 = not a maskable handler. Tail stays zero (out-of-range ids
// never reach the read -- the wordlist tops out at 218).
extern const int32_t gAptMemberIndexToEventBit[256] = {   // dword_82143BA8
    /* 200 onData           */ 0x00000100,
    /* 201 onDragOut        */ 0x00010000,
    /* 202 onDragOver       */ 0x00008000,
    /* 203 onEnterFrame     */ 0x00000002,
    /* 204 onKeyDown        */ 0x00000040,
    /* 205 onKeyUp          */ 0x00000080,
    /* 206 onKillFocus      */ -1,
    /* 207 onLoad           */ 0x00000001,
    /* 208 onMouseDown      */ 0x00000010,
    /* 209 onMouseMove      */ 0x00000008,
    /* 210 onMouseUp        */ 0x00000020,
    /* 211 onPress          */ 0x00000400,
    /* 212 onRelease        */ 0x00000800,
    /* 213 onReleaseOutside */ 0x00001000,
    /* 214 onRollOut        */ 0x00004000,
    /* 215 onRollOver       */ 0x00002000,
    /* 216 onSetFocus       */ -1,
    /* 217 onUnload         */ 0x00000004,
    /* 218 onMouseWheel     */ 0x00080000,
};

// AptListenerEventDescriptor: the {eventMask, nameIndex} pair, matching the local
// struct in AptAnimationTarget.cpp:227 verbatim (same tag + members so the array
// element type -- and thus the gAptListenerEventDescriptors mangled name -- match).
struct AptListenerEventDescriptor
{
    int nEventMask;   // +0x00
    int nNameIndex;   // +0x04
};

// gAptListenerEventDescriptors: {eventMask, nameIndex} pairs (X360 unk_82F7334C..).
// FLAG: rodata contents un-recovered; X360 vaddr 0x82F7334C. Count is the matching
// descriptor-table length (X360 (82F73380 - 82F73350)/8 + 1 == 7). Zeroed pairs +
// a zero count make the descriptor walk a no-op until the table is extracted.
extern const AptListenerEventDescriptor gAptListenerEventDescriptors[7] = {};   // unk_82F7334C
extern const int gAptListenerEventDescriptorCount = 0;                          // FLAG: real count == 7 (extract with the table)

// ===========================================================================
// 10. The per-VFT object-size table the GC pool's StaticInitialize scans
//     (X360 byte_82144A18[AptVFT_NumVFTs]).  Zeroed STORAGE only: the contents
//     are FILLED by AptValueGC_PoolManager::StaticInitialize from sizeof() of the
//     reconstructed x64 classes (the console .rdata bytes are RECOVERED -- the
//     0x82144A18 dump -- and recorded per entry there as the 32-bit cross-check).
// ===========================================================================
uint8_t byte_82144A18[AptVFT_NumVFTs] = {};   // 0x82144A18 (filled at StaticInitialize)

// ===========================================================================
// 11. Analog-input tables (X360 .data / rodata; the input layer fills them each
//     frame).  FLAG: per-pad stride 16 bytes (4 floats / 16 bytes per pad).
//     Zeroed storage; the input subsystem writes the live samples.  Sized for 8
//     pads (8 * 4 floats) so the [4*nPad] / [16*player] reads stay in-bounds.
// ===========================================================================
extern const float gAptKeyAnalogStickL[8 * 4] = {};   // unk_8324D750 (stick id 501)
extern const float gAptKeyAnalogStickR[8 * 4] = {};   // unk_8324E2D8 (stick id 502)
extern const float gAptKeyAnalogTrigger[8 * 4] = {};  // flt_8324E200

float gAptAnalogAxis0[8 * 4] = {};   // flt_8324E200 (player stride 16 bytes)
float gAptAnalogAxis1[8 * 4] = {};   // flt_8324E204 (player stride 16 bytes)
u8    gAptAStickLeft[8 * 16]  = {};  // unk_8324D750 (player stride 16 bytes)
u8    gAptAStickRight[8 * 16] = {};  // unk_8324E2D8 (player stride 16 bytes)

// ===========================================================================
// 12. Function-pointer customization hooks.  Null by default (the hooks are OFF
//     until a host installs them).  Owned by the Apt host-integration layer.
// ===========================================================================
typedef void (*AptVarNotFoundCb)(const char* pName);   // matches AptActionInterpreterVariable.cpp:57
enum AptMaskRenderOperation : int;                     // matches the Apt render headers

void (*gpAptFSCommandHook)(const char* pCommand, const char* pArgs)      = nullptr;
// (gpAptBackToScriptHook RETIRED 2026-07-12: dword_8324E828 is NOT a standalone
//  hook -- it is &gAptFuncs (dword_8324E818) + 0x10 == pfnSetBackgroundColour.
//  The tag-5 arm in AptMovie::doFrameControls now calls the table slot directly.)
void (*gpAptGCTableFree)(void* p, unsigned nBytes)                       = nullptr;   // dword_8324E820
AptVarNotFoundCb gpAptVarNotFoundCb                                      = nullptr;
// (gpAptInputRecorderSink / gpAptInputRecorderTagSink RETIRED 2026-08-07: like
//  dword_8324E828 above, dword_8324E830/834 are NOT standalone hooks -- they are
//  &gAptFuncs (dword_8324E818) + 0x18/+0x1C == pfnDebugAddSavedInput /
//  pfnDebugSetScreenGrabPending. AptUpdate + AptAnimationTarget call the table
//  slots directly.)
void (*gpfnAptDestroyCustomControl)(intptr_t nZId)                       = nullptr;   // dword_8324E898
void (*gpfnAptCustomControlPushRenderData)(const char* pInstanceName)    = nullptr;   // dword_8324E8CC
void (*gpfnAptCustomControlPopRenderData)(const char* pInstanceName)     = nullptr;   // dword_8324E8D0
void (*gpfnAptDrawTextRenderData)(intptr_t nZId, AptMaskRenderOperation eOp, int nTick) = nullptr;  // dword_8324E868
void (*gpfnAptReleaseTextRenderData)(intptr_t nZId, int nOp)             = nullptr;   // dword_8324E864

// ===========================================================================
// 13. bool / byte flags.  false / 0 (the features default OFF).
// ===========================================================================
bool gAptDefaultTextMouseWheelEnabled = false;   // byte_8324E392
bool gbAptCustomControlRenderEnabled  = false;   // byte_82F733F6

// The BackgroundColour once-per-load latch (the SDK's gbBackgroundColorSet):
// set by the doFrameControls tag-5 arm, reset by AptLoadAnimation @0x82B07AF4.
unsigned char gbAptBackgroundColourSet = 0;   // byte_8324D807
unsigned char gbAptInShutdown        = 0;   // byte_8324E7C9
unsigned char gbAptRecorderGate      = 0;   // byte_82F733F7

// ===========================================================================
// 14. int / uint scalars.  0 (counters / ids / mode / tick start at zero).
// ===========================================================================
int gAptBoundingRectMode       = 0;
int gAptDeferredReleaseCount    = 0;   // dword_8324E508
intptr_t gAptEmptyTextRenderDataZID  = 0;   // pointer-width (matches the widened mZID)
int gAptInputRecorderEnabled    = 0;   // dword_8324E518
int gAptLookupPoolSize          = 0;   // dword_82F733EC
int gAptVMThreadId              = 0;   // dword_8324E500
int gbAptSavedInputActive       = 0;   // dword_8324D7F0 (int-typed flag)
int gnAptActionFrameId          = 0;   // dword_8324E514
// The "null input" context id queueClipEvents stamps on an enterFrame enqueue
// (PS3 `gNullInput`, the 4th AddActionFront arg). FLAG: no writer found in the
// dumps (a bss dword); zero until one surfaces.
int gAptNullInputId             = 0;   // console gNullInput
int gnAptNativeArgCount         = 0;   // dword_8324E760
int gnCurrUpdateTick            = 0;   // dword_8324E520 (the update-side tick bank AptUpdate advances)
// The render-side consumed tick AptUpdate banks against (the update side never runs
// more than the banked-frame cap ahead of it). Advanced by the render drive.
int gnCurrRenderTickConsumed    = 0;   // dword_8324E524
// (gAptParseArgHeapCount retired 2026-07-01: dword_8324E3D4 is
//  AptScriptFunctionBase::snRegBlockCurrentFrameCount.)

int32_t gAptMouseX  = 0;   // dword_8324E534
int32_t gAptMouseY  = 0;   // dword_8324E538
int32_t gAptTimerMs = 0;

uint32_t gAptInputRecorderTag    = 0;   // dword_8324D820
uint32_t gAptKeyLastEvent        = 0;   // dword_8324E7A8
uint32_t gnAptGCThreadId_Ctor    = 0;   // dword_8324E500
uint32_t gnAptGCThreadId_Release = 0;   // dword_8324E504

// (gAptParseArgHeapPtr + its /alternatename ODR bridge retired 2026-07-01: the "parse-arg
//  scratch heap" bump pointer off_8324E3D0 IS AptScriptFunctionBase::spRegBlockCurrentFrameBase
//  -- the register-block window base. Consumers now use PushStaticData/PopStaticData.)

// ===========================================================================
// 15. Pointer / thread-id / mutex scalars (void* and typed).  Null / 0.
//     gAptRenderThreadId is EA::Thread::ThreadId == typedef void* (same symbol).
// ===========================================================================
AptCIH**  gAptMaskAncestorScratch = nullptr;   // mask-ancestor scratch array
AptTarget* gpCurrentAptTarget     = nullptr;   // off_8324E574

// (gAptActionInterpreterVM retired 2026-07-01: dword_8324E760 IS the gAptActionInterpreter
//  object above -- the null void* "view" was a placeholder from before the VM was live.)
void* gAptDeferredReleaseQueue = nullptr;   // off_8324E2C8
void* gAptRenderThreadId       = nullptr;   // dword_8324E504 (EA::Thread::ThreadId == void*)
void* gAptUnresolveMutex       = nullptr;   // unk_8324E728
void* gAptUnresolveMutexName   = nullptr;   // unk_82143270

// gpAptTarget (off_8324E574) is ALREADY defined as `AptTarget* gpAptTarget` in
// AptTarget.cpp:49 -- so we do NOT redefine it.  But AptMovie.cpp:72 references it
// under a divergent `void* gpAptTarget` spelling (the same ODR quirk as
// gAptParseArgHeapPtr), leaving `?gpAptTarget@@3PEAXEA` unresolved.  Alias that
// void* symbol onto the existing AptTarget* definition (same storage / console
// address); no new object is defined here.
#pragma comment(linker, "/alternatename:?gpAptTarget@@3PEAXEA=?gpAptTarget@@3PEAUAptTarget@@EA")
