#include "GameSource/Gui/Flow/Screen/States/BrnImageGallery.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash
#include "GameSource/GameState/BrnGameStateSharedIO.h"          // BrnGameState::GameStateModuleIO::EImageGalleryType
#include "GameSource/Gui/BrnGuiCache.h"                         // BrnGui::GuiCache

// BrnGui::ImageGalleryState -- reconstructed from BURNOUT_X360_ARTIST.XEX. This TU bodies the
// out-of-line ledger surface: the compiler-emitted ctor plus GetCurrentCategory /
// ClearExpectedComponent / SetExpectedComponent / GetGsmIOCategoryFromGuiEnum. The four
// header-inline exports (GetResourcesToLoad / SetExpectedAptComponentList / UpdateWFInit /
// HideMiddleImage) live in BrnImageGallery.h.

namespace BrnGui
{
namespace
{
    // FLAG boundary: the apt-component watcher half of the cache. BrnGui::GuiCache exposes no
    // committed ClearExpectedAptComponentList accessor, so it is reached here through a free
    // boundary helper (the same discipline BrnBootLegal uses). Body links from the GuiCache TU;
    // replace with a named GuiCache accessor when homed.
    struct ImageGalleryCacheBoundary
    {
        static void ClearExpectedAptComponentList(GuiCache* lpCache, GuiFlow leFlow);  // FLAG
    };
}   // anonymous namespace

// ---- ImageGalleryState (ctor) @ 0x82500328 ----------------------------------
// Compiler-emitted construction of the image-gallery flow state. The X360 writes the state
// vtable (+0x000) then a long run of embedded sub-object vtable stores (the four category tabs,
// the 30-slot carousel-overview selectable array walked by the stride-0xA8 loop, the three
// carousel items, animators and text fields). Those per-slot vtables belong to embedded widget
// subobjects whose own ctors lay them down; the modelled effect is 'construct the CgsGui::State
// base + the embedded GUI sub-objects', which default member construction reproduces. No member
// payload beyond vtables is set here.
ImageGalleryState::ImageGalleryState()
    : CgsGui::State()
{
}

// ---- GetCurrentCategory @ 0x82482D40 ----------------------------------------
// The currently highlighted image category (0..3). Reads
// mCategorySelectableGroup.GetHighlightedIndex() as a signed byte and range-checks it against
// [E_GUI_IMAGE_CATEGORIES_FIRST(0), E_GUI_IMAGE_CATEGORIES_COUNT(4)).
EGuiImageCategories ImageGalleryState::GetCurrentCategory()
{
    const s8 liCategory = static_cast<s8>(muHighlightedCategoryIndex);   // X360 +3309

    // First assert: unsigned >= 0x80 == signed < 0 (below E_GUI_IMAGE_CATEGORIES_FIRST).
    CGS_ASSERT(liCategory >= 0,
               "mCategorySelectableGroup.GetHighlightedIndex() >= E_GUI_IMAGE_CATEGORIES_FIRST");
    CGS_ASSERT(liCategory < 4,
               "mCategorySelectableGroup.GetHighlightedIndex() < E_GUI_IMAGE_CATEGORIES_COUNT");
    return static_cast<EGuiImageCategories>(liCategory);
}

// ---- ClearExpectedComponent @ 0x82482CC0 ------------------------------------
// Clear the pending expected-component list: zero the seven id slots and the live count, assert
// the cache pointer, then drop the flow-0 watch list.
void ImageGalleryState::ClearExpectedComponent()
{
    for (u32 luSlot = 0; luSlot < KU_MAX_INIT_COMPONENTS_NUM; ++luSlot)
    {
        mauExpectedComponentIds[luSlot] = 0;   // X360 +64 (x7)
    }
    muNumExpectedComponents = 0;               // X360 +92

    CGS_ASSERT(mpGuiCache, "mpGuiCache");       // BrnImageGallery.h:310
    ImageGalleryCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, E_GUIFLOW_SCREEN);
}

// ---- SetExpectedComponent @ 0x82482BD8 --------------------------------------
// Register one expected apt component by name: bounds-check the 7-slot list, hash the
// NUL-terminated name (CgsHash reflected CRC-32 over strlen bytes), append it and bump the
// count. The X360 builds the overflow assert text via StrStream; collapsed to the static
// CGS_ASSERT per convention. DWARF return type is void (the asm's r3 == the CalculateHash
// result is a dead tail-call leftover).
void ImageGalleryState::SetExpectedComponent(const char* lpacComponentName)
{
    CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
               "No space for new expected component");   // BrnImageGallery.h:284

    // strlen the name (the X360's while(*p++) walk) then hash the byte span.
    const char* lpcEnd = lpacComponentName;
    while (*lpcEnd++)
    {
    }
    const s32 liLength = static_cast<s32>(lpcEnd - lpacComponentName - 1);

    // CalculateHash takes a mutable char*; the name is read-only here.
    const u32 luHash =
        CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacComponentName), liLength);

    mauExpectedComponentIds[muNumExpectedComponents] = luHash;   // X360 +64 base
    ++muNumExpectedComponents;                                   // X360 +92
}

// ---- GetGsmIOCategoryFromGuiEnum @ 0x824847A0 -------------------------------
// Map an image-gallery GUI category (takedown/mugshot/rulebreaker/finish) onto its
// GameStateModuleIO EImageGalleryType. The X360 default case builds the assert text via
// StrStream ('Invalid image category in Image Gallery : ' << leCategory << '\n'); collapsed to
// the static CGS_ASSERT per convention (int + newline dropped).
BrnGameState::GameStateModuleIO::EImageGalleryType
ImageGalleryState::GetGsmIOCategoryFromGuiEnum(EGuiImageCategories leImageCategory)
{
    using namespace BrnGameState;
    switch (leImageCategory)
    {
        case 0:  return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT;   // 0
        case 1:  return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_MUGSHOT;            // 4
        case 2:  return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT;  // 2
        case 3:  return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT;    // 3
        default:
            CGS_ASSERT(false, "Invalid image category in Image Gallery : ");   // cpp:1217
            return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT;   // 0 (LABEL_7 fallthrough)
    }
}
}   // namespace BrnGui
