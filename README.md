# DHT11 Sensor Reader for MSP430FR2355


This repository contains a C project for the Texas Instruments MSP430FR2355 microcontroller to read temperature and humidity data from a DHT11 sensor. The data is then transmitted over UART, and the reading interval can be controlled via serial commands.

## Features

*   Reads temperature and humidity from a DHT11 sensor.
*   Transmits sensor data over UART at a 38400 baud rate.
*   Utilizes hardware interrupts from push buttons to trigger temperature or humidity display.
*   Configurable data reading interval via UART commands.
*   Clock system configured to run at 4MHz using the DCO.
*   Includes a software DCO trim function for improved clock accuracy.

## Hardware Requirements

*   Texas Instruments MSP430FR2355 LaunchPad™ Development Kit
*   DHT11 Temperature and Humidity Sensor
*   (2) Push buttons for interrupts
*   A serial-to-USB converter or a PC with a serial port to view UART output.

## Software Requirements

*   **IDE**: [Texas Instruments Code Composer Studio (CCS) v12.5.0](https://www.ti.com/tool/CCSTUDIO) or compatible.
*   **Compiler**: `ti-cgt-msp430_21.6.1.LTS` or a compatible version of the TI MSP430 compiler.

## Pinout and Connections

| MSP430FR2355 Pin | Connection      | Purpose                               |
| ---------------- | --------------- | ------------------------------------- |
| `P1.3`           | DHT11 Data Pin  | Data line for the DHT11 sensor        |
| `P4.3`           | UART TX         | Transmit data to a serial monitor     |
| `P4.2`           | UART RX         | Receive commands from a serial monitor|
| `P4.1`           | Push Button     | Interrupt to request temperature reading |
| `P2.3`           | Push Button     | Interrupt to request humidity reading    |
| `VCC`            | Sensor VCC      | 3.3V Power                            |
| `GND`            | Sensor GND      | Ground                                |

## How to Use

1.  **Clone the repository:**
    ```sh
    git clone https://github.com/andreitoma59/DHT11-reader-MSP430.git
    ```

2.  **Build and Flash:**
    *   Import the project into Code Composer Studio.
    *   Build the project to generate the `.out` file.
    *   Flash the generated firmware to your MSP430FR2355 LaunchPad.

3.  **Connect to Serial Monitor:**
    *   Connect your UART-to-USB adapter to the specified TX/RX pins.
    *   Open a serial terminal application (e.g., PuTTY, Tera Term, `screen`).
    *   Configure the serial connection with the following settings:
        *   **Baud Rate**: 38400
        *   **Data Bits**: 8
        *   **Parity**: None
        *   **Stop Bits**: 1

4.  **Operation:**
    *   The application starts automatically after flashing. In the serial monitor, you will see the message `Starting DHT11 read...` at regular intervals.
    *   To display the temperature, press the button connected to `P4.1`. The temperature will be printed on the next successful sensor read.
    *   To display the humidity, press the button connected to `P2.3`. The humidity will be printed on the next successful sensor read.
    *   If no button is pressed, no sensor data is printed.

### UART Commands

You can send commands through the serial terminal to change the sensor reading interval:

*   `t1`: Sets the delay between sensor readings to 1 second. The device will respond with `Delay set to 1 second`.
*   `t2`: Sets the delay between sensor readings to 2 seconds. The device will respond with `Delay set to 2 seconds`.

The default delay is 5 seconds.

## Code Overview

*   **`main.c`**: The main application file. It contains all the logic for peripheral initialization, sensor communication, and interrupt handling.
    *   `initClock()`, `initGPIO()`, `initUART()`, `initTB0()`: Functions to set up the necessary microcontroller peripherals.
    *   `dht_read()`: Implements the single-wire communication protocol to read 40 bits of data from the DHT11 sensor.
    *   `sendUART()`: A simple function for transmitting character strings over UART.
    *   `Port_2()` and `Port_4()` ISRs: Handle button presses to set flags for displaying humidity and temperature.
    *   `USCI_A1_ISR()`: Handles incoming UART data for command processing.
    *   `main()`: Contains the main application loop that periodically attempts to read the sensor and prints data based on user requests.
*   **`lnk_msp430fr2355.cmd`**: The linker command file provided by Texas Instruments, which defines the memory map and section placement for the MSP430FR2355.