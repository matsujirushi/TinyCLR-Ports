#include <TinyCLR.h>
#include "WioLTE_Controller.h"
#include "WioLTE_ApiInfo.h"

static TinyCLR_Api_Info ApiInfo;

const TinyCLR_Api_Info* WioLTE_GetApi()
{
	ApiInfo.Author = "Seeed";
	ApiInfo.Name = "Seeed.TinyCLR.NativeApis.WioLTE.WioLTEProvider";
	ApiInfo.Type = TinyCLR_Api_Type::Custom;
	ApiInfo.Version = 0;
	ApiInfo.Count = 1;
	ApiInfo.Implementation = &Interop_LedBlink2;

	return &ApiInfo;
}
