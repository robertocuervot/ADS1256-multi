# ADS1256 Multi-Channel Support

A fork of the [ADS1256 Library](https://github.com/CuriousScientist0/ADS1256) by Curious Scientist, modified to support **simultaneous operation of multiple ADS1256 ADC units** on a single Arduino microcontroller.

## Overview

The ADS1256 is a 24-bit, 8-channel, simultaneous-sampling analog-to-digital converter with an SPI interface. This fork extends the original library to enable parallel reading from multiple ADS1256 chips, allowing you to dramatically increase the number of analog input channels while maintaining synchronization between devices.

**Current Status**: Working implementation with 2 ADS1256 units (8 differential channels total). Full library refactoring is planned for future releases.

## Key Features

- ✅ **Multi-ADC Support**: Control multiple ADS1256 devices simultaneously from a single microcontroller
- ✅ **Synchronized Parallel Reading**: Read data from all ADCs in a coordinated manner to minimize time skew
- ✅ **Differential Input Support**: Examples include 8 differential channels (4 per ADS1256)
- ✅ **Flexible Pin Configuration**: Each ADS1256 can use independent pins for DRDY, RESET, and CS
- ✅ **Backward Compatible**: Original library methods remain unchanged and functional
- ✅ **SPI Transaction Support**: Proper SPI transaction handling for reliable communication

## Current Implementation

### What's Included

This repository contains:

1. **Modified ADS1256 Library Files** (`ADS1256-Multi_8DiffChannels/`)
   - `ADS1256.h` - Header file with extended public access to critical variables
   - `ADS1256.cpp` - Implementation file (unchanged core functionality)
   
2. **Example Sketch** (`ADS1256-Multi_8DiffChannels/ADS1256-Multi_8DiffChannels.ino`)
   - Complete working example demonstrating parallel operation of 2 ADS1256 units
   - Implements 8 synchronized differential channels (4 per ADC)
   - Serial menu for data display and performance measurement

3. **Original Library Files** (`src/`)
   - Unmodified ADS1256 library files for reference

### What Changed

**Important Note**: The original ADS1256 library code is **completely unchanged**. This fork makes minimal modifications to expose internal variables and methods:

#### 1. Exposed Public Access to Internal Variables (ADS1256.h)

The original library declared several variables as private. These were exposed to public scope to allow the example sketch to manage multiple ADC instances

## Hardware Setup

### Pin Configuration

Each ADS1256 unit requires 4 GPIO pins:

| Signal | Description | Example Pin (ADC1) | Example Pin (ADC2) |
|--------|-------------|-------------------|-------------------|
| DRDY   | Data Ready (Input) | 5 | 8 |
| RESET  | Reset (Output) | 2 | 6 |
| SYNC   | Sync/Master Clock (Optional) | Unused | Unused |
| CS     | Chip Select (Output) | 3 | 7 |

**SPI Pins** (Shared across all ADS1256 units):
- **MOSI**: Pin 11 (Arduino Uno/Mega)
- **MISO**: Pin 12 (Arduino Uno/Mega)
- **SCK**: Pin 13 (Arduino Uno/Mega)

*Note: Pin assignments vary by microcontroller. Check your board's SPI documentation.*

## Tested Platforms

This implementation has been **verified and tested on**:
- **Arduino Uno** ✅

**Other boards** (Arduino Mega, Leonardo, Due, Nano, etc.) may be compatible, but have not been tested. If using a different Arduino board, verify:
- SPI pin locations (MOSI, MISO, SCK may differ)
- Available GPIO pins for DRDY, RESET, and CS signals
- Voltage levels and current requirements for your specific board

## Usage

### Basic Setup (2 ADCs, 8 Differential Channels)

```cpp
#include "ADS1256.h"

// Define pins for each ADC
const int PIN_DRDY_1 = 5, PIN_RESET_1 = 2, PIN_CS_1 = 3;
const int PIN_DRDY_2 = 8, PIN_RESET_2 = 6, PIN_CS_2 = 7;

// Create ADC instances (using default SPI)
ADS1256 ADC1(PIN_DRDY_1, PIN_RESET_1, ADS1256::PIN_UNUSED, PIN_CS_1, 2.500, &SPI);
ADS1256 ADC2(PIN_DRDY_2, PIN_RESET_2, ADS1256::PIN_UNUSED, PIN_CS_2, 2.500, &SPI);

void setup() {
  // Initialize both ADCs
  ADC1.InitializeADC();
  ADC2.InitializeADC();
  
  // Configure sampling parameters
  ADC1.setPGA(PGA_64);
  ADC1.setDRATE(DRATE_100SPS);
  
  ADC2.setPGA(PGA_64);
  ADC2.setDRATE(DRATE_100SPS);
  
  // Start synchronized differential acquisition
  startSynchronizedDifferentialCycle();
}

void loop() {
  long result1, result2;
  readAllDifferentialParallel(&result1, &result2);
  
  Serial.print(result1);
  Serial.print("\t");
  Serial.println(result2);
}
```

### Parallel Reading Functions

#### 1. `areAllReady()`
Checks if all ADC DRDY signals are LOW (data ready).

```cpp
bool ready = areAllReady();  // Returns true if all ADCs are ready
```

#### 2. `waitForAllReady()`
Blocking wait until all ADCs have data ready.

```cpp
waitForAllReady();  // Blocks until all DRDY pins go LOW
```

#### 3. `startSynchronizedDifferentialCycle()`
Initiates the synchronized differential channel cycling sequence. Must be called before `readAllDifferentialParallel()`.

```cpp
startSynchronizedDifferentialCycle();  // Start the cycle
```

#### 4. `readAllDifferentialParallel(long* result1, long* result2)`
Reads data from all ADCs in parallel. Cycles through differential channels:
- Cycle 0: DIFF_0_1 (A0+A1 vs GND)
- Cycle 1: DIFF_2_3 (A2+A3 vs GND)
- Cycle 2: DIFF_4_5 (A4+A5 vs GND)
- Cycle 3: DIFF_6_7 (A6+A7 vs GND)

```cpp
long data1, data2;
readAllDifferentialParallel(&data1, &data2);  // Read synchronized data
```

#### 5. `stopConversions()`
Gracefully stops all ADC conversions.

```cpp
stopConversions();  // Stop and prepare for new acquisition
```

## Performance Considerations

### Timing

- **Parallel Reading**: All ADCs are read sequentially but within tight SPI transactions to minimize timing differences
- **Channel Cycling**: 4 cycles are required to read all 4 differential channel pairs (8 channels total)
- **Typical Speed**: At DRATE_100SPS, approximately 25 complete cycles per second (all 8 channels read)

### Limitations

- **Current Implementation**: Requires manual management of SPI transactions in sketch
- **Future Improvements**: Will be abstracted into library methods as design matures

## Future Plans

This is a **work-in-progress** toward a complete, independent multi-ADC library. Planned enhancements:

1. ✅ **Proof of Concept** (Current Status)
   - Working implementation with 2 ADS1256 units
   
2. 🔄 **Library Abstraction** (Next Phase)
   - Encapsulate parallel reading functions into library
   - Support for 3+ ADS1256 units
   - Automatic pin and configuration management
   - Device registry/manager class
   
3. 📋 **Advanced Features** (Future)
   - Support for mixed single-ended and differential channels
   - Automatic synchronization without manual cycling
   - Configurable channel patterns
   - Built-in calibration per device
   - Performance profiling and optimization

## Documentation

- **Original Library**: See [Curious Scientist's documentation](https://github.com/CuriousScientist0/ADS1256/blob/main/extras/ADS1256_ArduinoLibrary_Documentation_20251023.pdf)
- **ADS1256 Datasheet**: [TI ADS1256 24-Bit Delta-Sigma ADC](https://www.ti.com/product/ADS1256)
- **SPI Protocol**: Standard Arduino SPI documentation

## Example Features

The included example sketch (`ADS1256-Multi_8DiffChannels.ino`) demonstrates:

- ✅ Simultaneous initialization of 2 ADS1256 units
- ✅ Parallel synchronized reading of 8 differential channels
- ✅ Serial menu for interactive testing:
  - **'p'**: Continuous data printing
  - **'m'**: Performance measurement (cycles per second)
  - **'s'**: Stop acquisition
- ✅ Configuration display (PGA, MUX, DRATE values)

## Credits

- **Original Library**: [Curious Scientist](https://github.com/CuriousScientist0/ADS1256)
  - Initial implementation and core functionality
  - Excellent documentation and design

- **This Fork**: Multi-ADC support and parallel reading implementation

## License

This fork maintains the same license as the original ADS1256 library. See [LICENSE](LICENSE) file.

## Notes for Future Library Development

As this project evolves toward a complete independent library:

- **Encapsulation**: Helper functions (`areAllReady()`, `readAllDifferentialParallel()`, etc.) will be moved into a dedicated manager class
- **Generalization**: Implementation will support arbitrary numbers of ADCs, not just 2
- **Backward Compatibility**: Original library methods will continue to work unchanged

## Support & Issues

For questions about the original library, visit: https://github.com/CuriousScientist0/ADS1256

For issues specific to this multi-ADC fork, feel free to open an issue or discussion on this repository.
