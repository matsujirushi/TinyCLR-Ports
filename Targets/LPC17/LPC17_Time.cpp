// Copyright Microsoft Corporation
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

#include "LPC17.h"

#define TIMER_IDLE_VALUE  0x0000FFFFFFFFFFFFull

#define SLOW_CLOCKS_PER_SECOND LPC17_AHB_CLOCK_HZ
#define SLOW_CLOCKS_TEN_MHZ_GCD           1000000   // GCD(SLOW_CLOCKS_PER_SECOND, 10M)
#define SLOW_CLOCKS_MILLISECOND_GCD          1000   // GCD(SLOW_CLOCKS_PER_SECOND, 1k)
#define CLOCK_COMMON_FACTOR               1000000   // GCD(SYSTEM_CLOCK_HZ, 1M)
#define CORTEXM_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS 3

struct LPC17_Timer_Driver {

    uint64_t m_lastRead;
    uint32_t m_currentTick;
    uint32_t m_periodTicks;

    TinyCLR_Time_TickCallback m_DequeuAndExecute;

    static bool Initialize();
    static bool UnInitialize();
    static void Reload(uint32_t value);

};

static TinyCLR_Time_Provider timeProvider;
static TinyCLR_Api_Info timeApi;

const TinyCLR_Api_Info* LPC17_Time_GetApi() {
    timeProvider.Parent = &timeApi;
    timeProvider.Index = 0;
    timeProvider.GetInitialTime = &LPC17_Time_GetInitialTime;
    timeProvider.GetTimeForProcessorTicks = &LPC17_Time_GetTimeForProcessorTicks;
    timeProvider.GetProcessorTicksForTime = &LPC17_Time_GetProcessorTicksForTime;
    timeProvider.GetCurrentProcessorTicks = &LPC17_Time_GetCurrentTicks;
    timeProvider.SetTickCallback = &LPC17_Time_SetCompareCallback;
    timeProvider.SetNextTickCallbackTime = &LPC17_Time_SetCompare;
    timeProvider.Acquire = &LPC17_Time_Acquire;
    timeProvider.Release = &LPC17_Time_Release;
    timeProvider.DelayNoInterrupt = &LPC17_Time_DelayNoInterrupt;
    timeProvider.Delay = &LPC17_Time_Delay;

    timeApi.Author = "GHI Electronics, LLC";
    timeApi.Name = "GHIElectronics.TinyCLR.NativeApis.LPC17.TimeProvider";
    timeApi.Type = TinyCLR_Api_Type::TimeProvider;
    timeApi.Version = 0;
    timeApi.Count = 1;
    timeApi.Implementation = &timeProvider;

    return &timeApi;
}

static uint64_t g_nextEvent;   // tick time of next event to be scheduled

LPC17_Timer_Driver g_LPC17_Timer_Driver;

TinyCLR_Result LPC17_Time_GetInitialTime(const TinyCLR_Time_Provider* self, int64_t& utcTime, int32_t& timeZoneOffsetMinutes) {
    return TinyCLR_Result::NotSupported;
}

uint32_t LPC17_Time_GetSystemClock(const TinyCLR_Time_Provider* self) {
    return LPC17_SYSTEM_CLOCK_HZ;
}

uint32_t LPC17_Time_GetTicksPerSecond(const TinyCLR_Time_Provider* self) {
    return SLOW_CLOCKS_PER_SECOND;
}

uint32_t LPC17_Time_GetSystemCycleClock(const TinyCLR_Time_Provider* self) {
    return LPC17_AHB_CLOCK_HZ;
}

uint64_t LPC17_Time_GetTimeForProcessorTicks(const TinyCLR_Time_Provider* self, uint64_t ticks) {
    ticks *= (10000000 / SLOW_CLOCKS_TEN_MHZ_GCD);
    ticks /= (SLOW_CLOCKS_PER_SECOND / SLOW_CLOCKS_TEN_MHZ_GCD);

    return ticks;
}

uint64_t LPC17_Time_GetProcessorTicksForTime(const TinyCLR_Time_Provider* self, uint64_t time) {
    time /= 10;

#if 1000000 <= SLOW_CLOCKS_PER_SECOND
    return time * (SLOW_CLOCKS_PER_SECOND / 1000000);
#else
    return time / (1000000 / SLOW_CLOCKS_PER_SECOND);
#endif

}

uint64_t LPC17_Time_GetCurrentTicks(const TinyCLR_Time_Provider* self) {
    DISABLE_INTERRUPTS_SCOPED(irq);

    uint32_t tick_spent;
    uint32_t reg = SysTick->CTRL;
    uint32_t ticks = (SysTick->VAL & SysTick_LOAD_RELOAD_Msk);

    if ((reg & SysTick_CTRL_COUNTFLAG_Msk) == SysTick_CTRL_COUNTFLAG_Msk   // Interrupt was trigger on time as expected
        || ticks >= g_LPC17_Timer_Driver.m_currentTick) {                // Interrupt was trigger slower than expected
        if (ticks > 0) {
            tick_spent = g_LPC17_Timer_Driver.m_currentTick + (SysTick->LOAD - ticks);
        }
        else {
            tick_spent = g_LPC17_Timer_Driver.m_currentTick;
        }
    }
    else {
        tick_spent = g_LPC17_Timer_Driver.m_currentTick - ticks;
    }

    g_LPC17_Timer_Driver.m_currentTick = ticks;
    g_LPC17_Timer_Driver.m_lastRead += tick_spent;

    return (uint64_t)(g_LPC17_Timer_Driver.m_lastRead & TIMER_IDLE_VALUE);
}

TinyCLR_Result LPC17_Time_SetCompare(const TinyCLR_Time_Provider* self, uint64_t processorTicks) {
    uint64_t ticks;

    DISABLE_INTERRUPTS_SCOPED(irq);

    ticks = LPC17_Time_GetCurrentTicks(self);

    g_nextEvent = processorTicks;

    if (processorTicks == TIMER_IDLE_VALUE) {
        g_LPC17_Timer_Driver.m_periodTicks = SysTick_LOAD_RELOAD_Msk;
        g_LPC17_Timer_Driver.Reload(SysTick_LOAD_RELOAD_Msk);
    }
    else {
        if (ticks >= processorTicks) { // missed event
            g_LPC17_Timer_Driver.m_DequeuAndExecute();
        }
        else {
            g_LPC17_Timer_Driver.m_periodTicks = (processorTicks - ticks);

            if (g_LPC17_Timer_Driver.m_periodTicks >= SysTick_LOAD_RELOAD_Msk) {
                g_LPC17_Timer_Driver.Reload(SysTick_LOAD_RELOAD_Msk);
            }
            else {
                g_LPC17_Timer_Driver.Reload(g_LPC17_Timer_Driver.m_periodTicks);
            }
        }
    }

    return TinyCLR_Result::Success;
}

extern "C" {

    void SysTick_Handler(void *param) {
        INTERRUPT_STARTED_SCOPED(isr);

        if (LPC17_Time_GetCurrentTicks(nullptr) >= g_nextEvent) { // handle event
            g_LPC17_Timer_Driver.m_DequeuAndExecute();
        }
        else {
            LPC17_Time_SetCompare(nullptr, g_nextEvent);
        }
    }

}

TinyCLR_Result LPC17_Time_Acquire(const TinyCLR_Time_Provider* self) {
    g_nextEvent = TIMER_IDLE_VALUE;

    g_LPC17_Timer_Driver.Initialize();

    g_LPC17_Timer_Driver.m_periodTicks = SysTick_LOAD_RELOAD_Msk;

    g_LPC17_Timer_Driver.Reload(g_LPC17_Timer_Driver.m_periodTicks);
    return TinyCLR_Result::Success;
}

TinyCLR_Result LPC17_Time_Release(const TinyCLR_Time_Provider* self) {
    g_LPC17_Timer_Driver.UnInitialize();

    return TinyCLR_Result::Success;
}

TinyCLR_Result LPC17_Time_SetCompareCallback(const TinyCLR_Time_Provider* self, TinyCLR_Time_TickCallback callback) {
    if (g_LPC17_Timer_Driver.m_DequeuAndExecute != nullptr) return TinyCLR_Result::InvalidOperation;

    g_LPC17_Timer_Driver.m_DequeuAndExecute = callback;

    return TinyCLR_Result::Success;
}

void LPC17_Time_DelayNoInterrupt(const TinyCLR_Time_Provider* self, uint64_t microseconds) {
    DISABLE_INTERRUPTS_SCOPED(irq);

    uint64_t current = LPC17_Time_GetCurrentTicks(self);
    uint64_t maxDiff = LPC17_Time_GetProcessorTicksForTime(self, microseconds * 10);

    if (maxDiff <= CORTEXM_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS) maxDiff = 0;
    else maxDiff -= CORTEXM_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS;

    while (((int32_t)(LPC17_Time_GetCurrentTicks(self) - current)) <= maxDiff);

}

extern "C" void IDelayLoop(int32_t iterations);

void LPC17_Time_Delay(const TinyCLR_Time_Provider* self, uint64_t microseconds) {

    // iterations must be signed so that negative iterations will result in the minimum delay

    microseconds *= (LPC17_AHB_CLOCK_HZ / CLOCK_COMMON_FACTOR);
    microseconds /= (1000000 / CLOCK_COMMON_FACTOR);

    // iterations is equal to the number of CPU instruction cycles in the required time minus
    // overhead cycles required to call this subroutine.
    int32_t iterations = (int32_t)microseconds - 5;      // Subtract off call & calculation overhead
    IDelayLoop(iterations);
}

//******************** Profiler ********************

bool LPC17_Timer_Driver::Initialize() {
    g_LPC17_Timer_Driver.m_lastRead = 0;

    g_LPC17_Timer_Driver.m_currentTick = SysTick_LOAD_RELOAD_Msk;
    g_LPC17_Timer_Driver.m_periodTicks = SysTick_LOAD_RELOAD_Msk;

    SysTick_Config(g_LPC17_Timer_Driver.m_periodTicks);

    return true;
}
void LPC17_Timer_Driver::Reload(uint32_t value) {
    g_LPC17_Timer_Driver.m_currentTick = value;

    SysTick->LOAD = (uint32_t)(g_LPC17_Timer_Driver.m_currentTick - 1UL);
    SysTick->VAL = 0UL;
}

bool LPC17_Timer_Driver::UnInitialize() {
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;

    return true;
}
#ifdef __GNUC__
asm volatile (
    ".syntax unified\n\t"
    ".arch armv7-m\n\t"
    ".thumb\n\t"
    ".global  IDelayLoop\n\t"
    "@AREA ||i.IDelayLoop||, CODE, READONLY @ void IDelayLoop(UINT32 count)\n\t"
    ".section i.IDelayLoop, \"ax\", %progbits\n\t"
    ".thumb_func\n\t"
    "IDelayLoop:\n\t"
    "subs    r0, r0, #3          @@ 1 cycle\n\t"
    "bgt     IDelayLoop          @@ 3 cycles taken, 1 cycle not taken.\n\t"
    "bx lr                       @@ 3 cycles\n\t"
    );
#else
asm(
    "EXPORT  IDelayLoop\n\t"
    "AREA ||i.IDelayLoop||, CODE, READONLY ; void IDelayLoop(UINT32 count)\n\t"
    "IDelayLoop\n\t"
    "subs    r0, r0, #3          ;; 1 cycle\n\t"
    "bgt     IDelayLoop          ;; 3 cycles taken, 1 cycle not taken.\n\t"
    "bx      lr                  ;; 3 cycles\n\t"
);
#endif
