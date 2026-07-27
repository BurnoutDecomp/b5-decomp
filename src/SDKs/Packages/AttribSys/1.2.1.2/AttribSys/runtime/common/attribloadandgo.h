#pragma once

// AttribSys runtime -- Attrib::Vault (the "load and go" vault object) and the
// Attrib::IGarbageCollector callback interface.
//
// Reconstructed from the DecFIGS DWARF (SDKs/.../attribloadandgo.h) gated on the
// X360 ARTIST ledger. A Vault is the AttribSys library's in-memory image of one
// serialised .vlt blob: it is constructed over the blob (Vault ctor), has its
// asset dependencies resolved (ResolveDependency) and is then Initialize()d into
// the live attribute Database. The garbage collector callback lets the host free
// per-asset transient data when the GC reclaims a vault.
//
// Only the slice CgsAttribSys::AttribSysModule needs is bodied here:
//   Vault ctor      @ 0x8280A2E8  (X360)
//   ResolveDependency @ 0x82803338
//   Initialize      @ 0x8280A660
//   HasUnresolvedDependency : inline (mResolvedCount < mNumDependencies), matches
//                             the X360 RegisterSchema assert `*(v+44) < *(v+40)`.
// The remaining DWARF-declared query/export API is declared so callers compile;
// their bodies live in the AttribSys SDK (attribloadandgo.cpp), their own TU.

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

namespace Attrib
{
    class ExportManager;   // owned by attribexportmanager.h (declared opaquely here)
    class Vault;

    // Namespace-scope key aliases the export subsystem passes by const-reference
    // (DecFIGS attribloadandgo.h:24/25).
    //   TypeID   -- names an attribute TYPE (the gDatabaseType / gClassType /
    //               gCollectionType granularity keys, and every ExportPolicy method's
    //               `const TypeID&` parameter).
    //   ExportID -- names one exported attribute instance within a Vault.
    // Both are FULL 64-bit string hashes on the X360 spine (NOT the truncated
    // 32-bit ::Attribute::Key): the ExportManager policy table has a 16-byte
    // {u64 type, ptr policy} stride (GetExportPolicies @0x8280DC70 Allocs
    // 16*mReserve and AddExportPolicy receives the type in a 64-bit pair),
    // the serialised ExpN export entries carry u64 export/type ids, and the
    // three granularity keys are qword globals (qword_83011CE0/CD8/CE8) whose
    // values equal hash64('Attrib::DatabaseLoadData'/'Attrib::ClassLoadData'/
    // 'Attrib::CollectionLoadData'). See AttributeKey.h StringToKey64.
    typedef u64 TypeID;    // attribloadandgo.h:25
    typedef u64 ExportID;  // attribloadandgo.h:24

    // The host garbage-collector callback. AttribSys calls ReleaseData when it
    // reclaims a vault's asset so the host can free any transient data it attached.
    // Vtable order from DWARF (attribloadandgo.h:507): [0] ~IGarbageCollector,
    // [1] ReleaseData.
    //
    // The destructor is out-of-line (home: attribgarbagecollector.cpp). The X360
    // emits its scalar deleting destructor thunk @ 0x827DBA70 -- MSVC only produces
    // that ??_G thunk (and pins the interface vtable to one TU) when the virtual
    // destructor is NOT inline, so it is declared here and defined in that TU.
    struct IGarbageCollector
    {
        virtual ~IGarbageCollector();

        // luType selects the asset class, lAssetId names the asset (the 64-bit
        // Vault::AssetID -- spelled u64 here because Vault is declared below),
        // lpData/luSize the transient block being released.
        virtual void ReleaseData(u8 luType, u64 lAssetId,
                                 void* lpData, unsigned int luSize) = 0;
    };

    // Attrib::Vault -- one loaded serialised attribute vault. Member layout from the
    // DecFIGS DWARF (attribloadandgo.h); the dependency counters mNumDependencies(+40)
    // / mResolvedCount(+44) back HasUnresolvedDependency, confirmed against the X360
    // RegisterSchema asm.
    class Vault
    {
    public:
        // Hashed asset id (the serialised dependency/export keys). Full 64-bit
        // hash on the X360 spine: the DepN dependency-id array is walked with
        // ld/std doublewords (Vault ctor @0x8280A2E8) and the schema DepN ids
        // equal hash64('schema.vlt')/hash64('schema.bin').
        typedef u64 AssetID;

        // Construct over the export policies + serialised blob (X360 0x8280A2E8).
        // lAssetId is the vault's own id (0 for the schema vault); lpData/luSize the
        // .vlt image; lbType the EAttribSysVaultType; lpGC the host GC callback.
        Vault(ExportManager& lExportMgr, AssetID lAssetId, void* lpData,
              unsigned int luSize, u8 lbType, IGarbageCollector* lpGC);
        ~Vault();

        IGarbageCollector* GarbageCollector() const { return mGC; }

        // The vault's live-object refcount ops (DWARF attribloadandgo.h:145/146;
        // const with a mutable count, exactly the SDK's const-leaking shape).
        // The X360 inlines the raw +16 ops at every site: the export policies /
        // collections AddRef their source vault; ~ClassPrivate + the vault slot
        // Release it and destroy the vault on the final drop (Release returns
        // "hit zero"). The key argument is diagnostic-only.
        void AddRef(::Attribute::Key luKey) const
        {
            (void)luKey;
            ++mRefCount;
        }
        bool Release(::Attribute::Key luKey) const
        {
            (void)luKey;
            --mRefCount;
            return mRefCount == 0;
        }
        bool IsReferenced() const { return mRefCount != 0; }

        // Bind a resolved dependency's data into the vault (X360 0x82803338). The
        // arguments are (dependency index, data, size, asset-flag).
        void ResolveDependency(unsigned int luIndex, void* lpData,
                               unsigned int luSize, u8 lbIsAsset);

        // True while not every declared dependency has been resolved.
        bool HasUnresolvedDependency() const { return mResolvedCount < mNumDependencies; }

        // Commit the vault into the live attribute database (X360 0x8280A660).
        void Initialize();

        // Tear the vault back out of the live database (X360 0x8280E6F0): run each
        // export policy's per-export deinitialize, collect the database garbage, then
        // release the owned asset blocks. Body lives in the AttribSys load-and-go TU
        // (attribloadandgo.cpp); ~Vault calls it on a still-initialized vault.
        void Deinitialize();

        // The payload pointer of exported block luIndex (X360 0x82803420). Asserts
        // the index is within the loaded export count.
        void* GetExportData(unsigned int luIndex) const;

        // Register one initialized export's live object/data into the next free
        // export slot (X360 0x8280A988): stores the export id, resolves the
        // policy index for the type (it becomes the DataBlock kind byte so
        // Deinitialize can dispatch), and binds {lpData, luSize} into the slot.
        // Called by the export policies' Initialize (e.g. DatabaseExportPolicy
        // registers the freshly built DatabasePrivate under 'Attrib::Database').
        void Export(const TypeID& lrType, const ExportID& lrExport,
                    void* lpData, unsigned int luSize);

        // --------------------------------------------------------------------
        // Serialised container views (the .vlt chunk stream the ctor walks
        // @0x8280A2E8). All fields are the LE-ported on-disk shape (32-bit
        // slots; the BE originals are baked in the X360 exe / shipped in the
        // platform-2 bundles). Chunk = {u32 fourCC, u32 size incl. header}.
        // --------------------------------------------------------------------
        struct ChunkBlock
        {
            u32 muFourCC;   // +0x00 'Vers'/'DepN'/'StrN'/'DatN'/'ExpN'/'PtrN'
            u32 muSize;     // +0x04 whole-chunk byte size (advance stride)
        };

        // The DepN chunk (mDependencies points at its header, X360 +28).
        // Payload: pad, count, u64 ids[count], u32 nameOffsets[count], names.
        struct DependencyNode
        {
            ChunkBlock mChunk;              // +0x00
            u32        muPad;               // +0x08
            u32        muNumDependencies;   // +0x0C (ctor lwz +12)
            // AssetID ids[muNumDependencies] @ +0x10 (ctor ld +16 stride 8)
            const AssetID* GetIds() const
            {
                return reinterpret_cast<const AssetID*>(this + 1);
            }
        };

        // One serialised ExpN export entry (24-byte stride, entries @ chunk+16).
        struct ExportEntry
        {
            ExportID muExportId;   // +0x00
            TypeID   muTypeId;     // +0x08 (the policy-table lookup key)
            u32      muSize;       // +0x10 payload byte size
            u32      muOffset;     // +0x14 payload offset from the .vlt base
        };

        // The ExpN chunk (mExports points at its header, X360 +56; zeroed by
        // Initialize once the entries are consumed).
        struct ExportNode
        {
            ChunkBlock mChunk;               // +0x00
            u32        muBaseAllocExports;   // +0x08 explicit export-slot seed
            u32        muNumEntries;         // +0x0C serialised entry count
            const ExportEntry* GetEntries() const
            {
                return reinterpret_cast<const ExportEntry*>(this + 1);
            }
        };

        // One serialised PtrN fixup record (16-byte stride, records @ chunk+8;
        // Initialize walks them until an unknown type byte: 1 = zero the slot,
        // 2 = select current block, 3 = pointer fixup, 4 = cross-vault export
        // import, anything else terminates).
        struct PointerNode
        {
            u32 muSlotOffset;   // +0x00 byte offset of the 4-byte slot in the current block
            u16 muType;         // +0x04
            u16 muDepIndex;     // +0x06 dependency/data-block index
            u64 muDataOffset;   // +0x08 target offset inside that block's data
        };

    private:
        // --- DWARF member layout (attribloadandgo.h); X360 byte offsets in
        //     comments (ctor @0x8280A2E8 stores). Pointer widths are x64 here. ---

        // One owned asset/data block. Layout is X360-ATTESTED (0x82803210 Set /
        // 0x82803298 ReleaseAsset), which overrides the DecFIGS DWARF's split
        // mData(+0)/mKind(+4)/mSize(+8): the runtime only ever touches +0 and +4 and
        // NEVER +8 -- kind (high byte) and size (low 24 bits) are packed into ONE word
        // at +4. The DWARF 3-member form is PS3/source drift not realised by this build.
        //   +0  void*  mpData          // asset/block payload pointer
        //   +4  u32    muKindAndSize   // (kind << 24) | (size & 0x00FFFFFF)
        struct DataBlock
        {
            // Bind a payload block: data pointer, byte size (<= 0xFFFFFF) and kind tag.
            // (X360 0x82803210)
            void Set(void* lpData, unsigned int luSize, u8 lu8Kind);

            // Release the owned asset back to the host GC (if any) and clear the block.
            // (X360 0x82803298)
            void ReleaseAsset(Vault::AssetID lAssetId, IGarbageCollector* lpGC);

            bool         Inited()  const { return mpData != nullptr; }
            void*        GetData() const { return mpData; }
            unsigned int GetSize() const { return muKindAndSize & 0x00FFFFFFu; }
            u8           GetKind() const { return static_cast<u8>(muKindAndSize >> 24); }

        private:
            void* mpData;               // X360 +0
            u32   muKindAndSize;        // X360 +4 : (kind << 24) | (size & 0x00FFFFFF)
        };

        u64                  mVersion;           // X360 +0  (Vers chunk payload u64)
        AssetID              mUserID;            // X360 +8  (ctor zeroes; SetUserID)
        mutable unsigned int mRefCount;          // X360 +16 (ctor seeds 1; the const
                                                 //   AddRef/Release pair mutates it)
        const ExportManager& mExportMgr;         // X360 +20
        IGarbageCollector*   mGC;                // X360 +24
        DependencyNode*      mDependencies;      // X360 +28 (the DepN chunk; zeroed
                                                 //   by Initialize once consumed)
        DataBlock*           mDepData;           // X360 +32
        AssetID*             mDepIDs;            // X360 +36
        unsigned int         mNumDependencies;   // X360 +40
        unsigned int         mResolvedCount;     // X360 +44
        ChunkBlock*          mPointers;          // X360 +48 (the PtrN chunk; zeroed
                                                 //   by Initialize once applied)
        u8*                  mTransientData;     // X360 +52 (the raw .vlt image base)
        ExportNode*          mExports;           // X360 +56 (the ExpN chunk; zeroed
                                                 //   by Initialize once consumed)
        DataBlock*           mExportData;        // X360 +60 (= mDepData + numDeps)
        AssetID*             mExportIDs;         // X360 +64 (= mDepIDs + numDeps)
        unsigned int         mNumExports;        // X360 +68 (Export() cursor)
        unsigned int         mNumAllocExports;   // X360 +72 (ExpN seed + IsExported hits)
        unsigned int         mNumLoadedExports;  // X360 +76 (serialised ExpN entries)
        bool                 mInited;            // X360 +80
        bool                 mDeinited;          // X360 +81
    };

    // Attrib::Vault scalar deleting destructor (MSVC's ??_G thunk) @ 0x8280F098 -- the
    // deleting form of ~Vault. Runs the real ~Vault(), then -- when the low should-free
    // bit of the deleting flag is set -- returns the 88-byte (0x58) vault to the AttribSys
    // package allocator via the shared null-guarded census-free helper. Returns lpVault.
    // Mirrors the sibling Collection_ScalarDeletingDtor (vechashmap.cpp @ 0x8280C510).
    void* Vault_ScalarDeletingDtor(Vault* lpVault, int liDeleteFlag);
}
