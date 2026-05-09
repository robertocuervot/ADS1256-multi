/*
 ============================================================================
 ADS1256 Multi-ADC Library Header
 ============================================================================
 
 Name:		ADS1256.h
 Created:	2022/07/14 (Original by Curious Scientist)
 Modified:	2026 (Multi-ADC support added)
 
 Author:	Curious Scientist (https://github.com/CuriousScientist0/ADS1256)
 
 Description:
   Arduino-compatible library for the ADS1256 24-bit, 8-channel analog-to-digital
   converter. This fork has been modified to support simultaneous operation of
   multiple ADS1256 units on a single microcontroller.
   
 Key Modifications for Multi-ADC Support:
   1. Exposed internal variables (_spi, _outputBuffer, _outputValue, 
      _isAcquisitionRunning, _cycle) to allow external management of 
      multiple ADC instances.
   2. Constructor now accepts optional SPIClass* parameter for flexible 
      SPI interface management.
   3. Variables remain public to enable synchronized parallel reading patterns.
   
 Usage:
   See README.md for comprehensive documentation and examples.
   Original library documentation: 
   https://github.com/CuriousScientist0/ADS1256/blob/main/extras/ADS1256_ArduinoLibrary_Documentation_20251023.pdf
 
 Credits:
   Curious Scientist - Original implementation and excellent design
   Abraão Queiroz - ESP32 compatibility fixes
   Benjamin Pelletier - DRDY signal handling improvements
   RadoMmm - ADC-to-Volts conversion optimization
 ============================================================================
*/

#ifndef _ADS1256_h
#define _ADS1256_h

#include <SPI.h>

//Differential inputs
#define DIFF_0_1 0b00000001 //A0 + A1 as differential input
#define DIFF_2_3 0b00100011 //A2 + A3 as differential input
#define DIFF_4_5 0b01000101 //A4 + A5 as differential input
#define DIFF_6_7 0b01100111 //A6 + A7 as differential input

//Single-ended inputs
#define SING_0 0b00001111 //A0 + GND (common) as single-ended input
#define SING_1 0b00011111 //A1 + GND (common) as single-ended input
#define SING_2 0b00101111 //A2 + GND (common) as single-ended input
#define SING_3 0b00111111 //A3 + GND (common) as single-ended input
#define SING_4 0b01001111 //A4 + GND (common) as single-ended input
#define SING_5 0b01011111 //A5 + GND (common) as single-ended input
#define SING_6 0b01101111 //A6 + GND (common) as single-ended input
#define SING_7 0b01111111 //A7 + GND (common) as single-ended input

//PGA settings			  //Input voltage range
#define PGA_1 0b00000000  //± 5 V
#define PGA_2 0b00000001  //± 2.5 V
#define PGA_4 0b00000010  //± 1.25 V
#define PGA_8 0b00000011  //± 625 mV
#define PGA_16 0b00000100 //± 312.5 mV
#define PGA_32 0b00000101 //+ 156.25 mV
#define PGA_64 0b00000110 //± 78.125 mV

//Datarate						  //DEC
#define DRATE_30000SPS 0b11110000 //240
#define DRATE_15000SPS 0b11100000 //224
#define DRATE_7500SPS 0b11010000  //208
#define DRATE_3750SPS 0b11000000  //192
#define DRATE_2000SPS 0b10110000  //176
#define DRATE_1000SPS 0b10100001  //161
#define DRATE_500SPS 0b10010010   //146
#define DRATE_100SPS 0b10000010   //130
#define DRATE_60SPS 0b01110010    //114
#define DRATE_50SPS 0b01100011    //99
#define DRATE_30SPS 0b01010011    //83
#define DRATE_25SPS 0b01000011    //67
#define DRATE_15SPS 0b00110011    //51
#define DRATE_10SPS 0b00100011    //35
#define DRATE_5SPS 0b00010011     //19
#define DRATE_2SPS 0b00000011     //3

//Status register
#define BITORDER_MSB 0
#define BITORDER_LSB 1
#define ACAL_DISABLED 0
#define ACAL_ENABLED 1
#define BUFFER_DISABLED 0
#define BUFFER_ENABLED 1

//Register addresses
#define STATUS_REG 0x00
#define MUX_REG 0x01
#define ADCON_REG 0x02
#define DRATE_REG 0x03
#define IO_REG 0x04
#define OFC0_REG 0x05
#define OFC1_REG 0x06
#define OFC2_REG 0x07
#define FSC0_REG 0x08
#define FSC1_REG 0x09
#define FSC2_REG 0x0A

//Command definitions
#define WAKEUP 0b00000000
#define RDATA 0b00000001
#define RDATAC 0b00000011
#define SDATAC 0b00001111
#define RREG 0b00010000
#define WREG 0b01010000
#define SELFCAL 0b11110000
#define SELFOCAL 0b11110001
#define SELFGCAL 0b11110010
#define SYSOCAL 0b11110011
#define SYSGCAL 0b11110100
#define SYNC 0b11111100
#define STANDBY 0b11111101
#define RESET 0b11111110
//----------------------------------------------------------------


class ADS1256
{	
public:
	/// @brief Special constant indicating a pin is not used
	static constexpr int8_t PIN_UNUSED = -1;

	/// @brief Constructor - Initialize ADS1256 instance
	/// @param DRDY_pin GPIO pin connected to DRDY signal (Data Ready, active LOW)
	/// @param RESET_pin GPIO pin connected to RESET signal (active LOW)
	/// @param SYNC_pin GPIO pin connected to SYNC signal (optional, use PIN_UNUSED if not used)
	/// @param CS_pin GPIO pin connected to CS signal (Chip Select, active LOW)
	/// @param VREF Reference voltage in volts (typically 2.5 or 5.0)
	/// @param spi Pointer to SPI interface (defaults to &SPI). IMPORTANT: For multiple ADCs,
	///            pass the same &SPI pointer to all instances for proper SPI arbitration.
	ADS1256(const int8_t DRDY_pin, const int8_t RESET_pin, const int8_t SYNC_pin, 
	        const int8_t CS_pin, float VREF, SPIClass* spi = &SPI);
	
	//================================================================
	// INITIALIZATION AND CONFIGURATION
	//================================================================
	
	/// @brief Initialize the ADS1256 ADC
	/// @details Performs hardware reset, SPI setup, and applies default configuration
	void InitializeADC();	
	
	/// @brief Read value from an ADS1256 register
	/// @param registerAddress Register address (0x00-0x0A)
	/// @return Register value as long integer
	long readRegister(uint8_t registerAddress);
	
	/// @brief Write value to an ADS1256 register
	/// @param registerAddress Register address (0x00-0x0A)
	/// @param registerValueToWrite Value to write to register
	void writeRegister(uint8_t registerAddress, uint8_t registerValueToWrite);	

	//================================================================
	// SAMPLING AND MEASUREMENT CONTROL
	//================================================================
	
	/// @brief Set data rate (sampling rate)
	/// @param drate Use DRATE_* constants (e.g., DRATE_100SPS)
	void setDRATE(uint8_t drate);
	
	/// @brief Set Programmable Gain Amplifier (PGA)
	/// @param pga Use PGA_* constants (e.g., PGA_64 for ±78.125mV range)
	void setPGA(uint8_t pga);
	
	/// @brief Get current PGA setting
	/// @return PGA value (0-6, corresponds to gain 1-64)
	uint8_t getPGA();
	
	/// @brief Set multiplexer to select input channel(s)
	/// @param mux Use DIFF_* or SING_* constants
	void setMUX(uint8_t mux);
	
	/// @brief Set output data bit order
	/// @param byteOrder BITORDER_MSB or BITORDER_LSB
	void setByteOrder(uint8_t byteOrder);
	
	/// @brief Get current byte order setting
	uint8_t getByteOrder();
	
	/// @brief Enable/disable input buffer
	/// @param bufen BUFFER_ENABLED or BUFFER_DISABLED
	void setBuffer(uint8_t bufen);
	
	/// @brief Get current buffer setting
	uint8_t getBuffer();
	
	/// @brief Set auto-calibration
	/// @param acal ACAL_ENABLED or ACAL_DISABLED
	void setAutoCal(uint8_t acal);
	
	/// @brief Get auto-calibration setting
	uint8_t getAutoCal();
	
	//================================================================
	// GPIO CONTROL (optional feature)
	//================================================================
	
	/// @brief Configure GPIO pins direction
	void setGPIO(uint8_t dir0, uint8_t dir1, uint8_t dir2, uint8_t dir3);
	
	/// @brief Write GPIO values
	void writeGPIO(uint8_t dir0value, uint8_t dir1value, uint8_t dir2value, uint8_t dir3value);
	
	/// @brief Read GPIO pin state
	uint8_t readGPIO(uint8_t gpioPin);	
	
	/// @brief Set CLKOUT frequency
	void setCLKOUT(uint8_t clkout);
	
	/// @brief Set SDCS (Synchronous Data Clock Setting)
	void setSDCS(uint8_t sdcs);	
	
	/// @brief Send direct command to ADC
	/// @param directCommand Use SELFCAL, SELFOCAL, SELFGCAL, SYSOCAL, SYSGCAL, or other command constants
	void sendDirectCommand(uint8_t directCommand);	

	//================================================================
	// CONVERSION/READING METHODS
	//================================================================
	
	/// @brief Read a single conversion result
	long readSingle();
	
	/// @brief Continuous reading from single input
	long readSingleContinuous();
	
	/// @brief Cycle through single-ended inputs and get conversion
	long cycleSingle(); //Ax + COM
	
	/// @brief Cycle through differential inputs and get conversion
	long cycleDifferential(); //Ax + Ay
	
	/// @brief Convert raw 24-bit ADC value to voltage
	float convertToVoltage(int32_t rawData);
	
	/// @brief Stop ongoing conversion
	void stopConversion();
	
//================================================================
// PUBLIC MEMBERS - EXPOSED FOR MULTI-ADC SUPPORT
// 
// WARNING: These members are intentionally public to allow external
// management of multiple ADS1256 instances in parallel.
// Use with caution and maintain proper synchronization when
// controlling multiple ADCs.
//================================================================
// private:
	
SPIClass* _spi; //Pointer to an SPIClass object

void waitForLowDRDY(); // Block until DRDY is low
void waitForHighDRDY(); // Block until DRDY is high
void updateMUX(uint8_t muxValue);
void CS_LOW();
void CS_HIGH();

	/// @brief Recalculate conversion parameter based on current PGA setting
	/// @details Updates conversionParameter multiplier used in voltage conversion
	void updateConversionParameter();

	//================================================================
	// PRIVATE/PROTECTED MEMBER VARIABLES
	//================================================================

	/// @brief Reference voltage in volts
	/// @details Typically 2.5V or 5.0V. Used in ADC-to-voltage conversion.
	float _VREF = 0;
	
	/// @brief Conversion parameter multiplier (PGA-dependent)
	/// @details Calculated based on VREF and current PGA setting
	float conversionParameter = 0;
	
	// --- Pin Assignments ---
	
	/// @brief GPIO pin connected to DRDY signal (Data Ready)
	/// @details Active LOW - goes LOW when conversion data is ready
	int8_t _DRDY_pin;
	
	/// @brief GPIO pin connected to RESET signal
	/// @details Active LOW - holds ADC in reset when LOW
	int8_t _RESET_pin;
	
	/// @brief GPIO pin connected to SYNC signal (optional)
	/// @details Can be PIN_UNUSED if not utilized. Synchronizes multiple ADCs.
	int8_t _SYNC_pin;
	
	/// @brief GPIO pin connected to Chip Select (CS) signal
	/// @details Active LOW - selects/deselects SPI communication
	int8_t _CS_pin;

	// --- Register Cache ---
	/// @brief Cached value of DRATE register (data rate/sampling frequency)
	byte _DRATE;
	
	/// @brief Cached value of ADCON register (ADC configuration)
	byte _ADCON;
	
	/// @brief Cached value of MUX register (channel/input selection)
	byte _MUX;
	
	/// @brief Cached value of PGA bits within ADCON register
	/// @details Range: 0-6 (represents gains 1, 2, 4, 8, 16, 32, 64)
	byte _PGA;
	
	/// @brief Cached value of GPIO register
	byte _GPIO;
	
	/// @brief Cached value of STATUS register
	byte _STATUS;
	
	/// @brief Cached GPIO output values
	byte _GPIOvalue;
	
	/// @brief Cached byte order setting (MSB or LSB)
	byte _ByteOrder;

	// --- Data Buffers ---
	/// @brief 3-byte buffer holding raw 24-bit ADC conversion data
	/// @details [0]=MSB, [1]=middle byte, [2]=LSB
	byte _outputBuffer[3];
	
	/// @brief Combined 32-bit signed value from _outputBuffer[3]
	/// @details Sign-extended to proper 24-bit signed integer
	/// @note PUBLIC for multi-ADC support - allows external access to conversion results
	long _outputValue;
	
	/// @brief Flag tracking whether continuous acquisition is active
	/// @note PUBLIC for multi-ADC support - allows external state management
	bool _isAcquisitionRunning;
	
	/// @brief Cycle counter for channel multiplexing
	/// @details Tracks position when cycling through differential input channels (0-3)
	/// @note PUBLIC for multi-ADC support - allows synchronized cycling across multiple ADCs
	uint8_t _cycle;
};
#endif