//==============================================================================
// ADS1256 Multi-Channel Synchronized Reading Example
//==============================================================================
/*
 Project:  ADS1256 Multi-Device Support
 Purpose:  Demonstrate synchronized parallel reading of 2 ADS1256 units
           providing 8 total differential channels (4 per ADC)
 
 Target:   Arduino Uno/Mega (compatible boards)
 Features:
   - Two ADS1256 ADCs on shared SPI bus with independent chip select
   - Synchronized differential channel cycling (4 channels per ADC)
   - Serial menu for data printing and performance measurement
   - Configurable data rate and gain settings
   
 Usage:
   Serial commands (921600 baud):
   - 'p': Start printing measured values to Serial Monitor
   - 'm': Start measuring throughput (cycles/second)
   - 's': Stop current operation
   
 Pin Configuration:
   ADC1: DRDY=5, RESET=2, CS=3
   ADC2: DRDY=8, RESET=6, CS=7
   SPI:  MOSI=11, MISO=12, SCK=13 (shared)
   
 Expected Performance:
   At DRATE_100SPS: ~25 complete measurement cycles/second
   (each cycle reads all 8 channels = 1 reading per differential pair)
   Refer to Table 3: "ADS1256 Multiplexer throughput values for different 
   DRATE settings" in the documentation
*/

#include "ADS1256.h"

// Helper macro to convert unsigned 24-bit to signed 32-bit
#define convertSigned24BitToLong(value) ((value) & (1l << 23) ? (value) - 0x1000000 : value)

//==============================================================================
// HARDWARE PIN DEFINITIONS
//==============================================================================

// ADS1256 #1 pins
const int PIN_DRDY_1 = 5;   // Data Ready signal (input, active LOW)
const int PIN_RESET_1 = 2;  // Reset signal (output, active LOW)
const int PIN_CS_1 = 3;     // Chip Select (output, active LOW)

// ADS1256 #2 pins
const int PIN_DRDY_2 = 8;   // Data Ready signal (input, active LOW)
const int PIN_RESET_2 = 6;  // Reset signal (output, active LOW)
const int PIN_CS_2 = 7;     // Chip Select (output, active LOW)

// Note: SPI pins (MOSI, MISO, SCK) are shared and defined on Arduino board
// MOSI=11, MISO=12, SCK=13 for Arduino Uno/Mega

//==============================================================================
// ADC INSTANCES & DATA STORAGE
//==============================================================================

/// @brief ADC Instance #1
/// Parameters: DRDY_pin, RESET_pin, SYNC_pin, CS_pin, VREF(volts), SPI_pointer
/// VREF = 2.5V (typical for 5V reference circuit)
ADS1256 ADC1(PIN_DRDY_1, PIN_RESET_1, ADS1256::PIN_UNUSED, PIN_CS_1, 2.500, &SPI);

/// @brief ADC Instance #2 (same configuration, different pins)
ADS1256 ADC2(PIN_DRDY_2, PIN_RESET_2, ADS1256::PIN_UNUSED, PIN_CS_2, 2.500, &SPI);

/// @brief Storage for 4 differential readings from ADC1
/// Index mapping: [0]=DIFF_0_1, [1]=DIFF_2_3, [2]=DIFF_4_5, [3]=DIFF_6_7
long resultsA[4];

/// @brief Storage for 4 differential readings from ADC2 (same mapping as resultsA)
long resultsB[4];

//==============================================================================
// STATE MACHINE & CONTROL VARIABLES
//==============================================================================

/// @brief Application state machine
enum State {
  INIT,       // Initialize and wait for user input
  WAIT_START, // Idle, waiting for serial command
  READ,       // Actively reading from ADCs
  PRINT,      // Print results to serial
  MEASURE     // Measure throughput/timing
};

State state = INIT;
State PrintOrMeasure = PRINT;  // Determines action after READ state

/// @brief Array index for storing current cycle results (0-3)
int i = 0;

/// @brief Counter for throughput measurement
unsigned long cycleCount = 0;

/// @brief Timestamp for throughput calculation
unsigned long initialTime = 0;
unsigned long currentTime = 0;

//==============================================================================
// PARALLEL READING FUNCTIONS - CORE MULTI-ADC OPERATIONS
//==============================================================================

/// @brief Check if ALL ADCs have data ready (non-blocking)
/// @return true if ALL DRDY signals are LOW (ready), false if any is HIGH (busy)
/// @note This is a non-blocking query; use waitForAllReady() for blocking wait
bool areAllReady() {
  // DRDY is active LOW - when LOW, ADC has completed conversion
  bool all_ready = true;
  
  if (digitalRead(PIN_DRDY_1) == HIGH) all_ready = false;  // ADC1 not ready
  if (digitalRead(PIN_DRDY_2) == HIGH) all_ready = false;  // ADC2 not ready
  
  return all_ready;
}

/// @brief Block execution until ALL ADCs are ready
/// @details Loops until all DRDY signals go LOW simultaneously
/// @warning This is a blocking call - execution stops until all ADCs ready
/// @note Critical for synchronized parallel reading
void waitForAllReady() {
  while (!areAllReady()) {
    // Busy-wait loop until all ADCs signal ready
    // Add CPU yield or delay here if needed for low-power operation
  }
}

/// @brief Initialize synchronized differential channel cycling
/// @details Sets both ADCs to start from DIFF_0_1 and enables acquisition
/// @note Must be called before readAllDifferentialParallel()
/// @warning Staggered initialization prevents SPI bus conflicts
void startSynchronizedDifferentialCycle() {
  
  // ===== Initialize ADC1 =====
  if (ADC1._isAcquisitionRunning == false) {
    ADC1._cycle = 0;
    ADC1._isAcquisitionRunning = true;

    // Ensure CS lines are HIGH (inactive)
    ADC1.CS_HIGH();
    ADC2.CS_HIGH();

    // Start SPI transaction for ADC1
    ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
    ADC1.CS_LOW();  // Activate ADC1

    // Write DIFF_0_1 to MUX register (0x50=WREG command, 1=MUX register)
    ADC1._spi->transfer(0x50 | 1);  // WREG command + MUX register
    ADC1._spi->transfer(0x00);      // Number of bytes to follow minus 1
    ADC1._spi->transfer(DIFF_0_1);  // Select AIN0+AIN1 differential

    ADC1.CS_HIGH();  // Deactivate ADC1
    ADC1._spi->endTransaction();  // Release SPI bus
    delay(50);  // Wait for register update
  }

  // ===== Initialize ADC2 (separate, identical process) =====
  if (ADC2._isAcquisitionRunning == false) {
    ADC2._cycle = 0;
    ADC2._isAcquisitionRunning = true;

    // Start SPI transaction for ADC2
    ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
    ADC2.CS_LOW();  // Activate ADC2

    // Write DIFF_0_1 to MUX register
    ADC2._spi->transfer(0x50 | 1);  // WREG command
    ADC2._spi->transfer(0x00);      // Number of bytes
    ADC2._spi->transfer(DIFF_0_1);  // Select channel

    ADC2.CS_HIGH();  // Deactivate ADC2
    ADC2._spi->endTransaction();  // Release SPI
    delay(50);
  }
}

/// @brief Read both ADCs in parallel with synchronized channel cycling
/// @param result1 Pointer to storage for ADC1 conversion result (24-bit signed)
/// @param result2 Pointer to storage for ADC2 conversion result (24-bit signed)
/// @details This is the CORE FUNCTION for multi-ADC synchronized reading
/// Channel cycle:
///   Cycle 0 (read from previous): DIFF_0_1, Setup next: DIFF_2_3
///   Cycle 1 (read from previous): DIFF_2_3, Setup next: DIFF_4_5
///   Cycle 2 (read from previous): DIFF_4_5, Setup next: DIFF_6_7
///   Cycle 3 (read from previous): DIFF_6_7, Setup next: DIFF_0_1
/// After 4 cycles = all 8 channels read once
/// @note Synchronization: Both ADCs wait for ALL to be ready before proceeding
void readAllDifferentialParallel(long* result1, long* result2) {
  
  // SYNCHRONIZATION POINT: Wait for both ADCs to complete conversions
  // This is critical - ensures minimal time skew between readings
  waitForAllReady();  // Block until ALL DRDY signals are LOW

  // Determine MUX value for NEXT channel based on current cycle
  uint8_t mux_value;
  switch (ADC1._cycle) {
    case 0: mux_value = DIFF_2_3; break;  // Next will read 2-3
    case 1: mux_value = DIFF_4_5; break;  // Next will read 4-5
    case 2: mux_value = DIFF_6_7; break;  // Next will read 6-7
    case 3: mux_value = DIFF_0_1; break;  // Next will read 0-1 (wrap around)
  }

  // ===== READ ADC1 IN PARALLEL WITH ADC2 =====
  // Both ADCs are read sequentially but within tight SPI transactions
  // This minimizes timing differences between the two readings
  
  // --- ADC1 READ SEQUENCE ---
  ADC1.CS_LOW();  // Activate ADC1
  ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  // STEP 1: Update MUX register to next channel
  ADC1._spi->transfer(0x50 | MUX_REG);  // WREG command + MUX register address
  ADC1._spi->transfer(0x00);            // Number of bytes to follow - 1
  ADC1._spi->transfer(mux_value);       // Write next channel selection

  // STEP 2: Restart analog conversion pipeline
  ADC1._spi->transfer(0b11111100);  // SYNC command - restart conversion
  delayMicroseconds(4);             // t11 timing requirement
  ADC1._spi->transfer(0b11111111);  // WAKEUP command

  // STEP 3: Read conversion result from CURRENT cycle
  // Note: This reads data from the PREVIOUS channel selection
  ADC1._spi->transfer(0b00000001);  // RDATA command
  delayMicroseconds(7);             // t6 timing requirement

  // Read 24-bit result (3 bytes)
  ADC1._outputBuffer[0] = ADC1._spi->transfer(0);  // MSB (bits 23-16)
  ADC1._outputBuffer[1] = ADC1._spi->transfer(0);  // Middle (bits 15-8)
  ADC1._outputBuffer[2] = ADC1._spi->transfer(0);  // LSB (bits 7-0)

  // Combine 3 bytes into 32-bit value
  ADC1._outputValue = ((long)ADC1._outputBuffer[0] << 16) | 
                      ((long)ADC1._outputBuffer[1] << 8) | 
                      (ADC1._outputBuffer[2]);
  
  // Convert from unsigned 24-bit to signed 32-bit
  ADC1._outputValue = convertSigned24BitToLong(ADC1._outputValue);

  ADC1._spi->endTransaction();  // Release SPI bus
  ADC1.CS_HIGH();  // Deactivate ADC1

  *result1 = ADC1._outputValue;  // Store result via pointer

  // --- ADC2 READ SEQUENCE (identical to ADC1) ---
  ADC2.CS_LOW();  // Activate ADC2
  ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  // STEP 1: Update MUX
  ADC2._spi->transfer(0x50 | MUX_REG);
  ADC2._spi->transfer(0x00);
  ADC2._spi->transfer(mux_value);

  // STEP 2: Restart conversion
  ADC2._spi->transfer(0b11111100);  // SYNC
  delayMicroseconds(4);
  ADC2._spi->transfer(0b11111111);  // WAKEUP

  // STEP 3: Read result
  ADC2._spi->transfer(0b00000001);  // RDATA
  delayMicroseconds(7);

  ADC2._outputBuffer[0] = ADC2._spi->transfer(0);
  ADC2._outputBuffer[1] = ADC2._spi->transfer(0);
  ADC2._outputBuffer[2] = ADC2._spi->transfer(0);

  ADC2._outputValue = ((long)ADC2._outputBuffer[0] << 16) | 
                      ((long)ADC2._outputBuffer[1] << 8) | 
                      (ADC2._outputBuffer[2]);
  ADC2._outputValue = convertSigned24BitToLong(ADC2._outputValue);

  ADC2._spi->endTransaction();
  ADC2.CS_HIGH();

  *result2 = ADC2._outputValue;

  // ===== CYCLE MANAGEMENT =====
  // Increment cycle counter (same for both ADCs - they're synchronized)
  ADC1._cycle++;
  ADC2._cycle++;
  
  // Wrap around after 4 cycles (all differential channels read)
  if (ADC1._cycle == 4) {
    ADC1._cycle = 0;
    ADC2._cycle = 0;
  }
}

/// @brief Stop conversions on both ADCs
/// @details Sends SDATAC command to stop continuous data acquisition
/// @note Must be called after startSynchronizedDifferentialCycle() when done
void stopConversions() {
  // Ensure conversions are complete before stopping
  waitForAllReady();

  // Ensure CS lines are deactivated
  ADC1.CS_HIGH();
  ADC2.CS_HIGH();

  // --- Stop ADC1 ---
  ADC1.CS_LOW();
  ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  ADC1._spi->transfer(0b00001111);  // SDATAC command - stop continuous data

  ADC1._spi->endTransaction();
  ADC1.CS_HIGH();

  // Reset acquisition flag
  ADC1._isAcquisitionRunning = false;

  // --- Stop ADC2 ---
  ADC2.CS_LOW();
  ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  ADC2._spi->transfer(0b00001111);  // SDATAC command

  ADC2._spi->endTransaction();
  ADC2.CS_HIGH();

  // Reset acquisition flag
  ADC2._isAcquisitionRunning = false;
}


//==============================================================================
// SETUP & MAIN LOOP
//==============================================================================

void setup() {
  // Initialize Serial communication at 921600 baud for fast data output
  Serial.begin(921600);
  while (!Serial);  // Wait for Serial connection

  Serial.println("Setting up ADS1256 with SYNCHRONIZED PARALLEL reading...");

  // ===== Initialize ADC1 =====
  ADC1.InitializeADC();
  delay(1);
  
  // Configure ADC1 settings
  ADC1.setPGA(PGA_64);          // Set gain to 64
  delay(1);
  ADC1.setMUX(DIFF_0_1);        // Select first differential pair
  delay(1);
  ADC1.setDRATE(DRATE_100SPS);  // Set sampling rate to 100 samples/second
  delay(1);
  ADC1.sendDirectCommand(SELFCAL);  // Perform self-calibration
  delay(100);

  // Display ADC1 configuration
  Serial.println("\nADC1:");
  Serial.print("PGA: "); Serial.println(ADC1.getPGA());
  Serial.print("MUX: "); Serial.println(ADC1.readRegister(MUX_REG));
  Serial.print("DRATE: "); Serial.println(ADC1.readRegister(DRATE_REG));
  delay(100);

  // ===== Initialize ADC2 =====
  ADC2.InitializeADC();
  delay(1);
  
  // Configure ADC2 (identical settings to ADC1 for synchronized operation)
  ADC2.setPGA(PGA_64);
  delay(1);
  ADC2.setMUX(DIFF_0_1);
  delay(1);
  ADC2.setDRATE(DRATE_100SPS);
  delay(1);
  ADC2.sendDirectCommand(SELFCAL);
  delay(100);

  // Display ADC2 configuration
  Serial.println("\nADC2:");
  Serial.print("PGA: "); Serial.println(ADC2.getPGA());
  Serial.print("MUX: "); Serial.println(ADC2.readRegister(MUX_REG));
  Serial.print("DRATE: "); Serial.println(ADC2.readRegister(DRATE_REG));
  delay(100);

  // Start synchronized differential cycling on both ADCs
  startSynchronizedDifferentialCycle();

  Serial.println("\nSynchronized parallel acquisition ready.");
  Serial.println("Send 'p' to print data, 'm' to measure rate, or 's' to stop.");
}

void loop() {
  switch (state) {

    // ===== INIT STATE =====
    // Display menu and wait for user input
    case INIT:
      Serial.println("\n=== System ready ===");
      Serial.println("Press 'p' to print measurements");
      Serial.println("Press 'm' to measure the data rate");

      state = WAIT_START;
      break;

    // ===== WAIT_START STATE =====
    // Monitor Serial for user commands
    case WAIT_START:
      if (Serial.available()) {
        char c = Serial.read();
        
        if (c == 'p' || c == 'P') {
          // User selected print mode
          Serial.println("\n--- Printing data (press 's' to stop) ---");
          Serial.println("ADC1 Results\t\t\tADC2 Results");
          Serial.println("CH0-1\tCH2-3\tCH4-5\tCH6-7\t\tCH0-1\tCH2-3\tCH4-5\tCH6-7");
          delay(2000);

          i = 0;
          PrintOrMeasure = PRINT;
          state = READ;
          startSynchronizedDifferentialCycle();

        } else if (c == 'm' || c == 'M') {
          // User selected measure mode
          Serial.println("\n--- Measuring throughput (press 's' to stop) ---");
          delay(2000);

          cycleCount = 0;
          i = 0;
          PrintOrMeasure = MEASURE;
          state = READ;
          initialTime = micros();  // Start timer
          startSynchronizedDifferentialCycle();
        }
      }
      break;

    // ===== READ STATE =====
    // Read from both ADCs in parallel
    case READ:
      // Read both ADCs and store results in arrays
      readAllDifferentialParallel(&resultsA[i], &resultsB[i]);
      i++;

      // After collecting 4 results (one full cycle of all channels)
      if (i >= 4) {
        i = 0;
        // Transition to PRINT or MEASURE state
        state = PrintOrMeasure;
      }
      break;

    // ===== PRINT STATE =====
    // Output current readings to Serial Monitor
    case PRINT:
      // Print ADC1 results (4 differential channels)
      for (int j = 0; j < 4; j++) {
        Serial.print(resultsA[j]);
        if (j < 3) Serial.print("\t");
      }
      Serial.print("\t\t");  // Separator between ADC1 and ADC2
      
      // Print ADC2 results (4 differential channels)
      for (int j = 0; j < 4; j++) {
        Serial.print(resultsB[j]);
        if (j < 3) Serial.print("\t");
      }
      Serial.println();  // End line

      state = READ;  // Continue reading

      // Check for stop command
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
          Serial.println("\nStopping...");
          delay(500);
          stopConversions();
          i = 0;
          PrintOrMeasure = PRINT;
          state = INIT;  // Return to menu
        }
      }
      break;

    // ===== MEASURE STATE =====
    // Measure throughput (cycles per second)
    case MEASURE:
      cycleCount++;  // Count each measurement cycle
      state = READ;  // Continue reading

      // Report performance every 1000 cycles
      if (cycleCount >= 1000) {
        currentTime = micros();  // Get current timestamp
        
        // Calculate and display timing
        unsigned long elapsed_us = currentTime - initialTime;
        unsigned long elapsed_ms = elapsed_us / 1000;
        
        Serial.print("\n");
        Serial.print(cycleCount);
        Serial.print(" cycles (8 total channels per cycle) in ");
        Serial.print(elapsed_ms);
        Serial.print(" ms (~");
        Serial.print((cycleCount * 1000) / elapsed_ms);
        Serial.println(" cycles/sec)");

        // Reset for next measurement
        cycleCount = 0;
        initialTime = micros();
      }

      // Check for stop command
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
          Serial.println("\nStopping...");
          delay(500);
          stopConversions();
          i = 0;
          PrintOrMeasure = PRINT;
          state = INIT;  // Return to menu
        }
      }
      break;
  }
}
