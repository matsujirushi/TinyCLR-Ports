#include <TinyCLR.h>
#include "Seeed_TinyCLR_WioLTE.h"

#include "Device.h"
#include "slre.901d42c/slre.h"
#include "slre.901d42c/slre.c"
#include <alloca.h>

////////////////////////////////////////////////////////////////////////////////
// from Targets/STM32F4/STM32F4_Time.cpp

extern "C" void IDelayLoop(int32_t iterations);

static void TimeDelay(uint64_t nanoseconds)
{
	int32_t iterations = (int32_t)(nanoseconds * STM32F4_AHB_CLOCK_HZ / 1000000000);
	IDelayLoop(iterations - 5);
}

////////////////////////////////////////////////////////////////////////////////
// from Targets/STM32F4/STM32F4_GPIO.cpp

// indexed port configuration access
#define Port(port) ((GPIO_TypeDef*) (GPIOA_BASE + (port << 10)))

////////////////////////////////////////////////////////////////////////////////
// Gpio

enum GPIO_DIRECTION_TYPE
{
	GPIO_DIRECTION_INPUT,
	GPIO_DIRECTION_OUTPUT,
};

static void GpioSetDirection(int32_t pin, GPIO_DIRECTION_TYPE direction)
{
	int32_t val;
	switch (direction)
	{
	case GPIO_DIRECTION_INPUT:
		val = 0b00;	// 入力モード
		break;
	case GPIO_DIRECTION_OUTPUT:
		val = 0b01;	// 汎用出力モード
		break;
	default:
		return;
	}

	GPIO_TypeDef* gpio = Port(pin >> 4);
	int32_t gpioNum = pin % 16;

	gpio->MODER = gpio->MODER & ~(0b11 << (gpioNum * 2)) | (val << (gpioNum * 2));
}

static void GpioSetOutput(int32_t pin, bool high)
{
	GPIO_TypeDef* gpio = Port(pin >> 4);
	int32_t gpioNum = pin % 16;

	if (high)
	{
		gpio->BSRR = 1 << gpioNum;			// set bit
	}
	else
	{
		gpio->BSRR = 1 << gpioNum << 16;	// reset bit
	}
}

////////////////////////////////////////////////////////////////////////////////
// RGB LED(SK6812)

static void SK6812Reset(int32_t pin)
{
	GpioSetOutput(pin, false);
	TimeDelay(80000);
}

static void SK6812SetBit(int32_t pin, bool on)
{
	if (!on)
	{
		GpioSetOutput(pin, true);
		TimeDelay(300);
		GpioSetOutput(pin, false);
		TimeDelay(900);
	}
	else
	{
		GpioSetOutput(pin, true);
		TimeDelay(600);
		GpioSetOutput(pin, false);
		TimeDelay(600);
	}
}

static void SK6812SetByte(int32_t pin, uint8_t val)
{
	for (int i = 0; i < 8; i++)
	{
		SK6812SetBit(pin, val & (1 << (7 - i)));
	}
}

static void SK6812SetSingleLED(int32_t pin, uint8_t r, uint8_t g, uint8_t b)
{
	SK6812SetByte(pin, g);
	SK6812SetByte(pin, r);
	SK6812SetByte(pin, b);
}

////////////////////////////////////////////////////////////////////////////////
// APIs

#define RGB_LED_PWR_PIN	(8)
#define RGB_LED_PIN		(17)

TinyCLR_Result Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::slre_match___STATIC___I4__STRING__STRING(const TinyCLR_Interop_MethodData md)
{
	auto ip = (const TinyCLR_Interop_Provider*)md.ApiProvider.FindDefault(&md.ApiProvider, TinyCLR_Api_Type::InteropProvider);
	TinyCLR_Interop_ClrValue regexp;
	TinyCLR_Interop_ClrValue buf;
	TinyCLR_Interop_ClrValue ret;
	ip->GetArgument(ip, md.Stack, 0, regexp);
	ip->GetArgument(ip, md.Stack, 1, buf);
	ip->GetReturn(ip, md.Stack, ret);

	ret.Data.Numeric->I4 = slre_match(regexp.Data.String.Data, buf.Data.String.Data, buf.Data.String.Length, NULL, 0, 0);

	return TinyCLR_Result::Success;
}

TinyCLR_Result Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::slre_match2___STATIC___STRING__STRING__STRING(const TinyCLR_Interop_MethodData md)
{
	auto ip = (const TinyCLR_Interop_Provider*)md.ApiProvider.FindDefault(&md.ApiProvider, TinyCLR_Api_Type::InteropProvider);
	TinyCLR_Interop_ClrValue regexp;
	TinyCLR_Interop_ClrValue buf;
	TinyCLR_Interop_ClrValue ret;
	ip->GetArgument(ip, md.Stack, 0, regexp);
	ip->GetArgument(ip, md.Stack, 1, buf);
	ip->GetReturn(ip, md.Stack, ret);

	struct slre_cap cap;
	cap.ptr = NULL;
	int index = slre_match(regexp.Data.String.Data, buf.Data.String.Data, buf.Data.String.Length, &cap, 1, 0);
	if (index == SLRE_NO_MATCH) return TinyCLR_Result::Success;
	if (index < 0) return TinyCLR_Result::NotSupported;

	if (cap.ptr == NULL)
	{
		TinyCLR_Interop_ClrValue clrStr;
		ip->CreateString(ip, NULL, 0, clrStr);
		ip->AssignObjectReference(ip, ret, clrStr.Object);
	}
	else
	{
		TinyCLR_Interop_ClrValue clrStr;
		ip->CreateString(ip, cap.ptr, cap.len, clrStr);
		ip->AssignObjectReference(ip, ret, clrStr.Object);
	}
}

TinyCLR_Result Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::Init___VOID(const TinyCLR_Interop_MethodData md)
{
	// RGB_LED_PWR_PIN
	GpioSetDirection(RGB_LED_PWR_PIN, GPIO_DIRECTION_OUTPUT);
	GpioSetOutput(RGB_LED_PWR_PIN, true);

	// RGB_LED_PIN
	GpioSetDirection(RGB_LED_PIN, GPIO_DIRECTION_OUTPUT);
	GpioSetOutput(RGB_LED_PIN, false);

	return TinyCLR_Result::Success;
}

TinyCLR_Result Interop_Seeed_TinyCLR_WioLTE_Seeed_TinyCLR_WioLTE_WioLTENative::LedSetRGB___VOID__U1__U1__U1(const TinyCLR_Interop_MethodData md)
{
	auto ip = (const TinyCLR_Interop_Provider*)md.ApiProvider.FindDefault(&md.ApiProvider, TinyCLR_Api_Type::InteropProvider);
	TinyCLR_Interop_ClrValue r;
	TinyCLR_Interop_ClrValue g;
	TinyCLR_Interop_ClrValue b;
	ip->GetArgument(ip, md.Stack, 1, r);
	ip->GetArgument(ip, md.Stack, 2, g);
	ip->GetArgument(ip, md.Stack, 3, b);

	SK6812Reset(RGB_LED_PIN);
	SK6812SetSingleLED(RGB_LED_PIN, r.Data.Numeric->U1, g.Data.Numeric->U1, b.Data.Numeric->U1);

	return TinyCLR_Result::Success;
}
