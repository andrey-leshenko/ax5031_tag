/* For usleep */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SDSPI.h>
#include <ti/drivers/SPI.h>
// #include <ti/drivers/UART.h>
// #include <ti/drivers/Watchdog.h>
#include <ti/drivers/PIN.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>

#include <ti/sysbios/BIOS.h>

#define SPI_CS_PIN PIN_ID(11)

/* Board Header file */
#include <Board.h>

#define AX_PWRMODE				0x2
#define AX_PWRMODE_POWERDOWN	0b0000
#define AX_PWRMODE_VREGON		0b0100
#define AX_PWRMODE_STANDBY		0b0101
#define AX_PWRMODE_SYNTHTX		0b1100
#define AX_PWRMODE_FULLTX		0b1101

int fifo_underflows = 0;
int fifo_overflows = 0;

uint16_t axTXBuffer[64];
uint16_t axRXBuffer[64];

uint8_t transmitted_message[9000 / 8];
uint8_t debugbuff[9000 / 8];
size_t transmitted_message_length;

#include "key_srsha1_972002000555.h"

void handle_spi_error()
{
	while (1);
}

//
// Code for single transactions. Too slow for 1 Mbps transmission.
//

uint16_t doTransaction_ax5031(SPI_Handle handle, uint16_t tx)
{
	SPI_Transaction transaction;
	uint16_t rx;

	transaction.count = 1;
	transaction.rxBuf = &rx;
	transaction.txBuf = &tx;

	if (!SPI_transfer(handle, &transaction))
	{
		handle_spi_error();
	}

	return rx;
}

uint8_t axRead(SPI_Handle handle, uint8_t address)
{
	uint16_t tx = address << 8;
	uint16_t rx = doTransaction_ax5031(handle, tx);
	return rx & 0xFF;
}

void axWrite(SPI_Handle handle, uint8_t address, uint8_t value)
{
	uint16_t tx = (1 << 15) + (address << 8) + value;
	doTransaction_ax5031(handle, tx);
}

//
// Code for batch transmissions.
//

uint16_t axCreateReadFrame(uint8_t address)
{
	return address << 8;
}

uint16_t axCreateWriteFrame(uint8_t address, uint8_t value)
{
	return (1 << 15) + (address << 8) + value;
}


// If turn_pwrmode_fulltx is true - the transmitter will be turned on by the funciton
// after pre-filling the FIFO with data. This will avoid fifo underflows on start.
// The current implementation may read up to 31 bytes after the end of the data.
// This happens because we avoid any if statements inside the loop to be quick enough.
void axTransmit(SPI_Handle handle, uint8_t *data, size_t length, int turn_pwrmode_fulltx)
{
	const uint8_t fifocount_address = 0x35;
	const uint8_t fifocount_mask = 0x3F;

	const uint8_t fifoctrl_address = 0x4;
	const uint8_t fifoctrl_fifo_under_mask = 1 << 4;
	const uint8_t fifoctrl_fifo_over_mask = 1 << 5;
	const uint16_t fifoctrl_real_instr = axCreateReadFrame(fifoctrl_address);
	const uint16_t fifocount_real_instr = axCreateReadFrame(fifocount_address);

	const uint8_t fifodata_address = 0x5;
	const uint8_t fifodata_write_instr = (1 << 7) + fifodata_address;

	const size_t fifo_size = 32;

	SPI_Transaction transaction;
	transaction.txBuf = axTXBuffer;
	transaction.rxBuf = axRXBuffer;
	uint8_t *txBufferBytes = (uint8_t*)axTXBuffer;

	uint8_t fifocount;
	int sent = 0;
	int shifted_overflows = 0;
	int shifted_underflows = 0;
	int failures = 0;

	axRead(handle, fifoctrl_address); // Clear the underflow bit
	fifocount = axRead(handle, fifocount_address) & fifocount_mask;

	// We will place the command to begin transmitting at the end
	// of the expected transaction.
	if (turn_pwrmode_fulltx)
	{
		int message_bytes_sent = fifo_size - fifocount;
		axTXBuffer[message_bytes_sent + 2] = axCreateWriteFrame(AX_PWRMODE, AX_PWRMODE_FULLTX);
		turn_pwrmode_fulltx = 1;
	}

	// for testing
	int repeatRounds = 1;

	while (repeatRounds--)
	{
		sent = 0;
		while (sent < length)
		{
			int message_bytes_sent = fifo_size - fifocount;
			int transaction_size = message_bytes_sent + 2 + turn_pwrmode_fulltx;
			int i;

			// We increase the transaction size by 1, and now we can zero the variable.
			turn_pwrmode_fulltx = 0;

			for (i = 0; i < message_bytes_sent; i++)
			{
				txBufferBytes[(i << 1)] = data[sent + i];
				txBufferBytes[(i << 1) + 1] = fifodata_write_instr; // Assume little endianness
			}

			sent += message_bytes_sent;

			axTXBuffer[message_bytes_sent] = fifoctrl_real_instr;
			axTXBuffer[message_bytes_sent + 1] = fifocount_real_instr;

			transaction.count = transaction_size;

			failures += !SPI_transfer(handle, &transaction);
			failures += transaction_size - transaction.count;

			fifocount = axRXBuffer[message_bytes_sent + 1] & fifocount_mask;
			shifted_underflows += axRXBuffer[message_bytes_sent] & fifoctrl_fifo_under_mask;
			shifted_overflows += axRXBuffer[message_bytes_sent] & fifoctrl_fifo_over_mask;
		}
	}

	if (failures)
	{
		handle_spi_error();
	}

	fifo_underflows += shifted_underflows / fifoctrl_fifo_under_mask;
	fifo_overflows += shifted_overflows / fifoctrl_fifo_over_mask;
}

void startupAndTransmit(SPI_Handle handle)
{
	axWrite(handle, AX_PWRMODE, AX_PWRMODE_STANDBY);

	// 1. Configure FLT and PLLCPI to recommended settings
	// Set BANDSEL to select 433 MHz
	// Set FREQSEL to 1

	{
		uint8_t pllloop_address = 0x2C;
		uint8_t pllloop = axRead(handle, pllloop_address);
		pllloop = (pllloop & (1 << 6)) + 0b00101001;
		axWrite(handle, pllloop_address, pllloop);
	}

	// 2. Set carrier frequency

	{
		double freqCarrier = 433.92;
		double freqOscillator = 16;

		uint32_t transmitterFreq = (uint32_t)((freqCarrier / freqOscillator) * (1 << 24) + 0.5);
		transmitterFreq |= 1; // From programming manual. Something to do with tonal behavior.

		axWrite(handle, 0x23, transmitterFreq & 0xFF);
		axWrite(handle, 0x22, (transmitterFreq >> 8) & 0xFF);
		axWrite(handle, 0x21, (transmitterFreq >> 16) & 0xFF);
		axWrite(handle, 0x20, (transmitterFreq >> 24) & 0xFF);
	}

	// 3. Set TXPWR according to the desired output

	// TODO: Add code for setting the power

	// 4. Fsk Deviation, skipped here

	// 5. Set the bit-rate

	{
		double bitrate = 1e6;
		double freqOscillator = 16e6;

		uint32_t transmitterBitrate = (uint32_t)((bitrate / freqOscillator) * (1 << 24) + 0.5);

		axWrite(handle, 0x33, transmitterBitrate & 0xFF);
		axWrite(handle, 0x32, (transmitterBitrate >> 8) & 0xFF);
		axWrite(handle, 0x31, (transmitterBitrate >> 16) & 0xFF);
	}

	// 6. Set modulation type

	{
		const uint8_t psk_modulation_non_shaped = 0b100;
		//const uint8_t psk_modulation_shaped = 0b101;
		uint8_t modulation_address = 0x10;
		uint8_t modulation = axRead(handle, modulation_address);
		axWrite(handle, modulation_address, (modulation & (1 << 7)) + psk_modulation_non_shaped);
	}

	// 7. Set encoding type to raw

	{
		const uint8_t encoding_address = 0x11;
		uint8_t encoding = axRead(handle, encoding_address);
		axWrite(handle, encoding_address, encoding & (~0xFF));
	}

	// 8. Frame mode

	// Leave default frame mode

	axWrite(handle, AX_PWRMODE, AX_PWRMODE_SYNTHTX);

	// Perform VCO Auto-Ranging

	uint8_t pllranging_address = 0x2D;
	uint8_t pllranging = axRead(handle, pllranging_address);
	axWrite(handle, pllranging_address, pllranging | (1 << 4));

	while (axRead(handle, pllranging_address) & (1 << 4))
	{
		Task_sleep(10);
	}

	pllranging = axRead(handle, pllranging_address);

	if (pllranging & (1 << 5))
	{
		handle_spi_error();
	}

	// axWrite(handle, AX_PWRMODE, AX_PWRMODE_FULLTX);
	const int turn_pwrmode_fulltx = 1;
	axTransmit(handle, transmitted_message, transmitted_message_length, turn_pwrmode_fulltx);

	// Wait for the transmission to finish
	{
		uint8_t fifoctrl;

		do
		{
			fifoctrl = axRead(handle, 0x4);
		} while (!(fifoctrl & (1 << 2)));
	}

	axWrite(handle, AX_PWRMODE, AX_PWRMODE_POWERDOWN);

	// Put a breakpoint here to check for over/underflows
	fifo_underflows = 0;
	fifo_overflows = 0;
}

void createMessage()
{
	const uint8_t preamble = 0xAA;
	const int preamble_length = 4;
	const uint8_t postamble = 0xAA;
	const int postamble_length = 2;

	const int code_length = sizeof(code);

	memset(transmitted_message, preamble, preamble_length);
	memcpy(transmitted_message + preamble_length, code, code_length);
	memset(transmitted_message + preamble_length + code_length, postamble, postamble_length);

	transmitted_message_length = preamble_length + code_length + postamble_length;
}

Semaphore_Handle clockTickSemaphore;

void clockTickCallback(xdc_UArg arg)
{
	Semaphore_post(clockTickSemaphore);
}

void *mainThread(void *arg0)
{
	/* Call driver init functions */
	// GPIO_init();
	// I2C_init();
	// SDSPI_init();
	SPI_init();
	// UART_init();
	// Watchdog_init();

	SPI_Handle handle;
	SPI_Params params;
	Clock_Handle clockHandle;
	Clock_Params clockParams;

	SPI_Params_init(&params);
	params.transferMode = SPI_MODE_BLOCKING;
	params.transferTimeout = 1 * 1000;
	params.mode = SPI_MASTER;
	params.bitRate = 4500 * 1000; // Beware: lower bitrates may cause FIFO underflows
	params.dataSize = 16;
	params.frameFormat = SPI_POL0_PHA0;

	handle = SPI_open(Board_SPI0, &params);

	if (!handle)
	{
		handle_spi_error();
	}

	createMessage();

	uint8_t scratch = axRead(handle, 0x1);
	axWrite(handle, 0x1, ~scratch);
	uint8_t scratch2 = axRead(handle, 0x1);

	clockTickSemaphore = Semaphore_create(0, NULL, NULL);

	Clock_Params_init(&clockParams);
	clockParams.period = 1000 * 1000 / Clock_tickPeriod;

	clockHandle = Clock_create(clockTickCallback, 0, &clockParams, NULL);
	Clock_start(clockHandle);

	while (1)
	{
		Semaphore_pend(clockTickSemaphore, BIOS_WAIT_FOREVER);

		startupAndTransmit(handle);
	}
}
