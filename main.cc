#include <iostream>
#include <wiringPi.h>
constexpr auto LevelShifterHatEnable = 25;
int main() {
	wiringPiSetup();
	pinMode(LevelShifterHatEnable, OUTPUT);
	digitalWrite(LevelShifterHatEnable, HIGH);
	std::cout << "chipset!" << std::endl;
	for (int i = 0; i < 10; ++i) {
		digitalWrite(LevelShifterHatEnable, LOW);
		delay(1000);
		digitalWrite(LevelShifterHatEnable, HIGH);
		delay(1000);
	}
	digitalWrite(LevelShifterHatEnable, LOW);
	return 0;
}

