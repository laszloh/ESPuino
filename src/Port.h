#pragma once

#include <Arduino.h>

#include <Wire.h>

// Flag definitions taken from esphome (https://github.com/esphome/esphome)
namespace gpio {

enum Flags : uint8_t {
	// Can't name these just INPUT because of Arduino defines :(
	FLAG_NONE = 0x00,
	FLAG_INPUT = 0x01,
	FLAG_OUTPUT = 0x02,
	FLAG_OPEN_DRAIN = 0x04,
	FLAG_PULLUP = 0x08,
	FLAG_PULLDOWN = 0x10,
};

class FlagsHelper {
public:
	constexpr FlagsHelper(Flags val)
		: val_(val) { }
	constexpr operator Flags() const { return val_; }

protected:
	Flags val_;
};
constexpr FlagsHelper operator&(Flags lhs, Flags rhs) {
	return static_cast<Flags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr FlagsHelper operator|(Flags lhs, Flags rhs) {
	return static_cast<Flags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

enum InterruptType : uint8_t {
	INTERRUPT_RISING_EDGE = 1,
	INTERRUPT_FALLING_EDGE = 2,
	INTERRUPT_ANY_EDGE = 3,
	INTERRUPT_LOW_LEVEL = 4,
	INTERRUPT_HIGH_LEVEL = 5,
};

} // namespace gpio

class GpioDriver {
public:
	GpioDriver() = default;
	virtual ~GpioDriver() = default;

	virtual void init() { }
	virtual void exit() { }
	virtual void cyclic() { }

	virtual uint8_t getPin(size_t gpio) { return gpio; }
	virtual void pinMode(uint8_t pin, gpio::Flags mode) = 0;
	virtual bool digitalRead(uint8_t pin) = 0;
	virtual void digitalWrite(uint8_t pin, bool level) = 0;

	template <typename T>
	void attachInterruptArg(uint8_t pin, void (*userFunc)(T *), T *arg, gpio::InterruptType mode) {
		this->attachInterruptArg(pin, reinterpret_cast<void (*)(void *)>(userFunc), arg, mode);
	}

protected:
	virtual void attachInterruptArg(uint8_t pin, void (*userFunc)(void *), void *arg, gpio::InterruptType mode) { }
};

class InternalDriver : public GpioDriver {
public:
	static constexpr bool canHandle(size_t gpio) {
		return (gpio < GPIO_PIN_COUNT) && GPIO_IS_VALID_GPIO(gpio);
	}

	virtual void pinMode(uint8_t pin, gpio::Flags mode) override;
	virtual bool digitalRead(uint8_t pin) override;
	virtual void digitalWrite(uint8_t pin, bool level) override;

protected:
	virtual void attachInterruptArg(uint8_t pin, void (*userFunc)(void *), void *arg, gpio::InterruptType mode) override;
};

class PCA9555Driver : public GpioDriver {
public:
	static constexpr bool canHandle(size_t gpio) {
		return (gpio >= expanderBaseAddress) && (gpio < expanderBaseAddress + 15);
	}

	virtual void init() override;
	virtual void exit() override;
	virtual void cyclic() override;

	virtual uint8_t getPin(size_t gpio) { return gpio - expanderBaseAddress; }

	virtual void pinMode(uint8_t pin, gpio::Flags mode) override;
	virtual bool digitalRead(uint8_t pin) override;
	virtual void digitalWrite(uint8_t pin, bool level) override;

protected:
	static constexpr bool intPinEnabled = (PE_INTERRUPT_PIN < GPIO_PIN_COUNT) && GPIO_IS_VALID_GPIO(PE_INTERRUPT_PIN);
	static constexpr uint16_t defaultValue[] = {
		0x0000, // Input port registers
		0xFFFF, // Output port registers
		0x0000, // Polarity Inversion registers
		0xFFFF, // Configuration registers
	};
	enum RegisterMap {
		InputRegister = 0,
		OutputRegister = 2,
		InvertRegister = 4,
		ConfigRegister = 6,
	};
	struct InterruptHandle {
		std::function<void(void *)> fn;
		void *arg;
		gpio::InterruptType mode;
	};

	TwoWire &i2cBus {expanderI2cWire};
	bool allowReadFromPortExpander {false};
	bool found {false};
	uint16_t verifiedInputRegister {0};
	InterruptHandle interruptHandlers[16];

	virtual void attachInterruptArg(uint8_t pin, void (*userFunc)(void *), void *arg, gpio::InterruptType mode) override;
	static void IRAM_ATTR portExpanderISR(PCA9555Driver *driver);
	void findExpander();
	void resetExpander();

	bool writeRegister(uint8_t reg, uint8_t value) {
		i2cBus.beginTransmission(expanderI2cAddress);
		i2cBus.write(reg);
		i2cBus.write(value);
		return !i2cBus.endTransmission();
	}

	bool readRegister(uint8_t reg, uint8_t &data) {
		i2cBus.beginTransmission(expanderI2cAddress);
		i2cBus.write(reg);
		if (i2cBus.endTransmission(false) != 0) {
			return false;
		}
		bool noError = !i2cBus.requestFrom(expanderI2cAddress, sizeof(data));
		if (noError) {
			data = i2cBus.read();
		}
		return noError;
	}

	bool writeRegister(uint8_t reg, uint16_t value) {
		i2cBus.beginTransmission(expanderI2cAddress);
		i2cBus.write(reg);
		i2cBus.write(value);
		return !i2cBus.endTransmission();
	}

	bool readRegister(uint8_t reg, uint16_t &data) {
		i2cBus.beginTransmission(expanderI2cAddress);
		i2cBus.write(reg);
		if (i2cBus.endTransmission(false) != 0) {
			return false;
		}
		bool noError = !i2cBus.requestFrom(expanderI2cAddress, sizeof(data));
		if (noError) {
			i2cBus.readBytes((uint8_t *) &data, sizeof(data));
		}
		return noError;
	}
};

class Gpio {
public:
	Gpio(uint8_t pin, GpioDriver *driver)
		: driver(driver)
		, pin(pin) { }
	Gpio() = default;

	bool isValid() const { return driver != nullptr; }
	explicit operator bool() { return isValid(); }

	uint8_t getPin() const { return pin; }

	void setInverted(bool inverted) { this->inverted = inverted; }
	bool isInverted() const { return inverted; }

	void pinMode(gpio::Flags mode) {
		if (!isValid()) {
			return;
		}
		driver->pinMode(pin, mode);
	}

	bool digitalRead() {
		if (!isValid()) {
			return false;
		}
		return driver->digitalRead(pin) != inverted;
	}
	void digitalWrite(bool level) {
		if (!isValid()) {
			return;
		}
		return driver->digitalWrite(pin, level != inverted);
	}

	template <typename T>
	void attachInterruptArg(void (*userFunc)(T *), T *arg, gpio::InterruptType mode) {
		if (!isValid()) {
			return;
		}
		if (inverted) {
			switch (mode) {
				case gpio::INTERRUPT_RISING_EDGE:
					mode = gpio::INTERRUPT_FALLING_EDGE;
					break;

				case gpio::INTERRUPT_FALLING_EDGE:
					mode = gpio::INTERRUPT_RISING_EDGE;
					break;

				case gpio::INTERRUPT_LOW_LEVEL:
					mode = gpio::INTERRUPT_HIGH_LEVEL;
					break;

				case gpio::INTERRUPT_HIGH_LEVEL:
					mode = gpio::INTERRUPT_LOW_LEVEL;
					break;

				default:
					break;
			}
		}
		driver->attachInterruptArg(pin, userFunc, arg, mode);
	}

protected:
	GpioDriver *driver {nullptr};
	uint8_t pin {99};

	bool inverted {false};
};

class GpioDriverFactory {
public:
	static constexpr Gpio getGpio(size_t gpioPin) {

		if (gpioPin == GPIO_PIN_UNUSED) {
			return Gpio();
		}

		if (InternalDriver::canHandle(gpioPin)) {
			InternalDriver &driver = GpioDriverFactory::internal();
			return Gpio(driver.getPin(gpioPin), &driver);
		}

		if (PCA9555Driver::canHandle(gpioPin)) {
			PCA9555Driver &driver = GpioDriverFactory::pca9555();
			return Gpio(driver.getPin(gpioPin), &driver);
		}

		// nobody could handle the pin
		return Gpio();
	}

	static inline void init() {
		internal().init();
		pca9555().init();
	}

	static inline void cyclic() {
		internal().cyclic();
		pca9555().cyclic();
	}

	static inline void exit() {
		internal().exit();
		pca9555().exit();
	}

protected:
	static InternalDriver &internal() {
		static InternalDriver internalDriver;
		return internalDriver;
	}
	static PCA9555Driver &pca9555() {
		static PCA9555Driver pca9555Driver;
		return pca9555Driver;
	}

	GpioDriverFactory() = default;
	~GpioDriverFactory() = default;
};
