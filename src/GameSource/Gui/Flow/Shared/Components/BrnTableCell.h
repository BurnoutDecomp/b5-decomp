#pragma once

// ===================================================================================
// BrnGui::TableCell  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTableCell.h
//   class:BrnGui::TableCell
//
// One cell of a GUI table row: it wraps a single child GUI component (either a text field
// or an icon) and remembers which kind it is, so a row can drive its cells generically.
// BrnGui::TableRow embeds an array of 16 of these BY VALUE (maCells[16]); this header is
// that hard by-value-containment dependency.
//
// CLASS SHAPE (DecFIGS DWARF GameSource/Gui/Flow/Shared/Components/BrnTableCell.h:42,
// X360-attested):
//   * polymorphic (vptr @+0x00 -- the X360 calls Select / Update through the vtable),
//   * meComponentType  @+0x04 (TableCellComponentTypes),
//   * mpComponent      @+0x08 (CgsGui::GuiComponent*).
// sizeof == 0x0C (12 bytes) -- proven by TableRow::Construct, which strides the cell array
// 0xC bytes/element (cell[i] at row+0x238 + 0xC*i) and clears the two data words at +0x04
// and +0x08 of each.
//
// Only DECLARATIONS are provided here (the cell bodies are their own TU,
// GameSource/Gui/Flow/Shared/Components/BrnTableCell.cpp); the per-TU compile gate needs
// only these declarations. All access is by name; offsets above are X360 references.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent / StateInterface
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"       // ParameterFormatType (SetLocalisedText)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                       // BrnGui::GuiFlow

namespace BrnGui
{
    class GuiCache;

    class TableCell
    {
    public:
        // BrnTableCell.h:45 -- max cell text length.
        static const u8 KI_MAX_TEXT_LENGTH = 64;

        // The kind of child component a cell holds (DWARF BrnTableCell.h:47).
        enum TableCellComponentTypes
        {
            E_TABLECELLCOMPONENTTYPES_NOTYPE    = 0,
            E_TABLECELLCOMPONENTTYPES_TEXTFIELD = 1,
            E_TABLECELLCOMPONENTTYPES_ICON      = 2,
            E_TABLECELLCOMPONENTTYPES_COUNT     = 3,
        };

        // @0x824F87C0-region (BrnTableCell.cpp:46) -- build the cell: name it, wire its
        // state interface, take ownership of the child component and record its kind. The
        // trailing u64 is the cell's apt id seed (the X360 passes 0xFFFFFFFF from TableRow).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       CgsGui::GuiComponent* lpComponent, TableCellComponentTypes leType,
                       const char* lpacParentName, u64 luAptId);

        void Clear();
        bool IsText();
        bool IsIcon();

        // The wrapped child component (+0x08). Exposed so a row can register each cell's
        // component with the GUI cache BY NAME (TableRow::AppendExpectedAptComponent reads
        // mpComponent and forwards mpComponent->GetName()).
        CgsGui::GuiComponent* GetComponent() const { return mpComponent; }

        const char* GetText() const;
        void SetText(const char* lpacText);
        bool SetLocalisedText(const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liNumParams, const char* const* lppacParams,
                              CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats);
        u32 GetIconState() const;
        void SetIconState(u32 luState);

        virtual void Select();
        virtual void Update();

        // @0x824F87C0 -- register the cell's child component (by name hash) as expected on the
        // given flow layer. TableRow::AppendExpectedAptComponent forwards each cell here.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

    private:
        TableCellComponentTypes meComponentType;   // +0x04
        CgsGui::GuiComponent*   mpComponent;       // +0x08
    };
}
