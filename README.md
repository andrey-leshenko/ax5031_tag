# AX5031 Prototype Transmitter

This project contains code for using the [AX5031](http://www.onsemi.com/PowerSolutions/product.do?id=AX5031)
transmitter together with the Texas Instruments [CC1350 LaunchPad](http://www.ti.com/tool/LAUNCHXL-CC1350) to transmit PSK codes.
The goal was to create a prototype for the next generation of [ATLAS](http://www.tau.ac.il/~stoledo/tags/)
positioning tags.

A photograph of the hardware prototype used can be found in the `docs/` folder.
The transmitter is placed on a custom designed printed board with the necessary antenna circuitry.
The LanuchPad and the transmitter communicate via SPI.

The following functionality was implemented:

* Configuring and turning on the SPI driver on the CC1350.
* Reading and writing registers on the AX5031 transmitter by sending commands through SPI.
* Configuring the transmitter and running the full startup sequence.
* Efficiently feeding data into the transmitter while avoiding FIFO underflows.
* Doing the transmission cycle in fixed intervals of 1000 ms.

The prototype is able to transmit successfully, and the transmissions can be detected by ATLAS tracking code.

This project was done as part of the
[Advanced Computer Systems](https://sivantoledoacademic.wordpress.com/teaching/advanced-computer-systems-fall-2017/)
course (Prof Sivan Toledo, Tel Aviv University).

-Andrey Leshenko
