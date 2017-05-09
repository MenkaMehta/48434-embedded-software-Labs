/*! @file
 *
 *  @brief I/O routines for the K70 I2C interface.
 *
 *  This contains the functions for operating the I2C (inter-integrated circuit) module.
 *
 *  @author Corey Stidston and Menka Mehta
 *  @date 2017-05-08
 */

 #include "I2C.h"
#include "MK70F12.h"
#include "PE_Types.h"
#include "Cpu.h"
#include "stdlib.h"
#include "types.h"

//Definitions
#define I2C_D_READ  0x01 //from datasheet figure 11
#define I2C_D_WRITE 0x00 //from datasheet figure 11


static void* ReadCompleteUserArgumentsGlobal;
   /*!< Private global pointer to the user arguments to use with the user callback function */
static void (*ReadCompleteCallbackGlobal)(void *);
/*!< Private global pointer to data ready user callback function */

static char SlaveAddress;          /*!< Private global variable to store 8-bit slave address */
 /*! @brief Sets up the I2C before first use.
  *
  *  @param aI2CModule is a structure containing the operating conditions for the module.
  *  @param moduleClk The module clock in Hz.
  *  @return BOOL - TRUE if the I2C module was successfully initialized.
  */
 BOOL I2C_Init(const TI2CModule* const aI2CModule, const uint32_t moduleClk)
 {
   ReadCompleteUserArgumentsGlobal = aI2CModule->readCompleteCallbackArguments;
    // userArguments made globally(private) accessible
   ReadCompleteCallbackGlobal = aI2CModule->readCompleteCallbackFunction;
   // userFunction made globally(private) accessible

  //arrays store multiplier and scl divider values
   const static uint8_t mult[] = { 1, 2, 4 };
   const static uint16_t scl[] = { 20, 22, 24, 26, 28, 30, 34, 40, 28, 32, 36, 40, 44, 48,
       56, 68, 48, 56, 64, 72, 80, 88, 104, 128, 80, 96, 112, 128, 144, 160, 192,
       240, 160, 192, 224, 256, 288, 320, 384, 480, 320, 384, 448, 512, 576, 640,
       768, 960, 640, 768, 896, 1024, 1152, 1280, 1536, 1920, 1280, 1536, 1792,
       2048, 2304, 2560, 3072, 3840 };

  uint8_t i, //counter for looping through scl dividerarray -scldivcount
            j, // counter for looping through multplier array - mulcount
          multiplier, //selected count value of multiplier
          sclDivider, //selected count value of scl divider
  uint32_t baudRateError = 100000; // baudrate should be close to 100kbps lab4 requirement
  uint32_t selectedBaudRate;   //baudRate of current calculation

   //enables clocks
   SIM_SCGC4 |= SIM_SCGC4_IIC0_MASK; //pg 352/2275 k70 manual
	 SIM_SCGC5 |= SIM_SCGC5_PORTE_MASK; // enable pin routing port E

//Schematics-  accelerometer pg9/11 on the right PTE18 and 19
// mux page 286/2275 k70 and ODE is open drain enable pg 325/2275
  PORTE_PCR18 = PORT_PCR_MUX(0x4) | PORT_PCR_ODE_MASK;
	PORTE_PCR19 = PORT_PCR_MUX(0x4) | PORT_PCR_ODE_MASK;

//loop to find baudrate pg 1870
for (i=0; i <(sizeof(scl)/sizeof(uint16_t))-1; i++)
{
  for (j=0; j< (sizeof(mult)/sizeof(uint8_t))-1; j++)
  {
    if (abs(selectedBaudRate -  aI2CModule ->BaudRate) < baudRateError) //check if the baudRate is closer to 100kbps
    {
      selectedBaudRate =moduleClk / mult[j] * scl[i]; //calculate baudRate
      baudRateError = abs(selectedBaudRate - a12CModule->baudRate); //calculate difference between baudrates
      multiplier = j;
      sclDivider = i;
    }
  }
}
//set baudrate pg1870 k70
  I2C0_F |= I2C_F_ICR(sclDivider);
  I2C0_F |= I2C_F_MULT(mult[multiplier]);

 // Clear the control register
	I2C0_C1 = 0;

// enable I2C register pg 1871/2275
 I2C0_C1 |= I2C_C1_IICEN_MASK;

 // I have to enable interrupt as well
 // Enable interrupt
	I2C0_C1 |= I2C_C1_IICIE_MASK;

  //12c programmable input glitch filter registers
  I2C0_FLT = I2C0_FLT_FLT(0x00);
  //clear the control register c2
  //I2C0_C2 = 0; //not sure


//NVICS page 91/2275 k70 manual
// IRQ = 24 mod 32 = 24
// NVICICPR0 = NVIC_ICPR_CLRPEND(1 << 24); // Clear any pending interrupts on I2C0
// NVICISER0 = NVIC_ISER_SETENA(1 << 24); // Enable interrupts on I2C0
NVICICPR1 = (1<<24);                   	// Clears pending interrupts on I2C0 module
NVICISER1 = (1<<24);                   	// Enables interrupts on I2C0 module

return true;
 }

 /*! @brief Selects the current slave device
  *
  * @param slaveAddress The slave device address.
  */
 void I2C_SelectSlaveDevice(const uint8_t slaveAddress)
 {
   SlaveAddress = slaveAddress; //store the slave address globally(private)
 }

  /*! @brief Write a byte of data to a specified register
   *
   * @param registerAddress The register address.
   * @param data The 8-bit data to write.
   */
 void I2C_Write(const uint8_t registerAddress, const uint8_t data)
 {
   //page 1872 k70 manual
   while((I2C0_S & I2C_S_BUSY_MASK) == I2C_S_BUSY_MASK) // Wait till bus is idle
   {

   }

  //clear interrupt
 	I2C0_S |= I2C_S_IICIF_MASK;

  //enable i2c and interrupts
  I2C0_C1 |= I2C_C1_IICEN_MASK | I2C_C1_IICIE_MASK;

//start
 12C0_C1 |= 12C_C1_MST_MASK; //master mode selected, start signal
 12C0_C1 |= 12C_C1_TX_MASK; //transmit mode selected

 //TXAK Transmit Acknowledge Enable ?????
 //RSTA Repeat Start. ????

 //Arbitration pg 1874/2275 k70

//slave adresses I2C Data I/O register (i2Cx_D) pg 1875/2275 k70 manual
 I2C0_D = (SlaveAddress << 1)  | I2C_D_WRITE; // Send slave address with write bit //not sure???

//Wait
while(!((I2C0_S & 12C_S_IICIF_MASK)==I2C_S_IICIF_MASK))
{

}
//set the interrupt flag pg 1874 k70 manual
I2C0_S |= 12C_S_IICIF_MASK;

I2C0_D = registerAddress; //send slave register address

//Wait for ack
while(!((I2C0_S & 12C_S_IICIF_MASK)==I2C_S_IICIF_MASK))
{

}
//set the interrupt flag pg 1874 k70 manual
I2C0_S |= 12C_S_IICIF_MASK;

 I2C0_D = data; // Write data

 //Wait
 while(!((I2C0_S & 12C_S_IICIF_MASK)==I2C_S_IICIF_MASK))
 {

 }
 //set the interrupt flag pg 1874 k70 manual
 I2C0_S |= 12C_S_IICIF_MASK;

 //stop //diasble the following.
I2C0_C1 &= ~(I2C_C1_IICIE_MASK | I2C_C1_MST_MASK | I2C_C1_TX_MASK);

 }

 /*! @brief Reads data of a specified length starting from a specified register
  *
  * Uses polling as the method of data reception.
  * @param registerAddress The register address.
  * @param data A pointer to store the bytes that are read.
  * @param nbBytes The number of bytes to read.
  */
 void I2C_PollRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
 {

 }

 /*! @brief Reads data of a specified length starting from a specified register
  *
  * Uses interrupts as the method of data reception.
  * @param registerAddress The register address.
  * @param data A pointer to store the bytes that are read.
  * @param nbBytes The number of bytes to read.
  */
 void I2C_IntRead(const uint8_t registerAddress, uint8_t* const data, const uint8_t nbBytes)
 {

 }

 /*! @brief Interrupt service routine for the I2C.
  *
  *  Only used for reading data.
  *  At the end of reception, the user callback function will be called.
  *  @note Assumes the I2C module has been initialized.
  */
 void __attribute__ ((interrupt)) I2C_ISR(void)
 {

 }