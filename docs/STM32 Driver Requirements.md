# STM32 Driver Requirements

Nov 3, 2026 · @Ryan

## Scope and conventions

This document specifies six bare-metal drivers for an STM32 Nucleo board: GPIO, UART, I2C, SPI, a BME280 sensor driver, and a microSD (SPI mode) block driver with a FAT filesystem on top. Vendor HAL libraries are out of scope; drivers use CMSIS register definitions only. Builds use STM32CubeIDE with an Empty project type, so no HAL code is generated.

**Requirement IDs** follow `DRV-<AREA>-<NNN>`: GEN (all drivers), GPIO, UART, I2C, SPI, BME, SD. IDs are never reused after deletion.

**Language.** "Shall" marks a mandatory requirement. "Should" marks a goal that may be waived with a recorded rationale.

**Verification methods** (same set as NASA F´):

| Method | Meaning |
| --- | --- |
| Test (T) | Executed on hardware or host with pass/fail criteria |
| Analysis (A) | Calculation or logic-analyzer capture review |
| Inspection (I) | Code or document review |
| Demonstration (D) | Observed behavior without formal measurement |

**Out of scope for v1:** multi-block SD transfers, DMA on I2C, multi-controller I2C, CAN, USB, littlefs, and low-power modes.

## System context

The drivers run on one STM32 Nucleo-64 board and talk to two external modules plus the host PC. The SD card sits on its own SPI peripheral because the common blue microSD module does not release MISO when deselected.

&#91;embedded content: driver layers · arrows point from caller to callee\]

Device drivers call only the bus interfaces, never registers, so the BME280 driver swaps between I2C1 and SPI1 at init and the SD driver owns SPI2 alone.

| Item | Part | Interface | Notes |
| --- | --- | --- | --- |
| MCU board | Nucleo-F401RE (STM32F401RE, Cortex-M4F, 84 MHz max, 96 KB RAM) | — | Onboard ST-Link for flashing, debug, and virtual COM port |
| Host link | ST-Link virtual COM port | UART | Console and log output |
| Sensor | BME280 breakout, 3.3 V | I2C, then SPI | Chip ID 0x60; I2C address 0x76 or 0x77 |
| Storage | microSD module, SDHC ≤ 32 GB, FAT32 | SPI (dedicated) | Powered from 5 V if the module has a regulator |
| Test gear | 24 MHz 8-channel logic analyzer + PulseView | — | Protocol decode for all buses |

**Pin assignment** for the Nucleo-F401RE. Morpho pin numbers are verified against the Nucleo-64 user manual (UM1724).

| Signal | Peripheral | Pin | Header | AF |
| --- | --- | --- | --- | --- |
| Console TX | USART2 | PA2 | Wired to ST-Link VCP | AF7 |
| Console RX | USART2 | PA3 | Wired to ST-Link VCP | AF7 |
| BME280 SCL | I2C1 | PB8 | D15 | AF4 |
| BME280 SDA | I2C1 | PB9 | D14 | AF4 |
| BME280 SCK | SPI1 | PA5 | D13 | AF5 |
| BME280 MISO (SDO) | SPI1 | PA6 | D12 | AF5 |
| BME280 MOSI (SDI) | SPI1 | PA7 | D11 | AF5 |
| BME280 CS (CSB) | GPIO | PB6 | D10 | Output |
| SD SCK | SPI2 | PB13 | CN10 pin 30 | AF5 |
| SD MISO | SPI2 | PB14 | CN10 pin 28 | AF5 |
| SD MOSI | SPI2 | PB15 | CN10 pin 26 | AF5 |
| SD CS | GPIO | PB12 | CN10 pin 16 | Output |
| SD module power (with regulator) | +5V | — | CN7 pin 18 | Power |
| BME280 power, 3.3 V modules | +3V3 | — | CN7 pin 16 | Power |
| Common ground | GND | — | CN7 pin 20 | Power |

**Pin notes:**

- PA5 also drives the green user LED (LD2), so it flickers during SPI1 traffic. That is harmless at these speeds.
- The BME280 is wired for either I2C or SPI at a time, not both; rewire between the two test campaigns.
- SPI1 runs from APB2 (up to 42 MHz SCK) and SPI2 from APB1 (up to 21 MHz SCK), both well above the 8 MHz in DRV-SPI-003.

## General requirements

Every driver shall meet these before its bus-specific requirements count as verified.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-GEN-001 | Drivers shall access hardware through CMSIS register definitions only, with no vendor HAL or LL code. | I |
| DRV-GEN-002 | Every public function that can fail shall return a status code from a shared enum (OK, TIMEOUT, NACK, BUSY, BAD\_PARAM, HW\_ERROR). | I |
| DRV-GEN-003 | No blocking call shall wait indefinitely; every hardware wait shall have a caller-configurable timeout. | T |
| DRV-GEN-004 | Drivers shall not allocate memory dynamically. | I |
| DRV-GEN-005 | Each peripheral shall be configured through an init function taking a configuration struct; no global configuration state outside the driver. | I |
| DRV-GEN-006 | Bus drivers (UART, I2C, SPI) shall support multiple instances of the same peripheral type. | T |
| DRV-GEN-007 | Device drivers (BME280, SD) shall depend only on the bus driver interface, so they can be unit-tested on the host with a mock bus. | T |
| DRV-GEN-008 | Code shall compile with `-Wall -Wextra -Werror` on arm-none-eabi-gcc with zero warnings. | I |
| DRV-GEN-009 | Each public function shall have a header comment stating parameters, return codes, and blocking behavior. | I |
| DRV-GEN-010 | Interrupt handlers should complete in under 10 µs at the configured core clock. | A |

## GPIO driver

Every other driver configures its pins through this one, so it is built first, alongside the LED blinky.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-GPIO-001 | The driver shall configure mode, output type, speed, pull-up/down, and alternate function for one pin in a single call taking a configuration struct. | T |
| DRV-GPIO-002 | The driver shall support pins 0–15 on every GPIO port present on the LQFP64 package (A, B, C, D, H). | T |
| DRV-GPIO-003 | Invalid parameters (pin > 15, speed > 3, AF > 15, unknown mode, null pointer) shall return BAD\_PARAM without modifying any register. | T |
| DRV-GPIO-004 | Configuring one pin shall leave every other pin's configuration on the same port unchanged. | T |
| DRV-GPIO-005 | The mode register shall be written last, so a pin never drives with stale output type, pull, or AF settings. | I |
| DRV-GPIO-006 | Port clock enable shall be a separate function, called once per port, and shall read back the enable register before returning. | I |
| DRV-GPIO-007 | The driver shall provide pin write, toggle, and read functions. | T |
| DRV-GPIO-008 | Pin writes shall use the BSRR register so they are atomic with respect to interrupts. | I |
| DRV-GPIO-009 | The driver should support edge-triggered interrupts on input pins through EXTI. | T |

**Test notes:** for 004, snapshot all port registers, configure one pin, and diff the snapshots; only that pin's bit fields may change. Use LD2 (PA5) for 007 output checks and the B1 user button (PC13, active low) for input and EXTI checks. Measure toggle rate on the logic analyzer as a sanity check.

## UART driver

The UART driver provides the console and debug log, so it is built first and verified against the ST-Link virtual COM port.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-UART-001 | The driver shall support baud rates of 9600, 115200, and 921600 with 8N1 framing. | T |
| DRV-UART-002 | Actual baud rate shall be within ±2% of the requested rate. | A |
| DRV-UART-003 | The driver shall provide blocking transmit and receive functions with timeouts. | T |
| DRV-UART-004 | The driver shall provide interrupt-driven receive into a ring buffer of configurable size. | T |
| DRV-UART-005 | On ring buffer overflow, the driver shall drop new bytes and increment an overflow counter readable by the caller. | T |
| DRV-UART-006 | The driver shall detect and report framing, noise, and overrun errors. | T |
| DRV-UART-007 | The driver shall support `printf` retargeting for console output. | D |
| DRV-UART-008 | Interrupt-driven receive shall sustain continuous input at 115200 baud for 60 s with zero lost bytes while the main loop is idle. | T |

**Test notes:** test the console (USART2) by echoing bytes typed in a PC terminal (001, 003). Do not jumper PA2 to PA3: the ST-Link drives PA3, so a jumper puts two outputs on one wire. For self-contained loopback tests, run the driver on a second instance (USART1 on PA9/PA10, or USART6 on PC6/PC7) and jumper its TX to RX. Provoke overrun for 006 by delaying the read loop while pasting a long string. Measure bit time on the logic analyzer for 002. For 008, send a known file from the PC and compare a checksum.

## I2C driver

The I2C driver is controller-only with 7-bit addressing, which covers the BME280 and most breakout sensors.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-I2C-001 | The driver shall operate as a bus controller with 7-bit addressing. | T |
| DRV-I2C-002 | The driver shall support Standard mode (100 kHz) and Fast mode (400 kHz). | T |
| DRV-I2C-003 | Measured SCL frequency shall not exceed the configured mode's maximum. | A |
| DRV-I2C-004 | The driver shall provide `write`, `read`, and combined `write_then_read` (repeated start) transfers. | T |
| DRV-I2C-005 | The driver shall provide register helpers: read N bytes from a register, write N bytes to a register. | T |
| DRV-I2C-006 | A transfer to an absent address shall return NACK within the timeout and leave the bus idle. | T |
| DRV-I2C-007 | The driver shall detect a stuck bus (SDA held low) and attempt recovery by clocking SCL up to 9 times, then issuing a STOP. | T |
| DRV-I2C-008 | The driver shall detect and report arbitration loss and bus errors. | T |
| DRV-I2C-009 | The driver shall provide a bus scan that reports every responding address from 0x08 to 0x77. | D |

**Test notes:** for 006, address an unused slot such as 0x50 with the BME280 connected. For 007, hold SDA low with a jumper to GND during a transfer, then release it. Confirm repeated starts in PulseView's I2C decoder for 004.

## SPI driver

The SPI driver is controller-only with software chip select, shared by the BME280 (SPI1) and the SD card (SPI2).

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-SPI-001 | The driver shall operate as a bus controller in full-duplex mode with 8-bit frames. | T |
| DRV-SPI-002 | The driver shall support all four clock modes (CPOL/CPHA 0–3). | A |
| DRV-SPI-003 | The driver shall support selectable clock prescalers, giving at least one rate ≤ 400 kHz and one rate ≥ 8 MHz. | A |
| DRV-SPI-004 | The clock rate shall be changeable after init without re-initializing the peripheral (needed for SD card init). | T |
| DRV-SPI-005 | Chip select shall be driven by GPIO under driver control, with one CS line per attached device. | I |
| DRV-SPI-006 | The driver shall provide a full-duplex `transfer`, plus `write` and `read` that send 0xFF filler bytes. | T |
| DRV-SPI-007 | Transfers of 512 bytes or more shall be able to use DMA, with a completion callback or poll function. | T |
| DRV-SPI-008 | A DMA transfer of 512 bytes at ≥ 8 MHz shall complete within 110% of the ideal time (512 × 8 bit-times). | A |

**Test notes:** jumper MOSI to MISO for loopback tests of 001, 006, and 007; received data shall equal sent data. Verify modes and rates (002, 003) from logic analyzer captures at a slow prescaler. The 24 MHz analyzer cannot resolve 8 MHz bit timing, so verify 008 by toggling a GPIO around the transfer and checking total time against 512 × 8 bit-times.

## BME280 sensor driver

The BME280 driver runs unchanged over either I2C or SPI, which is the main proof that the bus abstraction (DRV-GEN-007) works.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-BME-001 | The driver shall work over I2C or SPI, selected at init, with no other code changes. | T |
| DRV-BME-002 | Init shall read the chip ID and return an error if it is not 0x60. | T |
| DRV-BME-003 | Init shall issue a soft reset and wait for the NVM copy to finish before reading calibration data. | I |
| DRV-BME-004 | The driver shall read all trimming parameters and apply the compensation formulas from the Bosch datasheet. | I |
| DRV-BME-005 | Compensation shall use integer arithmetic only. | I |
| DRV-BME-006 | Compensated output shall match Bosch's reference implementation within ±1 LSB for a fixed set of recorded raw readings. | T |
| DRV-BME-007 | The driver shall support sleep, forced, and normal modes, and configurable oversampling for each measurement. | T |
| DRV-BME-008 | Temperature, pressure, and humidity shall be read in one burst to keep the three values consistent. | A |
| DRV-BME-009 | Readings at room conditions shall be plausible: 10–40 °C, 900–1100 hPa, 10–90 %RH. | D |

**Test notes:** for 006, record raw ADC and calibration bytes once, then run the driver and Bosch's BME280\_SensorAPI on the host against the same inputs. Check for the BMP280 (chip ID 0x58) first, since it is often sold as a BME280.

## microSD driver and filesystem

The SD driver exposes 512-byte block reads and writes over SPI, and FatFs sits on top through its disk I/O layer.

| ID | Requirement | Verify |
| --- | --- | --- |
| DRV-SD-001 | The driver shall initialize SDHC cards in SPI mode: ≥ 74 clocks with CS high, then CMD0, CMD8, ACMD41 loop, CMD58. | A |
| DRV-SD-002 | Initialization shall run at ≤ 400 kHz, then raise the SPI clock to at least 8 MHz. | A |
| DRV-SD-003 | The driver shall reject SDSC and SDXC cards with a distinct error code in v1. | T |
| DRV-SD-004 | The driver shall provide single-block read (CMD17) and write (CMD24) of 512 bytes. | T |
| DRV-SD-005 | Deferred to v2: the driver should provide multi-block read and write (CMD18, CMD25). | T |
| DRV-SD-006 | The driver shall check every R1 response and data token, and report card errors distinctly from timeouts. | T |
| DRV-SD-007 | The driver shall poll the card's busy signal after each write, with a timeout of at least 250 ms. | I |
| DRV-SD-008 | Commands shall carry valid CRC7, and the driver should enable and check CRC16 on data blocks. | T |
| DRV-SD-009 | The driver shall implement the FatFs `diskio` interface (initialize, status, read, write, ioctl sync and sector count). | I |
| DRV-SD-010 | A file written by the board shall open and match byte-for-byte on a PC, and a PC-written file shall be readable by the board. | T |
| DRV-SD-011 | The logging app shall call `f_sync` at a configurable interval, default 1 s, so at most one interval of data is lost on power failure. | T |
| DRV-SD-012 | Sustained sequential write throughput should be at least 100 KB/s. | T |

**Test notes:** capture init at 400 kHz in PulseView's SD card (SPI mode) decoder for 001–002. For 006, pull the card mid-transfer. For 011, cut power at random times (a relay or MOSFET makes this repeatable) and check the log for data loss beyond one interval.

## Verification and traceability

Each requirement is closed by evidence committed to the repo, so a reviewer can trace any ID from spec to code to result.

- **Tests** live in `tests/` and name the IDs they cover in their header, e.g. `// Verifies: DRV-I2C-006, DRV-GEN-003`.
- **Host unit tests** run device drivers (BME280, SD protocol logic) against a mock bus in CI on every push.
- **Hardware tests** run on the Nucleo and print `PASS`/`FAIL <ID>` over the UART console.
- **Analysis evidence** is a PulseView capture (`.sr` file) plus a screenshot in `docs/evidence/<ID>.png`.
- **A traceability matrix** in `docs/traceability.md` lists every ID, its test or evidence, and its status.

## Open items

- [x] Verify the morpho pin numbers in the pin table against UM1724 before wiring the SD card.
- [ ] Check whether the BME280 module exposes CSB and SDO for SPI, or is I2C-only.
- [ ] Check whether the microSD module has a level shifter and regulator (sets 3.3 V vs 5 V supply).
- [x] Multi-block SD transfers (DRV-SD-005) deferred to v2.
- [x] Build system: STM32CubeIDE, Empty project (no HAL).
