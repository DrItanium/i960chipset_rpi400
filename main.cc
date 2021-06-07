#include <cstdint>
#include <iostream>
#include <memory>
#include <tuple>
#include <cerrno>
#include <cstring>

#include <wiringPi.h>
//#include <wiringPiSPI.h>
#include <mcp23s17.h>
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
constexpr auto SPI_CHAN = 0;
constexpr auto BA1Pin = 148;
constexpr auto BA2Pin = 149;
constexpr auto BA3Pin = 150;
constexpr auto BE0Pin = 151;
constexpr auto BE1Pin = 152;
constexpr auto HOLDPin = 153;
constexpr auto HLDAPin = 154;
constexpr auto LOCKPin = 155;
constexpr auto MCP23S17_SPEED = 10000000;
constexpr auto Megabytes = 1024 * 1024;
int spiFD = 0;
bool asEnabled = false;
bool denEnabled = false;
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
void
markReady() {
	pinMode(READYPin, LOW);
	pinMode(READYPin, HIGH);
}
void 
setDataLinesDirection(decltype(OUTPUT) direction) {
	/// @todo replace these calls with direct reads and writes
	for (int i = 100; i < 116; ++i) {
		pinMode(i, direction);
	}
}
uint16_t 
getDataValue() {
	uint16_t value = 0;
	for (int i = 100, j = 0; i < 116; ++i, ++j) {
		if (digitalRead(i) != LOW) {
			value |= static_cast<uint16_t>(1 << j);
		}
	}	
	return value;
}
void
setDataValue(uint16_t value) {
	for (int i = 100, j = 0; i < 116; ++i, ++j) {
		if ((value & static_cast<uint16_t>(i << j))) {
			digitalWrite(i, HIGH);
		} else {
			digitalWrite(i, LOW);
		}
	}
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
	digitalWrite(RESET960Pin, LOW);
	digitalWriteBlock(PinDirectionDescription {LevelShifterHatEnable, HIGH},
			PinDirectionDescription {INT0Pin, HIGH},
			PinDirectionDescription {READYPin, HIGH},
			PinDirectionDescription {GPIOCSPin, HIGH});
	wiringPiISR(ASPin, INT_EDGE_FALLING, []() { asEnabled = true; });
	wiringPiISR(DENPin, INT_EDGE_FALLING, []() { denEnabled = true; });
	// we have 4 io expanders to talk to, the speed of the spi device is marked is set to 4MHz, we want to increase this if we directly access the 
	mcp23s17Setup(100, SPI_CHAN, 0);
	mcp23s17Setup(116, SPI_CHAN, 1);
	mcp23s17Setup(132, SPI_CHAN, 2);
	mcp23s17Setup(148, SPI_CHAN, 3);
	for (int i = 0; i < 16; ++i) {
		pinMode(100 + i, INPUT);
		pinMode(116 + i, INPUT);
		pinMode(132 + i, INPUT);
		pinMode(148 + i, INPUT);
	}
	pinMode(HOLDPin, OUTPUT);
	pinMode(LOCKPin, OUTPUT);
	digitalWrite(HOLDPin, LOW);
	digitalWrite(LOCKPin, HIGH);
	for (int i = 156; i < (148 + 16); ++i) {
		pinMode(i, OUTPUT);
		digitalWrite(i, LOW);
	}
	delay(1000);
	digitalWrite(RESET960Pin, HIGH);
	std::cout << "waiting for fail pin" << std::endl;
	while (digitalRead(FAILPin) == LOW); // wait for self test to start
	std::cout << "running self test" << std::endl;
	while (digitalRead(FAILPin) == HIGH); // wait for self test to finish
}

[[noreturn]]
void handleChecksumFailure() {
	std::cerr << "Processor Checksum Failure" << std::endl;
	while (true) { }
}
// Idle -> Idle : no request
// Recovery -> Idle : no request
// Recovery -> Address : Request pending
// Idle -> Address : New Request
// Address -> Data : ToDataState
// Data -> Recovery : After READY and no burst (blast low)
// Data -> Data : After READY and burst (blast high)
// Idle -> ChecksumFailure if fail is asserted
// Recovery -> ChecksumFailure if fail is asserted
// 
// Address: Entry: clear address state triggered
// Data: burst is sampled
// Data: Entry: den is cleared
enum class ProcessorChipsetState {
	Idle,
	Address,
	Data,
	Recovery,
	Wait, // unused
	ChecksumFailure,
};
ProcessorChipsetState currentState = ProcessorChipsetState::Idle;
bool captureAddress = false;
bool currentlyReading = false;
uint32_t baseAddress = 0;
uint32_t choppedBits = 0;
uint8_t
getBurstAddressBits() {
	uint8_t bits = 0;
	bits |= digitalRead(BA1Pin) == LOW ? 0 : 0b0010;
	bits |= digitalRead(BA2Pin) == LOW ? 0 : 0b0100;
	bits |= digitalRead(BA3Pin) == LOW ? 0 : 0b1000;
	return bits;
}

void 
toAddressState()
{
	asEnabled = false;
	captureAddress = true;	
	baseAddress = 0;
	currentState = ProcessorChipsetState::Address;		
}
void 
handleChipsetIdle() {
	if (digitalRead(FAILPin) == HIGH) {
		currentState = ProcessorChipsetState::ChecksumFailure;
	} else {
		if (asEnabled) {
			toAddressState();
		}
	}
}
void 
handleChipsetAddress() {
	if (captureAddress) {
		captureAddress = false;
		baseAddress = 0;	
		for (int i = 116, j = 0; i < 148; ++i, ++j) {
			if (digitalRead(i) != LOW) {
				baseAddress |= (1 << j);
			}
		}	
		choppedBits = baseAddress & (~0b1111);
	}
	if (denEnabled) {
		denEnabled = false;
		currentlyReading = digitalRead(WRPin) == LOW;
		currentState = ProcessorChipsetState::Data;
		setDataLinesDirection(currentlyReading ? OUTPUT : INPUT);
	}
}
void
handleChipsetData() {
	auto burstBits = getBurstAddressBits();
	auto address = choppedBits | burstBits;
	if (currentlyReading) {
		std::cout << "Read from 0x" << std::hex << address << std::endl;
		setDataValue(0);
	} else {
		// stub for now
		auto value = getDataValue();
		std::cout << "Write 0x" << std::hex << value << " to 0x" << address << std::endl;
	}
	auto blastAsserted = digitalRead(BLASTPin) == LOW;
	digitalWrite(READYPin, LOW);
	digitalWrite(READYPin, HIGH);
	if (blastAsserted) {
		currentState = ProcessorChipsetState::Recovery;
	}
}
void
handleChipsetRecovery() {
	if (digitalRead(FAILPin) == HIGH) {
		currentState = ProcessorChipsetState::ChecksumFailure;
	} else {
		if (asEnabled) {
			toAddressState();
		} else {
			currentState = ProcessorChipsetState::Idle;
		}
	}
}

int main() {
	ram = std::make_unique<MemoryCell[]>((1024 * Megabytes) / sizeof(MemoryCell));
	wiringPiSetup();
	setup();
	// do the initial startup state
	// enter into the idle state	
	while (true) {
		switch (currentState) {
			case ProcessorChipsetState::Idle:
				handleChipsetIdle();
				break;
			case ProcessorChipsetState::Address:
				handleChipsetAddress();
				break;
			case ProcessorChipsetState::Data:
				handleChipsetData();
				break;
			case ProcessorChipsetState::Recovery:
				handleChipsetRecovery();
				break;
			case ProcessorChipsetState::ChecksumFailure:
				handleChecksumFailure();
				break;
			default:
				break;
		}
	}
	digitalWrite(LevelShifterHatEnable, LOW);
	return 0;
}

