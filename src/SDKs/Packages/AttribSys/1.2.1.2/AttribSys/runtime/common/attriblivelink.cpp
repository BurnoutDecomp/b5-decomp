// AttribSys live link: decode a tool's "AttribSys.xenon" edit message and apply it to the
// loaded attribute data (EditSpecifier / EditRecord / EditTable live here, as in the SDK).
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attriblivelink.h"

#include <cstdlib>   // atoi
#include <cstring>   // strstr, strncpy, strlen, memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                             // CgsCore::SPrintf
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"      // GetAttribSysAllocator / GetEaStlAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"   // AttribSysPackageAllocator
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"                   // Attrib::FindCollection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"            // Attrib::Database
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"           // Attrib::Class
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"  // Attrib::StringToKey
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"   // Attrib::TypeDesc / Attrib::Array
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h"    // ClassPrivate / Definition / ClassStaticDesc
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h" // DatabasePrivate::mClasses
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"        // Attrib::Instance / Attrib::Collection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"             // Attrib::Attribute / Attrib::Node

// EASTL allocator routing for the AttribSys containers. The console allocates every EditTable
// node from the static EASTL package allocator and never consults the map's allocator copy (the
// table copies an uninitialised default allocator), so the node hooks go straight to that
// allocator, and frees go back to it the way the AttribSys list nodes do. FLAG PC-platform: the
// vendor EASTL is newer than the console's and has an over-aligned node branch the console EASTL
// lacks; it is routed to the same allocator and is unreachable here (see the static_assert after
// EditTable).
#define EASTLAlloc(allocator, n) \
    CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Malloc((n), 0);
#define EASTLAllocAligned(allocator, n, alignment, offset) \
    CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Malloc((n), 0)
#define EASTLFree(allocator, p, size) \
    CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Free((void*)(p), static_cast<s32>(size), NULL)
#define EASTL_MAP_DEFAULT_ALLOCATOR allocator_type()
#include <EASTL/algorithm.h>   // eastl::lower_bound
#include <EASTL/map.h>         // eastl::map (Attrib::EditTable)

namespace Attrib
{
    // The edit opcode characters, in EditOpCodeLabel order (the terminator matches NUL).
    const char* kEditOpCodes = "=:+-#";

    enum EditOpCodeLabel
    {
        kReplaceLoopOp  = 0,
        kReplaceDataOp  = 1,
        kAddObjectOp    = 2,
        kRemoveObjectOp = 3,
        kShapeObjectOp  = 4,
        kTerminatorOp   = 5,
        kInvalidOp      = 6,
    };

    // Map one message character onto its opcode; kInvalidOp when it is not an opcode.
    static EditOpCodeLabel CharToEditOpCodeLabel(char lcChar)
    {
        const char* lpcOp = kEditOpCodes;
        while (*lpcOp != lcChar && *lpcOp != 0)
        {
            ++lpcOp;
        }
        if (*lpcOp != lcChar)
        {
            return kInvalidOp;
        }
        return static_cast<EditOpCodeLabel>(lpcOp - kEditOpCodes);
    }

    // -----------------------------------------------------------------------------------------
    // EditSpecifier -- the (class, collection, attribute, index) address of one edit. The three
    // keys are the full 64-bit attribute hashes.
    // -----------------------------------------------------------------------------------------
    struct EditSpecifier
    {
    public:
        EditSpecifier(u64 luClassKey, u64 luCollectionKey, u64 luAttribKey, unsigned int luIndex)
            : mClassKey(luClassKey), mCollectionKey(luCollectionKey), mAttribKey(luAttribKey), mIndex(luIndex)
        {
        }

        EditSpecifier(const EditSpecifier& lrOther)
            : mClassKey(lrOther.mClassKey), mCollectionKey(lrOther.mCollectionKey),
              mAttribKey(lrOther.mAttribKey), mIndex(lrOther.mIndex)
        {
        }

        Class*       GetClass() const;
        Collection*  GetCollection() const { return FindCollection(mClassKey, mCollectionKey); }
        u64          GetCollectionKey() const { return mCollectionKey; }
        u64          GetAttribKey() const { return mAttribKey; }
        unsigned int GetIndex() const { return mIndex; }

        bool operator<(const EditSpecifier& lrOther) const
        {
            if (mClassKey != lrOther.mClassKey)
                return mClassKey < lrOther.mClassKey;
            if (mCollectionKey != lrOther.mCollectionKey)
                return mCollectionKey < lrOther.mCollectionKey;
            if (mAttribKey != lrOther.mAttribKey)
                return mAttribKey < lrOther.mAttribKey;
            return mIndex < lrOther.mIndex;
        }

        const char* Decode(const char* lpcText);

    private:
        u64          mClassKey;
        u64          mCollectionKey;
        u64          mAttribKey;
        unsigned int mIndex;
    };

    // -----------------------------------------------------------------------------------------
    // EditRecord -- the state of one edited attribute: the original bytes (for revert) and the
    // tool-supplied extra image that pointer fixups may target.
    // -----------------------------------------------------------------------------------------
    struct EditRecord
    {
    public:
        // The op-info value of a record whose data has been replaced (and can be reverted).
        static const unsigned short KU_OPINFO_REPLACED = 7;

        EditRecord()
            : mOpInfo(0), mIsLoopEdit(0), mEditSpec(NULL), mOriginalKey(0), mOriginalLength(0),
              mOriginalRelease(0), mTypeSize(0), mExtraSize(0), mExtraImage(NULL), mOriginalImage(NULL)
        {
        }

        // The copy carries everything but the loop-edit flag, which starts clear.
        EditRecord(const EditRecord& lrOther)
            : mOpInfo(lrOther.mOpInfo), mIsLoopEdit(0), mEditSpec(lrOther.mEditSpec),
              mOriginalKey(lrOther.mOriginalKey), mOriginalLength(lrOther.mOriginalLength),
              mOriginalRelease(lrOther.mOriginalRelease), mTypeSize(lrOther.mTypeSize),
              mExtraSize(lrOther.mExtraSize), mExtraImage(lrOther.mExtraImage),
              mOriginalImage(lrOther.mOriginalImage)
        {
        }

        ~EditRecord();

        void                 SetEditSpecifier(const EditSpecifier& lrSpec) { mEditSpec = &lrSpec; }
        const EditSpecifier& GetEditSpecifier() const { return *mEditSpec; }
        bool                 IsLoopEdit() const { return mIsLoopEdit != 0; }

        EDecodeResult Process(const char* lpcMessage);

    private:
        EDecodeResult ProcessReplace(const char* lpcMessage);
        void          RevertReplace();

        unsigned short       mOpInfo;
        unsigned short       mIsLoopEdit;
        const EditSpecifier* mEditSpec;
        u64                  mOriginalKey;
        unsigned short       mOriginalLength;
        unsigned short       mOriginalRelease;
        unsigned int         mTypeSize;
        unsigned int         mExtraSize;
        unsigned char*       mExtraImage;
        union
        {
            unsigned char* mOriginalImage;
            Array*         mOriginalArray;
            Collection*    mCollection;
        };
    };

    // -----------------------------------------------------------------------------------------
    // EditTable -- every live edit, keyed by its EditSpecifier.
    // -----------------------------------------------------------------------------------------
    struct EditTable : public eastl::map<EditSpecifier, EditRecord, eastl::less<EditSpecifier>,
                                         CgsAttribSys::AttribSysPackageAllocator>
    {
    public:
        void* operator new(size_t lnBytes);
    };

    static_assert(alignof(EditTable::node_type) <= EASTL_ALLOCATOR_MIN_ALIGNMENT,
                  "EditTable nodes must never take the over-aligned allocation branch");

    // -----------------------------------------------------------------------------------------
    // TweakableAttribute -- an attribute cursor that hands out the address of an element.
    // -----------------------------------------------------------------------------------------
    struct TweakableAttribute : public Attribute
    {
    public:
        TweakableAttribute(Attribute& lrAttribute) : Attribute(lrAttribute) {}

        void* GetPointer(unsigned int luIndex) const;
    };

    // The edit table (created by the first message) and the edit callbacks.
    EditTable* gLiveLinkEditTable = NULL;
    void (*gEditNotifier)(const Collection*, u64) = NULL;
    void (*gLoopNotifier)(const Collection*, u64) = NULL;

    // The ModifyMemory diagnostic is formatted into this buffer before the length check.
    static char sacModifyMemoryMessage[1024];

    // =========================================================================================

    u64 Attribute::GetType() const
    {
        return mpNode->GetTypeDesc()->mType;
    }

    void* TweakableAttribute::GetPointer(unsigned int luIndex) const
    {
        if (mpData == NULL)
        {
            return const_cast<TweakableAttribute*>(this)->GetInternalPointer(luIndex);
        }
        if (luIndex != 0)
        {
            return NULL;
        }
        return mpData;
    }

    // Resolve the class this edit addresses in the attribute database (NULL if unregistered).
    Class* EditSpecifier::GetClass() const
    {
        return GetDatabasePrivate()->mClasses.Find(mClassKey);
    }

    // Parse "<class>.<collection>.<attrib>.<index>" up to the opcode character that ends the
    // index. Each name is hashed to its key (an empty name is key 0). Returns the opcode
    // position, or NULL if a '.' is missing or the index is longer than 15 characters.
    const char* EditSpecifier::Decode(const char* lpcText)
    {
        const char* lpcDot = strstr(lpcText, ".");
        if (lpcDot == NULL)
            return NULL;
        unsigned int luLength = static_cast<unsigned int>(lpcDot - lpcText);
        mClassKey = (lpcText != NULL && luLength != 0)
                        ? StringToKey(lpcText, luLength, KU_ATTRIB_STRING_TO_KEY_SEED) : 0;

        const char* lpcCollection = lpcDot + 1;
        lpcDot = strstr(lpcCollection, ".");
        if (lpcDot == NULL)
            return NULL;
        luLength = static_cast<unsigned int>(lpcDot - lpcCollection);
        mCollectionKey = (lpcCollection != NULL && luLength != 0)
                             ? StringToKey(lpcCollection, luLength, KU_ATTRIB_STRING_TO_KEY_SEED) : 0;

        const char* lpcAttrib = lpcDot + 1;
        lpcDot = strstr(lpcAttrib, ".");
        if (lpcDot == NULL)
            return NULL;
        luLength = static_cast<unsigned int>(lpcDot - lpcAttrib);
        mAttribKey = (lpcAttrib != NULL && luLength != 0)
                         ? StringToKey(lpcAttrib, luLength, KU_ATTRIB_STRING_TO_KEY_SEED) : 0;

        const char* lpcIndex = lpcDot + 1;
        const char* lpcEnd = lpcIndex;
        while (CharToEditOpCodeLabel(*lpcEnd) == kInvalidOp)
        {
            ++lpcEnd;
        }

        const unsigned int luIndexLength = static_cast<unsigned int>(lpcEnd - lpcIndex);
        if (luIndexLength + 1 > 16)
            return NULL;

        char lacIndex[64];
        strncpy(lacIndex, lpcIndex, luIndexLength);
        lacIndex[luIndexLength] = 0;
        mIndex = static_cast<unsigned int>(atoi(lacIndex));
        return lpcEnd;
    }

    // One hex digit; anything else reads as 0.
    static unsigned char TextToHex(char lcChar)
    {
        if (lcChar >= '0' && lcChar <= '9')
            return static_cast<unsigned char>(lcChar - '0');
        if (lcChar >= 'a' && lcChar <= 'f')
            return static_cast<unsigned char>(lcChar - 'a' + 10);
        if (lcChar >= 'A' && lcChar <= 'F')
            return static_cast<unsigned char>(lcChar - 'A' + 10);
        return 0;
    }

    // Overwrite luSize bytes at lpDest with the hex byte image lpcHex (two digits per byte,
    // first byte first).
    // FLAG PC-platform: the bytes land verbatim, so a live-link tool talking to the PC build
    // must send the PC vault's host-format image (little-endian, host-width pointers -- the
    // layout the vault transcoder writes), not the console's big-endian one.
    static void ModifyMemory(unsigned char* lpDest, unsigned int luSize, const char* lpcHex)
    {
        CgsCore::SPrintf(sacModifyMemoryMessage, sizeof(sacModifyMemoryMessage),
                         "DecodeEditMessage data content is too short (%d instead of %d)\n",
                         static_cast<int>(strlen(lpcHex) / 2), static_cast<int>(luSize));
        CGS_ASSERT(strlen(lpcHex) / 2 >= luSize, sacModifyMemoryMessage);

        for (; luSize != 0; --luSize)
        {
            const unsigned char luHigh = TextToHex(lpcHex[0]);
            const unsigned char luLow  = TextToHex(lpcHex[1]);
            lpcHex += 2;
            *lpDest++ = static_cast<unsigned char>((luHigh << 4) | luLow);
        }
    }

    // Find the bytes of the addressed attribute element: through a live instance of the
    // collection when it holds its own copy of the attribute, else through the class
    // definition (the collection's layout block or the class static data). Reports the
    // attribute's type key and its definition. NULL when the element does not exist.
    static unsigned char* RetrieveAttribDbDataPointer(const EditSpecifier& lrSpec, u64& lruType,
                                                      const Definition*& lrpDefinition)
    {
        Class* lpClass = lrSpec.GetClass();
        if (lpClass == NULL)
            return NULL;

        const ClassPrivate* lpPrivate = static_cast<const ClassPrivate*>(lpClass->GetPrivates());
        const Definition* lpBegin = lpPrivate->mDefinitions;
        const Definition* lpEnd = lpBegin + lpPrivate->mNumDefinitions;
        Definition lSearch;
        lSearch.mKey = lrSpec.GetAttribKey();
        const Definition* lpDefinition = eastl::lower_bound(lpBegin, lpEnd, lSearch);
        if (!(lpDefinition < lpEnd))
            lpDefinition = NULL;
        lrpDefinition = lpDefinition;
        if (lpDefinition == NULL)
            return NULL;

        unsigned char* lpData = NULL;
        Collection* lpCollection = lrSpec.GetCollection();
        if (lpCollection != NULL)
        {
            {
                Instance lInstance(lpCollection, NULL);
                AttributeValue lCursor;
                Attribute& lrAttribute = *reinterpret_cast<Attribute*>(
                    lInstance.Get(&lCursor, reinterpret_cast<int*>(&lInstance), lrSpec.GetAttribKey()));
                if (lrAttribute.GetLength() != 0
                    && static_cast<unsigned int>(lrAttribute.GetLength()) > lrSpec.GetIndex()
                    && !lrAttribute.IsInherited())
                {
                    TweakableAttribute lTweakable(lrAttribute);
                    lpData = static_cast<unsigned char*>(lTweakable.GetPointer(lrSpec.GetIndex()));
                    lruType = lrAttribute.GetType();
                }
            }
            if (lpData != NULL)
                return lpData;
        }

        lruType = lpDefinition->mType;
        if ((lpDefinition->mFlags & 0x2) != 0 && lpCollection != NULL)
        {
            lpData = static_cast<unsigned char*>(lpCollection->mpData);
            CGS_ASSERT(lpData != NULL, "Laid out definition but no layout found!\n");
            CGS_ASSERT(lpDefinition->mOffset < lpPrivate->mLayoutSize, "Invalid definition offset in layout!\n");
        }
        else if ((lpDefinition->mFlags & 0x10) != 0)
        {
            // GetStatic takes the narrow key type in this tree (as at its other call site).
            const ClassStaticDesc* lpStatic =
                ClassStaticDesc::GetStatic(static_cast<::Attribute::Key>(lpClass->GetKey()));
            CGS_ASSERT(lpStatic != NULL, "Static definition but no static data found!\n");
            CGS_ASSERT(lpDefinition->mOffset < lpStatic->mSize, "Invalid definition offset in static data!\n");
            lpData = static_cast<unsigned char*>(lpStatic->mStruct);
        }
        else
        {
            return NULL;
        }

        if (lpData != NULL)
        {
            lpData += lpDefinition->mOffset;
            if ((lpDefinition->mFlags & 0x1) != 0)
            {
                Array* lpArray = reinterpret_cast<Array*>(lpData);
                if (lpArray->muNumElements != 0 && lpArray->muNumElements > lrSpec.GetIndex())
                    return static_cast<unsigned char*>(lpArray->GetData(lrSpec.GetIndex()));
                lpData = NULL;
            }
        }
        return lpData;
    }

    // Undo a replace: restore the original bytes and drop the extra image.
    void EditRecord::RevertReplace()
    {
        u64 luType = 0;
        const Definition* lpDefinition;
        unsigned char* lpData = RetrieveAttribDbDataPointer(*mEditSpec, luType, lpDefinition);
        if (lpData == NULL)
            return;

        CGS_ASSERT(mOriginalImage != NULL, "RevertReplace with no original image.");
        memcpy(lpData, mOriginalImage, mTypeSize);

        if (mExtraImage != NULL)
        {
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Free(
                mExtraImage, static_cast<s32>(mExtraSize), "Attrib::EditRecord::ExtraImage");
            mExtraImage = NULL;
            mExtraSize = 0;
        }
    }

    // A replaced record reverts its attribute before it goes; the extra image is freed.
    EditRecord::~EditRecord()
    {
        if (mOriginalImage != NULL && mOpInfo == KU_OPINFO_REPLACED)
        {
            RevertReplace();
        }
        if (mExtraImage != NULL)
        {
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Free(
                mExtraImage, static_cast<s32>(mExtraSize), "Attrib::EditRecord::ExtraImage");
        }
    }

    // Dispatch on the opcode character that follows the edit specifier.
    EDecodeResult EditRecord::Process(const char* lpcMessage)
    {
        switch (CharToEditOpCodeLabel(*lpcMessage))
        {
            case kReplaceLoopOp:
                mIsLoopEdit = 1;
                return ProcessReplace(lpcMessage + 1);
            case kReplaceDataOp:
                mIsLoopEdit = 0;
                return ProcessReplace(lpcMessage + 1);
            default:
                return kDecodeInvalidOperation;
        }
    }

    // Replace the attribute element's bytes. The payload is
    //     [ "[" <type size> ":" <extra size> ":" <fixup list> "]" ] <hex data> [<hex extra>]
    // where each fixup is "<location>@<target>" (store the address of byte <target>) or
    // "<location>&<name>" / "<location>" (store NULL), comma separated. Locations and targets
    // below the type size index the attribute data, the rest index the extra image. The first
    // replace keeps a copy of the original bytes for revert.
    EDecodeResult EditRecord::ProcessReplace(const char* lpcMessage)
    {
        u64 luType = 0;
        const Definition* lpDefinition;
        unsigned char* lpData = RetrieveAttribDbDataPointer(*mEditSpec, luType, lpDefinition);
        if (lpData == NULL)
            return kDecodeCannotFindObject;

        const unsigned int luTypeSize = Database::Get().GetTypeDesc(luType).mSize;
        if (mOpInfo == 0)
        {
            mOpInfo = KU_OPINFO_REPLACED;
            if (mOriginalImage == NULL)
            {
                mTypeSize = luTypeSize;
                mOriginalImage = static_cast<unsigned char*>(
                    CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Malloc(luTypeSize, 0));
                memcpy(mOriginalImage, lpData, luTypeSize);
            }
            else
            {
                CGS_ASSERT(luTypeSize == mTypeSize, "Inconsistent type size found during live link edit.\n");
            }
        }
        else
        {
            CGS_ASSERT(luTypeSize == mTypeSize, "Inconsistent type size found during live link edit.\n");
        }

        char lacToken[112];
        unsigned int luExtraSize = 0;
        const char* lpcFixup = "]";
        if (*lpcMessage == '[')
        {
            const char* lpcCursor = lpcMessage + 1;
            char* lpcOut = lacToken;
            for (int liCount = 0; *lpcCursor != 0 && *lpcCursor != ']' && *lpcCursor != ':' && liCount < 16; ++liCount)
            {
                *lpcOut++ = *lpcCursor++;
            }
            *lpcOut = 0;
            CGS_ASSERT(static_cast<unsigned int>(atoi(lacToken)) == mTypeSize,
                       "Runtime/tooltime type size mismatch during live link edit.\n");
            if (*lpcCursor++ != ':')
                return kDecodeMalformedFixupList;

            lpcOut = lacToken;
            for (int liCount = 0; *lpcCursor != 0 && *lpcCursor != ']' && *lpcCursor != ':' && liCount < 16; ++liCount)
            {
                *lpcOut++ = *lpcCursor++;
            }
            *lpcOut = 0;
            luExtraSize = static_cast<unsigned int>(atoi(lacToken));
            if (*lpcCursor != ':')
                return kDecodeMalformedFixupList;

            lpcFixup = lpcCursor + 1;
            const char* lpcClose = lpcFixup;
            while (*lpcClose != 0 && *lpcClose != ']')
            {
                ++lpcClose;
            }
            if (*lpcClose != ']')
                return kDecodeMalformedFixupList;
            lpcMessage = lpcClose + 1;
        }

        ModifyMemory(lpData, luTypeSize, lpcMessage);
        const char* lpcExtraHex = lpcMessage + 2 * luTypeSize;
        if (luExtraSize != 0)
        {
            if (luExtraSize > mExtraSize)
            {
                if (mExtraImage != NULL)
                {
                    CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Free(
                        mExtraImage, static_cast<s32>(mExtraSize), "Attrib::EditRecord::ExtraImage");
                }
                mExtraSize = luExtraSize;
                mExtraImage = static_cast<unsigned char*>(
                    CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Malloc(luExtraSize, 0));
            }
            ModifyMemory(mExtraImage, luExtraSize, lpcExtraHex);
        }

        while (*lpcFixup != ']')
        {
            char* lpcOut = lacToken;
            for (int liCount = 0; *lpcFixup != 0 && *lpcFixup != '@' && *lpcFixup != '&' && *lpcFixup != ']' && liCount < 16; ++liCount)
            {
                *lpcOut++ = *lpcFixup++;
            }
            *lpcOut = 0;
            const unsigned int luLocation = static_cast<unsigned int>(atoi(lacToken));
            CGS_ASSERT(luLocation < mExtraSize + mTypeSize,
                       "Invalid pointer location specified in live link edit message.\n");

            // FLAG PC-platform: the console stores a 4-byte pointer; the PC build stores a
            // host-width one, so the tool's fixup locations must be host-layout offsets.
            unsigned char** lppSlot = (luLocation < mTypeSize)
                ? reinterpret_cast<unsigned char**>(lpData + luLocation)
                : reinterpret_cast<unsigned char**>(mExtraImage + (luLocation - mTypeSize));

            if (*lpcFixup == '@')
            {
                ++lpcFixup;
                lpcOut = lacToken;
                for (int liCount = 0; *lpcFixup != 0 && *lpcFixup != ',' && *lpcFixup != ']' && liCount < 16; ++liCount)
                {
                    *lpcOut++ = *lpcFixup++;
                }
                *lpcOut = 0;
                const unsigned int luTarget = static_cast<unsigned int>(atoi(lacToken));
                CGS_ASSERT(luTarget < mExtraSize + mTypeSize,
                           "Invalid pointer target specified in live link edit message.\n");
                *lppSlot = (luTarget < mTypeSize) ? lpData + luTarget : mExtraImage + (luTarget - mTypeSize);
            }
            else
            {
                if (*lpcFixup == '&')
                {
                    ++lpcFixup;
                }
                while (*lpcFixup != 0 && *lpcFixup != ',' && *lpcFixup != ']')
                {
                    ++lpcFixup;
                }
                *lppSlot = NULL;
            }

            if (*lpcFixup == ',')
                ++lpcFixup;
        }
        return kDecodeSuccessful;
    }

    void* EditTable::operator new(size_t lnBytes)
    {
        return CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator()->Malloc(lnBytes, 0);
    }

    void SetEditNotifier(void (*lpfnNotifier)(const Collection*, u64))
    {
        gEditNotifier = lpfnNotifier;
    }

    // Apply one message: find (or create) the edit record for the addressed attribute, run
    // its edit, and tell the loop or edit notifier on success.
    EDecodeResult DecodeLiveLinkMessage(const char* lpcMessage)
    {
        EditSpecifier lSpec(0, 0, 0, 0);
        const char* lpcEdit = lSpec.Decode(lpcMessage);
        if (lpcEdit == NULL)
            return kDecodeMalformedObjectName;

        if (gLiveLinkEditTable == NULL)
            gLiveLinkEditTable = new EditTable;

        EditTable::iterator lIt = gLiveLinkEditTable->find(lSpec);
        if (lIt == gLiveLinkEditTable->end())
        {
            lIt = gLiveLinkEditTable->insert(EditTable::value_type(lSpec, EditRecord())).first;
            if (lIt == gLiveLinkEditTable->end())
                return kDecodeUnableToAllocateEditRecord;
            lIt->second.SetEditSpecifier(lIt->first);
        }

        EditRecord& lrRecord = lIt->second;
        const EDecodeResult leResult = lrRecord.Process(lpcEdit);
        if (leResult == kDecodeSuccessful)
        {
            const EditSpecifier& lrSpec = lrRecord.GetEditSpecifier();
            if (lrRecord.IsLoopEdit() && gLoopNotifier != NULL)
                gLoopNotifier(lrSpec.GetCollection(), lrSpec.GetAttribKey());
            else if (gEditNotifier != NULL)
                gEditNotifier(lrSpec.GetCollection(), lrSpec.GetAttribKey());
        }
        return leResult;
    }
}
