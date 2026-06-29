#ifndef CGS_APT_COMPONENT_LIST_H
#define CGS_APT_COMPONENT_LIST_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::KeyValue
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"                       // AptValue

// ============================================================================
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptComponentList.h
//
// CgsGui::AptComponentList - the fixed-capacity, parallel-array store the APT
// (Scaleform-style) GUI interface keeps its per-component state in. The class
// IS the storage: every accessor takes the list base (`this`) and a component
// index in [0,256) and reads/writes one of its parallel tables. Reconstructed
// from BURNOUT_X360_ARTIST.XEX; the accessors are inlined in the X360 build (the
// baked assert path/line is CgsAptCommunicator.h:158..242, where they are
// declared inline) but home here per the project's mirrored-path layout.
//
// Declaration shape (members, names, element types) is taken verbatim from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/.../CgsAptCommunicator.h:102-140),
// which is a high-confidence near-ancestor of ARTIST. The X360 word/byte index
// math confirms each member's displacement: every word table is accessed as
// `4*(<word index>) + this` and the byte table as `<byte index> + this + 0x8C00`.
// The two pointer tables are loaded/stored with lwzx/stwx (32-bit on the PPC
// target); Hex-Rays renders those as `int`, but the DWARF (and the Get/Set
// return/parameter types) prove they are pointers, so they are modelled as such:
//
//   maapComponentData[256][32]    word 0x0000..0x1FFF  KeyValue*  (32 keys/comp)
//     -> word index 32*comp + key                       (Get/SetKeyValue)
//   mapComponentReference[256]    word 0x2000..0x20FF  AptValue*
//     -> word index comp + 0x2000                       (Get/SetAptValue)
//   mauHashedName[256]            word 0x2100..0x21FF  u32
//     -> word index comp + 0x2100  (== 8448)            (SetHashedName)
//   mauHashedReferenceName[256]   word 0x2200..0x22FF  u32
//     -> word index comp + 0x2200  (== 8704)            (SetHashedReferenceName)
//   maiNumUsedData[256]           byte 0x8C00..0x8CFF  u8
//     -> byte index comp + 0x8C00  (== 35840 == word 0x2300, i.e. directly after
//        the word tables, so the byte table opens where the words close)
//                                                       (Get/SetUsedData)
//   maacName[256][256]            byte 0x8D00..        char
//     -> the per-component name table (DWARF CgsAptCommunicator.h:140); not
//        touched by this TU but modelled so the layout is complete.
// ============================================================================

namespace CgsGui
{
    class AptComponentList
    {
    public:
        // CgsAptCommunicator.h:104 (DWARF) - component index must be in [0,256).
        static const s32 KU_MAX_COMPONENTS = 256;
        // CgsAptCommunicator.h:105 (DWARF) - 32 key slots per component
        // (the maapComponentData stride: word index 32*comp).
        static const s32 KI_MAX_DATA_PER_COMPONENT = 32;
        // CgsAptCommunicator.h:139 (DWARF) - per-component name buffer length.
        static const s32 KI_COMPONENT_NAME_LENGTH = 256;

        // ----- accessors (X360 inlined into CgsAptCommunicator) -----

        // @ 0x82846A68 : return mapComponentReference[liComponent], bounds-asserted.
        AptValue* GetAptValue(s32 liComponent) const;

        // @ 0x828469E8 : return maapComponentData[liComponent][liKey], bounds-asserted
        //               on the component index (the X360 asserts only the component).
        KeyValue* GetKeyValue(s32 liComponent, s32 liKey) const;

        // @ 0x82846AE0 : return maiNumUsedData[liComponent], bounds-asserted.
        u8 GetUsedData(s32 liComponent) const;

        // Read accessors the X360 inlines directly into CgsAptCommunicator (the
        // FindAptComponent hash scan reads mauHashedName at the instance's +0x8400;
        // the debug logs read the per-component name text maacName). They mirror the
        // existing SetHashedName setter and the maacName name store.
        u32         GetHashedName(s32 liComponent) const { return mauHashedName[liComponent]; }
        const char* GetName(s32 liComponent)       const { return maacName[liComponent]; }

        // @ 0x82846BE8 : mapComponentReference[liComponent] = lpValue, bounds-asserted.
        void SetAptValue(s32 liComponent, AptValue* lpValue);

        // @ 0x82846B60 : maapComponentData[liComponent][liKey] = lpKeyValue,
        //               bounds-asserted on the component index.
        void SetKeyValue(s32 liComponent, s32 liKey, KeyValue* lpKeyValue);

        // @ 0x82846C68 : maiNumUsedData[liComponent] = lu8Used, bounds-asserted.
        void SetUsedData(s32 liComponent, u8 lu8Used);

        // @ 0x82846CE8 : mauHashedName[liComponent] = luHash, bounds-asserted.
        void SetHashedName(s32 liComponent, u32 luHash);

        // @ 0x82846D68 : mauHashedReferenceName[liComponent] = luHash, bounds-asserted.
        void SetHashedReferenceName(s32 liComponent, u32 luHash);

        // @ 0x8284E1F0 : move component liFrom's record (keys + component reference +
        //               hashed name + hashed reference name + used flag) into slot
        //               liTo, then clear slot liFrom. Both indices bounds-asserted.
        void MoveComponent(s32 liFrom, s32 liTo);

    private:
        // CgsAptCommunicator.h:132 (DWARF) - word 0x0000 : 256 components x 32 key
        // pointers (Get/SetKeyValue index 32*comp + key).
        KeyValue* maapComponentData[KU_MAX_COMPONENTS][KI_MAX_DATA_PER_COMPONENT];

        // CgsAptCommunicator.h:133 (DWARF) - word 0x2000 : one component reference
        // (the AptValue bound to the component) per component.
        AptValue* mapComponentReference[KU_MAX_COMPONENTS];

        // CgsAptCommunicator.h:134 (DWARF) - word 0x2100 : hashed component name.
        u32 mauHashedName[KU_MAX_COMPONENTS];

        // CgsAptCommunicator.h:135 (DWARF) - word 0x2200 : hashed reference name.
        u32 mauHashedReferenceName[KU_MAX_COMPONENTS];

        // CgsAptCommunicator.h:136 (DWARF) - byte 0x8C00 : "used"/data-count flag.
        u8 maiNumUsedData[KU_MAX_COMPONENTS];

        // CgsAptCommunicator.h:140 (DWARF) - byte 0x8D00 : per-component name text.
        char maacName[KU_MAX_COMPONENTS][KI_COMPONENT_NAME_LENGTH];
    };
}

#endif // CGS_APT_COMPONENT_LIST_H
