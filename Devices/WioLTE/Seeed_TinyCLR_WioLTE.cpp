#include "Seeed_TinyCLR_WioLTE.h"

static const TinyCLR_Interop_MethodHandler methods[] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::Init___VOID,
    Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::LedSetRGB___VOID__U1__U1__U1,
    nullptr,
    Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::slre_match___STATIC___I4__STRING__STRING,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

const TinyCLR_Interop_Assembly Interop_Seeed_TinyCLR_WioLTE = {
    "Seeed.TinyCLR.WioLTE",
    0x0F93BFBC,
    methods
};
