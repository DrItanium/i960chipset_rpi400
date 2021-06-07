#include <cstdint>
#include <iostream>
#include <memory>
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
	PinHolder() { digitalWrite(pin, assertState); }
	~PinHolder() { digitalWrite(pin, deassertState); }
};
template<typename ... Pins>
void pinModeBlock(decltype(OUTPUT) direction, Pins&& ... pins) {
	(pinMode(pins, direction), ...);
}

void setup() {
	pinModeBlock(OUTPUT, 
		     LevelShifterHatEnable,
		     RESET960Pin,
		     READYPin,
		     INT0Pin,
		     GPIOCSPin);
	pinModeBlock(INPUT,
		     DENPin,
		     ASPin,
		     WRPin,
		     BLASTPin,
		     FAILPin);
	pinMode(RESET960Pin, OUTPUT);
	pinMode(LevelShifterHatEnable, OUTPUT);
	PinHolder<RESET960Pin> holdi960InReset;
	digitalWrite(LevelShifterHatEnable, HIGH);
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

