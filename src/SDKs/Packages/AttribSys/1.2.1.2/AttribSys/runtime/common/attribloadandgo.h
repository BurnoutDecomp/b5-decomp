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

    // The host garbage-collector callback. AttribSys calls ReleaseData when it
    // reclaims a vault's asset so the host can free any transient data it attached.
    // Vtable order from DWARF (attribloadandgo.h:507): [0] ~IGarbageCollector,
    // [1] ReleaseData.
    struct IGarbageCollector
    {
        virtual ~IGarbageCollector() {}

        // luType selects the asset class, lAssetId names the asset, lpData/luSize
        // the transient block being released.
        virtual void ReleaseData(u8 luType, Attribute::HashInt lAssetId,
                                 void* lpData, unsigned int luSize) = 0;
    };

    // Attrib::Vault -- one loaded serialised attribute vault. Member layout from the
    // DecFIGS DWARF (attribloadandgo.h); the dependency counters mNumDependencies(+40)
    // / mResolvedCount(+44) back HasUnresolvedDependency, confirmed against the X360
    // RegisterSchema asm.
    class Vault
    {
    public:
        // Hashed asset id (the serialised dependency/export keys).
        typedef Attribute::HashInt AssetID;

        // Construct over the export policies + serialised blob (X360 0x8280A2E8).
        // lAssetId is the vault's own id (0 for the schema vault); lpData/luSize the
        // .vlt image; lbType the EAttribSysVaultType; lpGC the host GC callback.
        Vault(ExportManager& lExportMgr, AssetID lAssetId, void* lpData,
              unsigned int luSize, u8 lbType, IGarbageCollector* lpGC);
        ~Vault();

        IGarbageCollector* GarbageCollector() const;

        // Bind a resolved dependency's data into the vault (X360 0x82803338). The
        // arguments are (dependency index, data, size, asset-flag).
        void ResolveDependency(unsigned int luIndex, void* lpData,
                               unsigned int luSize, u8 lbIsAsset);

        // True while not every declared dependency has been resolved.
        bool HasUnresolvedDependency() const { return mResolvedCount < mNumDependencies; }

        // Commit the vault into the live attribute database (X360 0x8280A660).
        void Initialize();

    private:
        // --- DWARF member layout (attribloadandgo.h). Pointer widths are x64 here;
        //     the load-bearing pair for this TU is mNumDependencies / mResolvedCount. ---
        struct DependencyNode;

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

        struct PointerNode;
        struct ExportNode;

        const ExportManager& mExportMgr;
        IGarbageCollector*   mGC;
        DependencyNode*      mDependencies;
        DataBlock*           mDepData;
        AssetID*             mDepIDs;
        unsigned int         mNumDependencies;   // X360 +40
        unsigned int         mResolvedCount;     // X360 +44
        PointerNode*         mPointers;
        u8*                  mTransientData;
        ExportNode*          mExports;
        DataBlock*           mExportData;
        AssetID*             mExportIDs;
        unsigned int         mNumExports;
        unsigned int         mNumAllocExports;
        unsigned int         mNumLoadedExports;
        bool                 mInited;
        bool                 mDeinited;
    };
}
