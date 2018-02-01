#pragma once

#include <TinyCLR.h>

struct Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_AtSerial {
    static const size_t FIELD____SerialWriter___GHIElectronicsTinyCLRStorageGHIElectronicsTinyCLRStorageStreamsDataWriter = 1;
    static const size_t FIELD____SerialReader___GHIElectronicsTinyCLRStorageGHIElectronicsTinyCLRStorageStreamsDataReader = 2;
    static const size_t FIELD____ReadedByteValid___BOOLEAN = 3;
    static const size_t FIELD____ReadedByte___U1 = 4;
};

struct Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_AtSerial__ResponseCompare {
    static const size_t FIELD___Type___SeeedTinyCLRWioLTEAtSerialResponseCompareType = 1;
    static const size_t FIELD___Pattern___STRING = 2;
};

struct Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTE {
    static const size_t FIELD____Native___SeeedTinyCLRWioLTEWioLTENative = 1;
    static const size_t FIELD____ModulePwrPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 2;
    static const size_t FIELD____AntPwrPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 3;
    static const size_t FIELD____EnableVccbPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 4;
    static const size_t FIELD____RgbLedPwrPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 5;
    static const size_t FIELD____SdPowrPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 6;
    static const size_t FIELD____PwrKeyPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 7;
    static const size_t FIELD____ResetModulePin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 8;
    static const size_t FIELD____StatusPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 9;
    static const size_t FIELD____DtrPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 10;
    static const size_t FIELD____WakeupInPin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 11;
    static const size_t FIELD____WDisablePin___GHIElectronicsTinyCLRDevicesGHIElectronicsTinyCLRDevicesGpioGpioPin = 12;
    static const size_t FIELD____Module___SeeedTinyCLRWioLTEAtSerial = 13;
};

struct Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative {
    static TinyCLR_Result Init___VOID(const TinyCLR_Interop_MethodData md);
    static TinyCLR_Result LedSetRGB___VOID__U1__U1__U1(const TinyCLR_Interop_MethodData md);

    static TinyCLR_Result slre_match___STATIC___I4__STRING__STRING(const TinyCLR_Interop_MethodData md);
    static TinyCLR_Result slre_match2___STATIC___STRING__STRING__STRING(const TinyCLR_Interop_MethodData md);
};

struct Interop_Seeed_TinyCLR_WioLTE_System_Diagnostics_Stopwatch {
    static const size_t FIELD____Running___BOOLEAN = 1;
    static const size_t FIELD____StartTime___mscorlibSystemDateTime = 2;
    static const size_t FIELD____ElapsedMilliseconds___I8 = 3;
};

extern const TinyCLR_Interop_Assembly Interop_Seeed_TinyCLR_WioLTE;
