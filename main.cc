#include <iostream>
#include <wiringPi.h>
constexpr auto EnablePin = 25;
int main() {
	wiringPiSetup();
	pinMode(EnablePin, OUTPUT);
	digitalWrite(EnablePin, HIGH);
	std::cout << "chipset!" << std::endl;
	for (int i = 0; i < 10; ++i) {
		digitalWrite(EnablePin, LOW);
		delay(1000);
		digitalWrite(EnablePin, HIGH);
		delay(1000);
	}
	digitalWrite(EnablePin, LOW);
	return 0;
}

