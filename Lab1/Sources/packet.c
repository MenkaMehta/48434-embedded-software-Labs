/*
 * packet.c
 *
 *  Created on: 25 Mar 2017
 *      Author: 98119910
 */
/*!
 ** @file packet.c
 ** @project Lab1
 ** @version 1.0
 ** @compiler GNU C Compiler
 ** @processor MK70FN1M0VMJ12
 ** @authors
 ** 	   Corey Stidston 98119910
 ** 	   Menka Mehta 12195032
 ** @brief
 **         packet module.
 **         This module contains the code for managing incoming and outgoing packets
 ** @date 29th March 2017
 */
/*!
 **  @addtogroup packet_module packet documentation
 **  @{
 */
/* MODULE packet */

/****************************************HEADER FILES****************************************************/
#include "packet.h"
#include "UART.h"
#include "MK70F12.h"

/****************************************GLOBAL VARS*****************************************************/
uint8_t Packet_Command, 				/*!< The packet's command */
			Packet_Parameter1, 			/*!< The packet's 1st parameter */
			Packet_Parameter2, 			/*!< The packet's 2nd parameter */
			Packet_Parameter3,		 	/*!< The packet's 3rd parameter */
			Packet_Checksum; 			/*!< The packet's checksum */

uint8_t packet_position = 0;	//Used to mark the position of incoming bytes

const uint8_t PACKET_ACK_MASK = 0x80u; //Used to mask out the Acknowledgment bit

uint8_t TowerNumberLsb = (SID & 0x00FF); 		//LSB of Tower Number
uint8_t TowerNumberMsb = (SID & 0xFF00) >> 8; 	//MSB of Tower Number

/****************************************PRIVATE FUNCTION DECLARATION***********************************/

bool PacketTest(void);

/****************************************PUBLIC FUNCTION DEFINITION***************************************/

/*! @brief Initializes the packets by calling the initialization routines of the supporting software modules.
 *
 *  @param baudRate The desired baud rate in bits/sec.
 *  @param moduleClk The module clock rate in Hz
 *  @return bool - TRUE if the packet module was successfully initialized.
 */
bool Packet_Init(const uint32_t baudRate, const uint32_t moduleClk)
{
	return UART_Init(baudRate, moduleClk);
}

/*! @brief Attempts to get a packet from the received data.
 *
 *  @return bool - TRUE if a valid packet was received.
 */
bool Packet_Get(void) {
	uint8_t uartData;

	//Checks whether there is data in the RxFIFO and stores it the address pointed by uartData
	if (UART_InChar(&uartData))
	{
		switch (packet_position)
		{
			//Command byte
			case 0:
				Packet_Command = uartData;
				packet_position++;
				return false; //Return false, incomplete packet
				break;

				//Parameter1 byte
			case 1:
				Packet_Parameter1 = uartData;
				packet_position++;
				return false; //Return false, incomplete packet
				break;

				//Parameter2 byte
			case 2:
				Packet_Parameter2 = uartData;
				packet_position++;
				return false; //Return false, incomplete packet
				break;

				//Parameter3 byte
			case 3:
				Packet_Parameter3 = uartData;
				packet_position++;
				return false; //Return false, incomplete packet
				break;

				//Checksum byte
			case 4:
				Packet_Checksum = uartData;

				if (PacketTest())
				{
					packet_position = 0;
					return true; //Return true, complete packet
				}

				//The Checksum doesn't match
				//Shift the packets down
				Packet_Command = Packet_Parameter1;
				Packet_Parameter1 = Packet_Parameter2;
				Packet_Parameter2 = Packet_Parameter3;
				Packet_Parameter3 = Packet_Checksum;
				packet_position = 0;
				return false;
				break;

			default: //reset
				packet_position = 0;
				break;
		}
	}
	return false;
}

/*! @brief Builds a packet and places it in the transmit FIFO buffer.
 *
 *  @return bool - TRUE if a valid packet was sent.
 */
bool Packet_Put(const uint8_t command, const uint8_t parameter1, const uint8_t parameter2, const uint8_t parameter3)
{
	if (!UART_OutChar(command))
		return false;				//Place Command byte in TxFIFO
	if (!UART_OutChar(parameter1))
		return false;			//Place Parameter1 byte in TxFIFO
	if (!UART_OutChar(parameter2))
		return false;			//Place Parameter2 byte in TxFIFO
	if (!UART_OutChar(parameter3))
		return false;			//Place Parameter3 byte in TxFIFO
	if (!UART_OutChar(command ^ parameter1 ^ parameter2 ^ parameter3))
		return false;	//Place Checksum byte in TxFIFO
	return true;
}

/*! @brief Handles the stored packet
 *
 *  @return void
 */
void Packet_Handle(void)
{
	bool error = true; //used to determine whether an error has occurred

	//Mask out the Acknowledgment Bit from the Packet Command so that it can be processed
	switch (Packet_Command & ~PACKET_ACK_MASK)
	{

		case GET_STARTUP_VAL:
			//Place the Tower Startup packet in the TxFIFO
			if (Packet_Put(TOWER_STARTUP_COMM, TOWER_STARTUP_PAR1, TOWER_STARTUP_PAR2, TOWER_STARTUP_PAR3))
			{

				//Place the Tower Version packet in the TxFIFO
				if (Packet_Put(TOWER_VERSION_COMM, TOWER_VERSION_V, TOWER_VERSION_MAJ, TOWER_VERSION_MIN))
				{

					//Place the Tower Number packet in the TxFIFO
					if (Packet_Put(TOWER_NUMBER_COMM, TOWER_NUMBER_PAR1, towerNumberLsb, towerNumberMsb))
					{
						error = false;
					}
				}
			}
			break;

		case GET_VERSION:
			//Place the Tower Version packet in the TxFIFO
			if (Packet_Put(TOWER_VERSION_COMM, TOWER_VERSION_V, TOWER_VERSION_MAJ, TOWER_VERSION_MIN))
			{
				error = false;
			}
			break;

		case TOWER_NUMBER:

			if (Packet_Parameter1 == TOWER_NUMBER_GET)
			{
				//Sub-Command: Get the Tower Number

				//Place the Tower Number packet in the TxFIFO
				if (Packet_Put(TOWER_NUMBER_COMM, TOWER_NUMBER_PAR1, towerNumberLsb, towerNumberMsb))
				{
					error = false;
				}
			} else if (Packet_Parameter1 == TOWER_NUMBER_SET)
			{
				//Sub-Command: Set the Tower Number
				towerNumberLsb = Packet_Parameter2;
				towerNumberMsb = Packet_Parameter3;
				error = false;
			}
			break;

		default:
			break;
	}

	//Check whether the Acknowledgment bit is set
	if (Packet_Command & PACKET_ACK_MASK)
	{
		//Create a new command response packet
		uint8_t maskedPacket = 0;

		if (error == true)
		{
			//If there are errors, the Acknowledgment bit should be 0
			maskedPacket = Packet_Command & ~PACKET_ACK_MASK;
		} else
		{
			//If there are no errors, the Acknowledgment bit should be 1
			maskedPacket = Packet_Command | PACKET_ACK_MASK;
		}

		//Place the Acknowledgment Packet in the TxFIFO
		Packet_Put(maskedPacket, Packet_Parameter1, Packet_Parameter2, Packet_Parameter3);
	}
}

/****************************************PRIVATE FUNCTION DEFINITION***************************************/

/*! @brief Handles the stored packet
 *
 *  @return bool - True if the calculated checksum is equal to the packet checksum
 */
bool PacketTest(void)
{
	uint8_t calculated_checksum = Packet_Command ^ Packet_Parameter1 ^ Packet_Parameter2 ^ Packet_Parameter3;
	return (calculated_checksum == Packet_Checksum);
}

/* END packet */
/*!
 ** @}
 */
