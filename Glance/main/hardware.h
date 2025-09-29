#ifndef HARDWARE_H
#define HARDWARE_H
#include <stdbool.h>

// Define GPIO pins according to the hardware v2.1 specification

// Power Control
#define PIN_PWR_CTRL    14  // Controls the power switch for V33_2 rail

// User Interface
#define PIN_LED         48  // General-purpose LED
#define PIN_BTN         1   // User-programmable button

// E-Paper Display (SPI)
#define PIN_EPD_BSY     40  // E-Paper Busy signal
#define PIN_EPD_RST     39  // E-Paper Reset signal
#define PIN_EPD_DC      38  // E-Paper Data/Command signal
#define PIN_EPD_CS      37  // E-Paper Chip Select

// External Flash (SPI)
#define PIN_FLS_CS      21  // External Flash Chip Select

// Shared SPI Bus
#define PIN_SPI_CLK     36  // SPI Clock
#define PIN_SPI_MOSI    35  // SPI MOSI
#define PIN_SPI_MISO    47  // SPI MISO

// I2C Bus
#define PIN_I2C_SDA     3   // I2C Data
#define PIN_I2C_SCL     9   // I2C Clock
#define PIN_I2C_ALRT    10  // Fuel Gauge Alert

void hardware_init(void);
void hardware_deinit(void);
void hardware_set_led(bool on);

#endif // HARDWARE_H
