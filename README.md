# Sailor Hat for Raspberry Pi: Firmware

## Introduction

[Sailor Hat for Raspberry Pi](https://hatlabs.github.io/sh-rpi/)
is a Raspberry Pi power management and CAN bus
controller board for marine and automotive use. 

The Sailor Hat circuit board includes an ATTiny1614 microcontroller (MCU) that,
well, controls the hat functionality. This repository contains the MCU
firmware implementing the Hat control functionality, including the I2C
communications protocol.

## Building

The project is built using PlatformIO (PIO). PlatformIO should automatically
fetch any dependencies and build the project.

## Flashing

# Required Hardware

A simple SerialUPDI programmer seems to be the best bet for flashing the MCU.
As of now (2022-11-20), the `avrdude` version provided by PlatformIO needs to
be updated to support the `serialupdi` protocol. Download and/or install
`avrdude` 7.0 or later and then copy `avrdude` and `avrdude.conf` to the
`~/.platformio/packages/tool-avrdude-megaavr` directory.

# Flashing Operation

Flashing the device is also done using PlatformIO.

The flashing harness is documented at the [Updating the firmware](https://hatlabs.github.io/sh-rpi/pages/software/#updating-the-firmware) section of the documentation.

## Usage

TODO: Document the I2C protocol

