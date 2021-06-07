#include <cstdint>
#include <iostream>
#include <memory>
#include <tuple>

#include <wiringPi.h>
constexpr auto DENPin = 1;
constexpr auto ASPin = 4;
constexpr auto INT0Pin = 5;
constexpr auto WRPin = 6;
constexpr auto RESET960Pin = 26;
constexpr auto LevelShifterHatEnable = 25;
constexpr auto READYPin = 27;
constexpr auto BLASTPin = 28;
constexpr auto FAILPin = 29;
constexpr auto GPIOCSPin = 10;
union MemoryCell {
	uint16_t value = 0;
	uint8_t bytes[2];
};
std::unique_ptr<MemoryCell[]> ram;
template<auto pin, auto assertState = LOW, auto deassertState = HIGH>
struct PinHolder final {
	PinHolder() noexcept { digitalWrite(pin, assertState); }
	~PinHolder() noexcept { digitalWrite(pin, deassertState); }
};
using PinConfigurationDescription = std::tuple<decltype(DENPin), decltype(OUTPUT)>;
void pinMode(PinConfigurationDescription desc) noexcept {
	pinMode(std::get<0>(desc), std::get<1>(desc));
}
template<typename ... Pins>
void pinModeBlock(Pins&& ... pins) noexcept {
	(pinMode(pins), ...);
}

using PinDirectionDescription = std::tuple<decltype(GPIOCSPin), decltype(LOW)>;

void digitalWrite(PinDirectionDescription description) noexcept {
	digitalWrite(std::get<0>(description), 
		     std::get<1>(description));
}

template<typename ... D>
void digitalWriteBlock(D&& ... args) noexcept {
	(digitalWrite(args), ...);
}

void setup() {
	pinModeBlock( PinConfigurationDescription { LevelShifterHatEnable, OUTPUT },
		      PinConfigurationDescription { RESET960Pin, OUTPUT },
		      PinConfigurationDescription { READYPin, OUTPUT },
		      PinConfigurationDescription { INT0Pin, OUTPUT },
		      PinConfigurationDescription { GPIOCSPin, OUTPUT },
		      PinConfigurationDescription { DENPin, INPUT },
		      PinConfigurationDescription { ASPin, INPUT },
		      PinConfigurationDescription { WRPin, INPUT },
		      PinConfigurationDescription { BLASTPin, INPUT },
		      PinConfigurationDescription { FAILPin, INPUT });
	PinHolder<RESET960Pin> holdi960InReset;
	digitalWriteBlock(PinDirectionDescription {LevelShifterHatEnable, HIGH},
			  PinDirectionDescription {INT0Pin, HIGH},
			  PinDirectionDescription {READYPin, HIGH},
			  PinDirectionDescription {GPIOCSPin, HIGH});
	std::cout << "chipset!" << std::endl;
}
int main() {
	ram = std::make_unique<MemoryCell[]>((512 * 1024 * 1024) / sizeof(MemoryCell));
	wiringPiSetup();
	setup();
	while (true) {
	}
	digitalWrite(LevelShifterHatEnable, LOW);
	return 0;
}

