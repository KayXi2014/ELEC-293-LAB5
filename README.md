# Phasor Voltmeter (C8051F381)

A small project for measuring peak voltages, frequency, period and phase difference (phasor) between two signals using a Silicon Labs C8051F38x microcontroller. The program reads two analog inputs (peak voltages), uses two digital inputs as zero-cross (reference and test) signals to measure half-period and time difference, and displays results on an attached 4-bit HD44780-compatible LCD and over UART (115200 baud).

This repository contains `Phasor.c` — the main program — and is intended for the ELEC-293 lab.
