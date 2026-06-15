#pragma once

#include "types.hpp"

// GUI resource-request vocabulary recovered from the DecFIGS DWARF
// (CgsGuiResourceModuleIO.h): the resource-type / load-unload enums and the
// (id, type) tuple that every GUI state hands to the loader via GetResourcesToLoad.
namespace CgsGui
{
    enum ResourceRequestTypes
    {
        E_GUI_RESOURCETYPE_START                 = 0,
        E_GUI_RESOURCETYPE_BUNDLE                = 1,
        E_GUI_RESOURCETYPE_HD_APT_BUNDLE         = 2,
        E_GUI_RESOURCETYPE_SD_APT_BUNDLE         = 3,
        E_GUI_RESOURCETYPE_APT                   = 4,
        E_GUI_RESOURCETYPE_APT_LOADING_SCREEN    = 5,
        E_GUI_RESOURCETYPE_APT_PERSISTENT        = 6,
        E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE       = 7,
        E_GUI_RESOURCETYPE_FLAPT_SD_BUNDLE       = 8,
        E_GUI_RESOURCETYPE_FLAPT_PERSISTENT      = 9,
        E_GUI_RESOURCETYPE_TEXTURE               = 10,
        E_GUI_RESOURCETYPE_LOCALISED_TEXT        = 11,
        E_GUI_RESOURCETYPE_LOCALISED_TEXT_BUNDLE = 12,
        E_FONT_RESOURCETYPE_HD_BUNDLE            = 13,
        E_FONT_RESOURCETYPE_SD_BUNDLE            = 14,
        E_FONT_RESOURCETYPE_FONTDATA             = 15,
        E_GUI_RESOURCETYPE_FSM_BUNDLE            = 16,
        E_GUI_RESOURCETYPE_FSM                   = 17,
        E_GUI_RESOURCETYPE_PFX_BUNDLE            = 18,
        E_GUI_RESOURCETYPE_PFX                   = 19,
        E_GUI_RESOURCETYPE_PFX_COLOURCUBE_DICTIONARY = 20,
        E_GUI_RESOURCETYPE_PFX_COLOURCUBE        = 21,
        E_GUI_RESOURCETYPE_DONE                  = 22,
    };

    enum ResourceRequestLoadUnload
    {
        E_GUI_RESOURCEREQUEST_LOAD   = 0,
        E_GUI_RESOURCEREQUEST_UNLOAD = 1,
    };

    struct sResourceTuple
    {
        u32                 muId;
        ResourceRequestTypes meType;
    };
}
