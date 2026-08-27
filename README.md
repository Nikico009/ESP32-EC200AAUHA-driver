# EC200 ESP32 Driver

A modular driver library for controlling **Quectel EC200 series cellular modems** from an **ESP32**.

The project is designed to provide a simple and reusable software interface for modem communication while keeping the implementation as independent from the underlying hardware design as possible.

The library focuses on providing higher-level communication interfaces through reusable modules, allowing applications to use cellular connectivity without having to directly manage the modem's AT command interface.

## Features

* ESP32 driver for Quectel EC200 series cellular modems.
* Modular architecture for different communication services.
* UART-based communication with the EC200 modem.
* Modem initialization and communication management.
* Network and SIM status handling.
* Support for cellular data communication.
* Higher-level communication interfaces such as:

  * TCP
  * UDP
  * HTTP
  * HTTPS
  * Other communication protocols and services as the project evolves.
* Designed to minimize dependencies on the specific hardware implementation.
* Configurable GPIO and UART assignments.
* Intended to be reusable across different ESP32-based hardware designs.

## Hardware Independence

The software is intentionally designed to remain as independent as possible from the physical hardware implementation.

The hardware design associated with this project is subject to a confidentiality agreement and is therefore not included in this repository.

The library only assumes the electrical interface required to communicate with the EC200 modem, such as:

* UART TX/RX
* Modem enable/control signals
* Power-key or equivalent modem control signal
* Optional status or auxiliary GPIOs

The actual GPIO assignments and electrical connections between the ESP32 and the EC200 are defined by the user according to their hardware implementation.

No proprietary hardware schematics, PCB designs, or confidential hardware documentation are required to use or develop the software contained in this repository.

## Architecture

The project is organized as a collection of modular communication libraries built around the EC200 modem.

The intention is to separate the low-level modem communication from higher-level network protocols.

A simplified architecture is:

```text
ESP32 Application
        │
        ▼
Communication Libraries
(TCP / UDP / HTTP / HTTPS / ...)
        │
        ▼
EC200 Driver
        │
        ▼
UART / GPIO Interface
        │
        ▼
Quectel EC200 Modem
        │
        ▼
Cellular Network
```

This architecture allows additional communication protocols and modem features to be added without requiring applications to directly interact with the EC200 AT command interface.

## Project Structure

The repository is organized into independent modules, with each library providing a specific communication interface or modem functionality.

Examples of planned functionality include:

```text
EC200 ESP32 Driver
│
├── Core modem communication
│
├── TCP
├── UDP
├── HTTP
├── HTTPS
│
└── Additional modem services
```

The exact set of modules may evolve as the project develops.

## Requirements

The project is intended for use with:

* ESP32-based microcontrollers.
* Espressif ESP-IDF.
* A compatible Quectel EC200 series modem.
* UART communication between the ESP32 and the modem.
* Appropriate power and control signals for the selected EC200 hardware.

The specific ESP32 GPIO assignments are configurable and depend on the hardware implementation.

## Hardware Configuration

Hardware-specific definitions such as UART peripherals and GPIO assignments are kept configurable within the driver.

For example, the application may define:

```c
#define UART_PORT UART_NUM_1

#define TXD_MICRO GPIO_NUM_17
#define RXD_MICRO GPIO_NUM_16

#define EN_MDM    GPIO_NUM_14
#define PWR_KEY   GPIO_NUM_13
```

These values are examples only and are not requirements of the library.

Users should configure the GPIO assignments according to their own hardware design.

## Development Philosophy

The project aims to provide a clean abstraction between the ESP32 application and the EC200 modem.

Applications should be able to perform common network operations through the library without having to manually construct and parse low-level AT commands whenever possible.

The architecture is intended to remain:

* Modular
* Reusable
* Hardware-independent
* Lightweight
* Easy to integrate
* Easy to extend

New communication protocols and modem capabilities can be implemented as independent modules while sharing the underlying EC200 communication layer.

## Project Status

This project is under active development.

The repository is intended to evolve into a general-purpose ESP32 software interface for Quectel EC200 cellular modems, with support for multiple communication protocols and modem services.

Stable releases are published using semantic versioning.

The project version is defined at repository/release level rather than individually for each communication module.

## Contributing

Contributions, bug reports and improvements are welcome.

If you find a problem or have an idea for improving the library, feel free to open an issue or submit a pull request.

When contributing, please keep the modular architecture and hardware-independent design principles of the project in mind.

## License

This project is released under the **MIT License**.

See [`LICENSE`](LICENSE) for the complete license text.

## Author

This project was originally created and developed by **Nicolás Comba**.

Copyright (c) 2026 Nicolás Comba.
