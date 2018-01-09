#include <TinyCLR.h>
#include "Seeed_TinyCLR_WioLTE.h"
#include "Seeed_TinyCLR_WioLTE_ApiInfo.h"

static TinyCLR_Api_Info ApiInfo;

const TinyCLR_Api_Info* Seeed_TinyCLR_WioLTE_GetApi()
{
	ApiInfo.Author = "Seeed";
	ApiInfo.Name = "Seeed.TinyCLR.WioLTE.Interop";
	ApiInfo.Type = TinyCLR_Api_Type::Custom;
	ApiInfo.Version = 0;
	ApiInfo.Count = 1;
	ApiInfo.Implementation = &Interop_Seeed_TinyCLR_WioLTE;

	return &ApiInfo;
}
