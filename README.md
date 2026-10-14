# Read-All-at-Startup
Simplified AUTOSAR-style NvM_ReadAll() for the S32K144: reads data blocks from an M24C04 I2C EEPROM into RAM mirrors, verifies each with CRC-8, and logs failed blocks over UART.
# NvM ReadAll on S32K144 (M24C04 EEPROM)

A simplified, AUTOSAR-inspired Non-Volatile Memory (NvM) demo for the NXP S32K144.
At startup, `NvM_ReadAll()` loops through every registered block, loads it from an
external M24C04 I2C EEPROM into its RAM mirror, checks its CRC, and logs which
blocks failed.

## Features

- Block table with ID, EEPROM address, RAM mirror pointer and length per block
- 3 blocks, each stored as a 4-byte record (3 data bytes + 1 CRC byte)
- CRC-8 integrity check (polynomial 0x07, initial value 0xFF)
- One failed block never stops the pass, so every block is checked
- Failure log: failed block count and IDs, reported over UART
- Optional first-run write (`WRITE_INITIAL_DATA`) to program the EEPROM with sample data

## Hardware

- NXP S32K144 board
- M24C04 EEPROM (512 bytes, 16-byte pages) on LPI2C0
- LPUART output to a serial terminal (PuTTY, 115200 8N1 by default; check your config)

## Project structure

| File | Purpose |
|---|---|
| `main.c` | Calls `NvM_Init()` and `NvM_Run()` |
| `nvm.h` | Configuration defines and public API |
| `nvm.c` | Block table, CRC, EEPROM read/write, `NvM_ReadAll()`, result printing |

## How it works

1. `NvM_Init()` starts the clocks, pins, UART and I2C master.
2. `NvM_Run()` optionally writes sample data with CRCs, then calls `NvM_ReadAll()`.
3. For each block: read the 4-byte record, recompute the CRC over the 3 data bytes,
   and compare it to the stored CRC.
4. Match: the data is copied into the RAM mirror. Mismatch or I2C error: the block ID is logged.
5. The final pass/fail report is printed over UART.

## Example output

    Reading block ID: 1
    Record: 08 05 06 29
    Stored CRC: 29
    Calculated CRC: 29
    CRC OK - BLOCK VALID
    ...
    Failed block count: 0
    ALL BLOCKS VALID

## Usage

1. Build and flash with S32 Design Studio.
2. Set `WRITE_INITIAL_DATA` to `1U` in `nvm.h` for the first run.
3. Set it to `0U` afterwards to read the existing EEPROM contents on each boot.

## Limitations

- Blocks must be below address 0x100 (the A8 address bit is not handled).
- No ROM defaults or redundant block copies.
- Synchronous and blocking, unlike the asynchronous AUTOSAR NvM.
