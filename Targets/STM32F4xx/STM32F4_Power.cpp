// Copyright Microsoft Corporation
// Copyright Oberon microsystems, Inc
// Copyright GHI Electronics, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "STM32F4.h"

#define TOTAL_POWER_CONTROLLERS 1
/* CR register bit mask */
#define CR_DS_MASK               ((uint32_t)0xFFFFFFFC)
#define CR_PLS_MASK              ((uint32_t)0xFFFFFF1F)

extern "C" void SystemInit();

struct PowerState {
    uint32_t controllerIndex;
    bool tableInitialized;
};

const char* powerApiNames[TOTAL_POWER_CONTROLLERS] = {
    "GHIElectronics.TinyCLR.NativeApis.STM32F4.PowerController\\0"
};

static TinyCLR_Power_Controller powerControllers[TOTAL_POWER_CONTROLLERS];
static TinyCLR_Api_Info powerApi[TOTAL_POWER_CONTROLLERS];
static PowerState powerStates[TOTAL_POWER_CONTROLLERS];

void STM32F4_Power_EnsureTableInitialized() {
    for (auto i = 0; i < TOTAL_POWER_CONTROLLERS; i++) {
        if (powerStates[i].tableInitialized)
            continue;

        powerControllers[i].ApiInfo = &powerApi[i];
        powerControllers[i].Initialize = &STM32F4_Power_Initialize;
        powerControllers[i].Uninitialize = &STM32F4_Power_Uninitialize;
        powerControllers[i].Reset = &STM32F4_Power_Reset;
        powerControllers[i].SetLevel = &STM32F4_Power_SetLevel;

        powerApi[i].Author = "GHI Electronics, LLC";
        powerApi[i].Name = powerApiNames[i];
        powerApi[i].Type = TinyCLR_Api_Type::PowerController;
        powerApi[i].Version = 0;
        powerApi[i].Implementation = &powerControllers[i];
        powerApi[i].State = &powerStates[i];

        powerStates[i].controllerIndex = i;
        powerStates[i].tableInitialized = true;
    }
}

const TinyCLR_Api_Info* STM32F4_Power_GetRequiredApi() {
    STM32F4_Power_EnsureTableInitialized();

    return &powerApi[0];
}

void STM32F4_Power_AddApi(const TinyCLR_Api_Manager* apiManager) {
    STM32F4_Power_EnsureTableInitialized();

    for (auto i = 0; i < TOTAL_POWER_CONTROLLERS; i++) {
        apiManager->Add(apiManager, &powerApi[i]);
    }

    apiManager->SetDefaultName(apiManager, TinyCLR_Api_Type::PowerController, powerApi[0].Name);
}

TinyCLR_Result STM32F4_Power_SetLevel(const TinyCLR_Power_Controller* self, TinyCLR_Power_Level level, TinyCLR_Power_WakeSource wakeSource, uint64_t data) {
    volatile uint32_t tmpreg = 0;

    switch (level) {
    case TinyCLR_Power_Level::Sleep1: // Sleep
    case TinyCLR_Power_Level::Sleep2: // Sleep
    case TinyCLR_Power_Level::Sleep3: // Sleep

        TinyCLR_UsbClient_Uninitialize(nullptr);

        RCC->APB1ENR |= RCC_APB1ENR_PWREN;

        /* Select the regulator state in STOP mode ---------------------------------*/
        tmpreg = PWR->CR;
        /* Clear PDDS and LPDSR bits */
        tmpreg &= CR_DS_MASK;

        /* Set LPDSR bit according to PWR_Regulator value */
        tmpreg |= PWR_CR_LPDS;

        /* Store the new value */
        PWR->CR = tmpreg;

        /* Set SLEEPDEEP bit of Cortex System Control Register */
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

        __WFI();

        /* Reset SLEEPDEEP bit of Cortex System Control Register */
        SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP_Msk);

        SystemInit();

        TinyCLR_UsbClient_Initialize(nullptr);

        break;

    case TinyCLR_Power_Level::Off:// Off - Non wakeup

        /* Clear Wakeup flag */
        PWR->CR |= PWR_CR_CWUF;

        /* Select STANDBY mode */
        PWR->CR |= PWR_CR_PDDS;

        /* Set SLEEPDEEP bit of Cortex System Control Register */
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

        /* Request Wait For Interrupt */
        __WFI();

        break;

    case TinyCLR_Power_Level::Idle:   // Idle
        PWR->CR |= PWR_CR_CWUF;

        __WFI(); // sleep and wait for interrupt
        return TinyCLR_Result::Success;

    case TinyCLR_Power_Level::Custom: // Custom - NotSupported
        //TODO
        return TinyCLR_Result::NotSupported;

    case TinyCLR_Power_Level::Active: // Active - Highest performance
    default:
        return TinyCLR_Result::Success;
    }

    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_Power_Reset(const TinyCLR_Power_Controller* self, bool runCoreAfter) {
#if defined BOOTLOADER_HOLD_VALUE && defined BOOTLOADER_HOLD_ADDRESS && BOOTLOADER_HOLD_ADDRESS > 0
    if (!runCoreAfter)
        *((uint32_t*)BOOTLOADER_HOLD_ADDRESS) = BOOTLOADER_HOLD_VALUE;
#endif

    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos)  // unlock key
        | (1 << SCB_AIRCR_SYSRESETREQ_Pos); // reset request

    while (1); // wait for reset

    TinyCLR_Result::InvalidOperation;
}

TinyCLR_Result STM32F4_Power_Initialize(const TinyCLR_Power_Controller* self) {
    return TinyCLR_Result::Success;
}

TinyCLR_Result STM32F4_Power_Uninitialize(const TinyCLR_Power_Controller* self) {
    return TinyCLR_Result::Success;
}
