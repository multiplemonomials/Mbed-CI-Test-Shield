# Mbed CI Test Shield
![Mbed CI Test Shield pcb](https://app.box.com/shared/static/bijiwmo5xfw0mz87y4zxysnwlab9mixu.png)

This project is an updated version of the [Mbed CI Test shield](https://github.com/ARMmbed/ci-test-shield), a project created by the ARM Mbed team several years ago.  Essentially, the CI Test Shield is a single board that can use and exercise as many different hardware interfaces of an MCU as possible.  When combined with appropriate test cases and scripts, it should allow a CI job to exercise and test nearly all major functionality of the Mbed board.

The goal of this project is to strike a balance between simplicity and functionality -- it should be cheap and easy to make so that CI infrastructure can be set up for as many Mbed targets as possible, but it should also test as many features of the MCU as possible within those constraints.  For these reasons, the board includes the following circuits:
- MicroSD card, which acts as an SPI slave
- 24FC02-I/SN I²C EEPROM, which acts as a regular and Fast Mode Plus I²C slave
- Analog filter on the PWM output, which allows creating an analog voltage using PWM and feeding it into the ADC
- Loopback from three digital inputs to three digital outputs, which helps test digital IO and interrupts
- a Bus Pirate, which is an affordable small logic analyzer.  This can measure voltages and frequencies, spy on I²C and I²C transactions, and act as an I2C and SPI master

The board is designed in the ubiquitous (if not very well standardized) Arduino Uno form factor, so it should be able to be directly attached to many dev boards that implement this form factor.  For other dev boards, jumper wires will have to be used to connect the shield to the Mbed board.

## Test Cases
The hardware is designed to support the following test cases.  Note that as of 4-30-2022, no code has been written for these yet, but I believe that a lot of code should be able to be reused from the original project.

|Major Mbed System|Variants|Signals used in test|Description of test|
|---|---|---|---|
|I2C Master| **Speed:** Standard mode (100kHz), Fast mode (400kHz), or Fast mode plus - (1MHz) if device supports it.  **API type:** Single byte vs multibyte.  Note that the current I2C EEPROM driver uses both types.|I2C_SCL, I2C_SDA, ~I2C_EN, ~I2C_EN_FMP|The test will send and receive bytes from the connected 24FC02 I2C EEPROM, and make sure that data can be written correctly.  The Bus Pirate can also log all transactions during the test.|
|I2C Slave|**Speed:** Standard mode (100kHz) or Fast mode (400kHz)|I2C_SCL, I2C_SDA|The Bus Pirate acts as an I2C master and attempts to read and write bytes from the Mbed device.|
|AnalogOut (DAC)||DAC_OUT|The Mbed device writes a series of expected voltages (or maybe a waveform) out of its DAC, and the Bus Pirate shall confirm that they are received correctly.|
|SPI Master|**Speed:** Different frequencies, likely from 1MHz to 20MHz. **API type:** Single byte vs multibyte. |SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS|The test will use SPI to read and write bytes from the SD card, and confirm that the correct bytes were received.  The Bus Pirate can also log all transactions during the test.|
|SPI Slave|**Speed:** Different frequencies, likely from 1MHz to 20MHz.|SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS|The Bus Pirate acts as an SPI master and attempts to read and write bytes from the Mbed device.|
|PWM Out|**PWM Frequency:** From the kHz to the MHz range, if supported.|PWM_OUT|To test PWM frequency, the PWM signal is fed into the Bus Pirate's frequency input pin.  To test duty cycle, the PWM signal is fed into a low pass filter that converts it into a voltage.  The voltage can then be read by the bus pirate to measure the duty cycle.|
|UART input and output|||Tested passively by the Greentea test runner working.|
|InterruptIn||BUSOUT_0, BUSIN_0|Tested via loopback of IO signals - should be able to create an interrupt on BUSIN_0 from an edge on BUSOUT_0|
|DigitalOut and DigitalIn||BUSOUT_0, BUSIN_0|Tested via loopback of IO signals from the output back to the input.|
|BusOut and BusIn||BUSOUT_[0:2], BUSIN_[0:2]|Tested via loopback of IO signals from the output back to the input.|
|AnalogIn (ADC)||PWM_OUT, ANALOG_IN|PWM from the processor will be used to generate a known analog voltage on an analog input.  The analog input can then be read to see if it matches the expected voltage.|