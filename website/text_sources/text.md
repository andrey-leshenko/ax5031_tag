# AX5031 Prototype Transmitter

## Background

ATLAS is a reverse GPS wildlife localization system developed at Tel Aviv University.
The system makes use of small tags that are attached to wild animals, and that transmit a fixed code every second.
Historically, these tags had a CC1350 like TI MCU with built-in radio, which they used for transmitting FSK modulated signals.

Theoretical research showed that PSK modulation detected by other techniques can give more accurate and noise resistant tracking.
While the CC1350 radio doesn't support PSK modulation, PSK-capble external transmitters are available.
We found the AX5031 transmitter which looked promising. It can transmit PSK, and is controlled via SPI.

## Goal

The goal of this project was to create a prototype ATLAS tag using the CC1350 LaunchPad and AX5031 in PSK transmission mode.
If the prototype performed well in tests, it could be used to further develop the next generation of ATLAS tags.

## Hardware Setup

Prof. Sivan Toledo designed a custom testing circuit board for the AX5031 transmitter.
The board has the necessary SPI connections and antenna circuitry.
It can be easily connected to the CC1350 launchpad. It can be seen in the photo below:

![The Prototype Transmitter (CC1350 Launchpad + AX5031)](images/prototype_small.jpg)

## Basic SPI Communication and Transmission

Connecting to the transmitter using SPI wans't very difficult.
The AX5031 programming manual explains in detail how to read and write its registers,
and documents the use of each register.
It was required to change the SPI pins on the CC1350 from their defaults,
and to enable the chip select pin.

By following the programming manual, the transmitter could be configured and made to transmit *something*.

## Transmitting Fast Enough

Soon enough it turned out that when transmitting at the required symbol rate,
transmissions were taking way more time than expected and that the transmission FIFO was reporting constant underflows.
Even with the transmitter detached, the SPI writes were too slow.
Analysing the SPI lines using an oscilloscope showed that while SPI transactions themselves are at the required speed,
there is a very long delay between different transactions that comes from the SPI driver code.

The solution was to use the SPI driver in batch mode, executing an array of many write transactions at a time.
This modification, and further optimisation of the transmission code solved the fast transmission problem,
and allowed the code to transmit at full speed with no FIFO underflows.

## Analysis Using a Spectrum Analyser

The characteristics of the transmission could be analysed by the Spectrum Analyser that is present at the lab.
This device essentially shows the frequency spectrum of radio signals that go into it.
Such analysis showed that the transmission were at the required frequency,
and that the real transmission power was as configured by the code.
Below is the spectrum of the PSK transmission, as shown by the device:

![Transmission Spectrum (Frequency: 434MHz, Span: 30MHz)](images/spectrum_small.jpg)

## Analysis in MATLAB

The next step was to record some of the transmission, and to analyze them in MATLAB.
The analysis showed that the transmissions fulfill the requirements of ATLAS.
There was a time error in frequency and symbol rate that happens because of frequency errors in the oscillator used by the transmitter,
but that was expected.

![Recorded Signal Alligned with the Expected Signal](images/in_matlab.svg)

## Detection in ATLAS

Some of the new detection and tracking algorithms have been implemented in ATLAS,
and already work with the prototype tag.
As of March 2018 the remaining parts are in active development, and will hopefully be implemented in the near future.

## Summary

The following functionality was implemented:

* Configuring and turning on the SPI driver on the CC1350.
* Reading and writing registers on the AX5031 transmitter by sending commands through SPI.
* Configuring the transmitter and running the full startup sequence.
* Efficiently feeding data into the transmitter while avoiding FIFO underflows.
* Doing the transmission cycle in fixed intervals of 1000 ms.

In addition, MATLAB code for analysis of recording transmission and code for the new tracking methods in ATLAS were written.
The prototype is able to transmit successfully, and the transmissions can be detected by ATLAS tracking code.

This project was done as part of the
[Advanced Computer Systems](https://sivantoledoacademic.wordpress.com/teaching/advanced-computer-systems-fall-2017/)
course (Prof Sivan Toledo, Tel Aviv University).

-Andrey Leshenko
