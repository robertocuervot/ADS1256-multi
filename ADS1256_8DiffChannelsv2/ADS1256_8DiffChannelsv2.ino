#include <ADS1256.h>
#include <SPI.h>

#define convertSigned24BitToLong(value) ((value) & (1l << 23) ? (value) - 0x1000000 : value)

// Pins for ADS1256 #1
const int PIN_DRDY_1 = 5;
const int PIN_RESET_1 = 2;
const int PIN_CS_1 = 3;

// Pins for ADS1256 #2
const int PIN_DRDY_2 = 8;
const int PIN_RESET_2 = 6;
const int PIN_CS_2 = 7;

// Create instances
ADS1256 ADC1(PIN_DRDY_1, PIN_RESET_1, ADS1256::PIN_UNUSED, PIN_CS_1, 2.500, &SPI);
ADS1256 ADC2(PIN_DRDY_2, PIN_RESET_2, ADS1256::PIN_UNUSED, PIN_CS_2, 2.500, &SPI);

// Array of ADS1256 pointers for easier management
ADS1256* adcs[] = {&ADC1, &ADC2};
const int NUM_ADCS = 2;

long resultsA[4];
long resultsB[4];

enum State {
  INIT,
  WAIT_START,
  READ,
  PRINT,
  MEASURE
};

State state = INIT;
State PrintOrMeasure = PRINT;

int i = 0;
unsigned long cycleCount = 0;
unsigned long initialTime = 0;
unsigned long currentTime = 0;

// ============================================================
// PARALLEL READ FUNCTIONS (Similar to HX711MULTI)
// ============================================================

// Wait for all ADCs to be ready (DRDY pin LOW)
bool areAllReady() {
  bool all_ready = true;
  if (digitalRead(PIN_DRDY_1) == HIGH) all_ready = false;
  if (digitalRead(PIN_DRDY_2) == HIGH) all_ready = false;
  // Serial.println("areAllReady() called");
  return all_ready;
}

// Wait blocking for all ADCs to be ready
void waitForAllReady() {
  while (!areAllReady()) {}
}

// Update MUX for all ADCs to the same channel (synchronized)
void updateAllMUX(uint8_t mux_value) {
  // Update MUX register for both ADCs to the same channel
  // This keeps them synchronized during the reading cycle

  // Update ADC1
  ADC1.CS_LOW(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, LOW);
  
  ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  ADC1._spi->transfer(0x50 | MUX_REG);  // WREG + MUX address
  ADC1._spi->transfer(0x00);             // Empty byte
  ADC1._spi->transfer(mux_value);        // MUX value
  ADC1._spi->endTransaction();
  
  ADC1.CS_HIGH(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, HIGH);
  
  // delay(5);  // Brief delay between devices
  
  // Update ADC2 with same value
  ADC2.CS_LOW(); // if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, LOW);
  
  ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  ADC2._spi->transfer(0x50 | MUX_REG);
  ADC2._spi->transfer(0x00);
  ADC2._spi->transfer(mux_value);
  ADC2._spi->endTransaction();
  
  ADC2.CS_HIGH(); // if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, HIGH);
}

void startSynchronizedDifferentialCycle() {
  // Initialize ADC1
  if (ADC1._isAcquisitionRunning == false) {
    ADC1._cycle = 0;
    ADC1._isAcquisitionRunning = true;
    
    ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
    ADC1.CS_LOW(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, LOW);
    
    ADC1._spi->transfer(0x50 | 1);      // WREG
    ADC1._spi->transfer(0x00);
    ADC1._spi->transfer(DIFF_0_1);
    
    ADC1.CS_HIGH(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, HIGH);
    ADC1._spi->endTransaction();
    delay(50);
  }

  // Initialize ADC2 (separately, same way)
  if (ADC2._isAcquisitionRunning == false) {
    ADC2._cycle = 0;
    ADC2._isAcquisitionRunning = true;
    
    ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
    ADC2.CS_LOW(); // if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, LOW);
    
    ADC2._spi->transfer(0x50 | 1);      // WREG
    ADC2._spi->transfer(0x00);
    ADC2._spi->transfer(DIFF_0_1);
    
    ADC2.CS_HIGH(); // if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, HIGH);
    ADC2._spi->endTransaction();
    delay(50);
  }
}

void readAllDifferentialParallel(long *result1, long *result2) {
  // First, wait for ALL ADCs to be ready simultaneously
  waitForAllReady();
  // Serial.println("ALL READY!!!!");

  // Determine which channel to read based on the cycle counter
  uint8_t mux_value;
  switch (ADC1._cycle) {
    case 0: mux_value = DIFF_0_1; break;
    case 1: mux_value = DIFF_2_3; break;
    case 2: mux_value = DIFF_4_5; break;
    case 3: mux_value = DIFF_6_7; break;
  }

  // Update MUX for BOTH ADCs simultaneously (using updateAllMUX)
  // updateAllMUX(mux_value);

  // Now read from both ADCs
  // ADC1 read
  ADC1.CS_LOW(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, LOW);
  ADC1._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  // Update MUX
  ADC1._spi->transfer(0x50 | MUX_REG);  // WREG
  ADC1._spi->transfer(0x00);
  ADC1._spi->transfer(mux_value);
  
  // Read data
  ADC1._spi->transfer(0b11111100);  // SYNC
  delayMicroseconds(4);
  ADC1._spi->transfer(0b11111111);  // WAKEUP
  ADC1._spi->transfer(0b00000001);  // RDATA
  delayMicroseconds(7);

  ADC1._outputBuffer[0] = ADC1._spi->transfer(0);
  ADC1._outputBuffer[1] = ADC1._spi->transfer(0);
  ADC1._outputBuffer[2] = ADC1._spi->transfer(0);

  ADC1._outputValue = ((long)ADC1._outputBuffer[0] << 16) | 
                      ((long)ADC1._outputBuffer[1] << 8) | 
                      (ADC1._outputBuffer[2]);
  ADC1._outputValue = convertSigned24BitToLong(ADC1._outputValue);
  
  ADC1._spi->endTransaction();
  ADC1.CS_HIGH(); // if (ADC1._CS_pin != -1) digitalWrite(ADC1._CS_pin, HIGH);

  *result1 = ADC1._outputValue;

  // ADC2 read (same process)
  ADC2.CS_LOW(); // if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, LOW);
  ADC2._spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  // Update MUX
  ADC2._spi->transfer(0x50 | MUX_REG);
  ADC2._spi->transfer(0x00);
  ADC2._spi->transfer(mux_value);
  
  ADC2._spi->transfer(0b11111100);  // SYNC
  delayMicroseconds(4);
  ADC2._spi->transfer(0b11111111);  // WAKEUP
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
  ADC2.CS_HIGH(); //if (ADC2._CS_pin != -1) digitalWrite(ADC2._CS_pin, HIGH);

  *result2 = ADC2._outputValue;

  // Increment cycle counter (shared between both, they're synchronized)
  ADC1._cycle++;
  ADC2._cycle++;
  if (ADC1._cycle == 4) {
    ADC1._cycle = 0;
    ADC2._cycle = 0;
  }
}

// ============================================================
// SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(921600);
  while (!Serial);

  Serial.println("Setting up ADS1256 with SYNCHRONIZED PARALLEL reading...");

  // Initialize ADC1
  ADC1.InitializeADC();
  delay(1);
  ADC1.setPGA(PGA_64);
  delay(1);
  ADC1.setMUX(DIFF_0_1);
  delay(1);
  ADC1.setDRATE(DRATE_30000SPS);
  delay(1);
  ADC1.sendDirectCommand(SELFCAL);
  delay(100);

  Serial.println("\nADC1:");
  Serial.print("PGA: "); Serial.println(ADC1.getPGA());
  Serial.print("MUX: "); Serial.println(ADC1.readRegister(MUX_REG));
  Serial.print("DRATE: "); Serial.println(ADC1.readRegister(DRATE_REG));
  delay(100);

  // Initialize ADC2
  ADC2.InitializeADC();
  delay(1);
  ADC2.setPGA(PGA_64);
  delay(1);
  ADC2.setMUX(DIFF_0_1);
  delay(1);
  ADC2.setDRATE(DRATE_30000SPS);
  delay(1);
  ADC2.sendDirectCommand(SELFCAL);
  delay(100);

  Serial.println("\nADC2:");
  Serial.print("PGA: "); Serial.println(ADC2.getPGA());
  Serial.print("MUX: "); Serial.println(ADC2.readRegister(MUX_REG));
  Serial.print("DRATE: "); Serial.println(ADC2.readRegister(DRATE_REG));
  delay(100);

  // Start synchronized differential cycles
  startSynchronizedDifferentialCycle();

  Serial.println("\nSynchronized parallel acquisition ready.");
}

void loop() {
  switch (state) {

    case INIT:
      Serial.println("System ready");
      Serial.println("press 'p' to print measurements or...");
      Serial.println("Press 'm' to measure the data rate");

      state = WAIT_START;
      break;

    case WAIT_START:
      if (Serial.available()) {
        char c = Serial.read();
        if (c == 'p' || c == 'P') {
          Serial.println("Printing data...");
          Serial.println("Press 's' to stop");
          delay(2000);

          i = 0;
          PrintOrMeasure = PRINT;
          state = READ;

          startSynchronizedDifferentialCycle();

        } else if (c == 'm' || c == 'M') {
          Serial.println("Measuring data rate...");
          Serial.println("Press 's' to stop");
          delay(2000);

          cycleCount = 0;
          initialTime = micros();
          i = 0;
          PrintOrMeasure = MEASURE;
          state = READ;

          startSynchronizedDifferentialCycle();
        }
      }
      break;

    case READ:
      // Read both ADCs in parallel
      readAllDifferentialParallel(&resultsA[i], &resultsB[i]);
      i++;

      if (i >= 4) {
        i = 0;
        state = PrintOrMeasure;
      }
      break;

    case PRINT:
      for (int j = 0; j < 4; j++) {
        Serial.print(resultsA[j]);
        if (j < 3) Serial.print("\t");
      }
      Serial.print(" | ");
      for (int j = 0; j < 4; j++) {
        Serial.print(resultsB[j]);
        if (j < 3) Serial.print("\t");
      }
      Serial.println();

      state = READ;

      if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
          Serial.println("Stopping");
          delay(500);
          ADC1.stopConversion();
          ADC2.stopConversion();
          i = 0;
          PrintOrMeasure = PRINT;
          state = INIT;
        }
      }
      break;

    case MEASURE:
      cycleCount++;
      state = READ;

      if (cycleCount >= 1000) {
        currentTime = micros();
        Serial.print("\n");
        Serial.print(cycleCount);
        Serial.print(" cycles (8 channels) in ");
        Serial.print((currentTime - initialTime) / 1000);
        Serial.println(" ms");

        cycleCount = 0;
        initialTime = micros();
      }

      if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
          Serial.println("Stopping");
          delay(500);
          ADC1.stopConversion();
          ADC2.stopConversion();
          i = 0;
          PrintOrMeasure = PRINT;
          state = INIT;
        }
      }
      break;
  }
}