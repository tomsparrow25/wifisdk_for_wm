/****************************************************************************//**
 * @file     mc200_gpio.c
 * @brief    This file provides GPIO functions.
 * @version  V1.0.0
 * @date     06-Feb-2013
 * @author   CE Application Team
 *
 * @note
 * Copyright (C) 2012 Marvell Technology Group Ltd. All rights reserved.
 *
 * @par
 * Marvell is supplying this software which provides customers with programming
 * information regarding the products. Marvell has no responsibility or 
 * liability for the use of the software. Marvell not guarantee the correctness 
 * of this software. Marvell reserves the right to make changes in the software 
 * without notification. 
 * 
 *******************************************************************************/

#include "mc200.h"
#include "mc200_driver.h"
#include "mc200_gpio.h"

/** @addtogroup MC200_Periph_Driver
 *  @{
 */

/** @defgroup GPIO GPIO
 *  @brief GPIO driver modules
 *  @{
 */

/** @defgroup GPIO_Private_Type
 *  @{
 */

/*@} end of group GPIO_Private_Type */

/** @defgroup GPIO_Private_Defines
 *  @{
 */
#define PortTotal         ((GPIO_MaxNo >> 5) + 1)

#define PortNum(gpioNo)   (gpioNo >> 5)
#define GpioNum(gpioNo)   (gpioNo & 0x1F)

/*@} end of group GPIO_Private_Defines */

/** @defgroup GPIO_Private_Variables
 *  @{
 */

/*@} end of group GPIO_Private_Variables */

/** @defgroup GPIO_Global_Variables
 *  @{
 */

/*@} end of group GPIO_Global_Variables */

/** @defgroup GPIO_Private_FunctionDeclaration
 *  @{
 */

/*@} end of group GPIO_Private_FunctionDeclaration */

/** @defgroup GPIO_Private_Functions
 *  @{
 */

/*@} end of group GPIO_Private_Functions */

/** @defgroup GPIO_Public_Functions
 *  @{
 */
/****************************************************************************//**
 * @brief      Set one GPIO pin direction 
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 * @param[in]  dir:  Data transfer direction, should be GPIO_INPUT or GPIO_OUTPUT.
 *
 * @return none
 *
 *******************************************************************************/
void GPIO_SetPinDir(GPIO_NO_Type gpioNo, GPIO_Dir_Type dir)
{
  CHECK_PARAM(IS_GPIO_NO(gpioNo));
  CHECK_PARAM(IS_GPIO_DDR(dir));

  /* Set GPIOx data direction */
  if(dir == GPIO_INPUT)
  {
    GPIO->GCDR[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
  }
  else
  {
    GPIO->GSDR[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
  }
}

/****************************************************************************//**
 * @brief      Write one GPIO pin output 
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 * @param[in]  bitVal:  Value to be write to GPIO pin, should be GPIO_LOW or GPIO_HIGH.
 *
 * @return none
 *
 *******************************************************************************/
void GPIO_WritePinOutput(GPIO_NO_Type gpioNo, GPIO_IO_Type bitVal)
{
  CHECK_PARAM(IS_GPIO_NO(gpioNo));
  CHECK_PARAM(IS_GPIO_IO(bitVal));
 
  /* Set GPIOx data value */
  if(bitVal == GPIO_IO_LOW)
  {
    GPIO->GPCR[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
  }
  else
  {
    GPIO->GPSR[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
  }
}

/****************************************************************************//**
 * @brief      Read one GPIO pin level of input or output
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 *
 * @return  Value read from GPIO pin, should be GPIO_IO_LOW or GPIO_IO_HIGH
 *
 *******************************************************************************/
GPIO_IO_Type GPIO_ReadPinLevel(GPIO_NO_Type gpioNo)
{
  uint32_t gpioVal; 

  GPIO_IO_Type retVal;
  
  CHECK_PARAM(IS_GPIO_NO(gpioNo));

  /* Get the GPIO pin value regardless of input and output */ 
  gpioVal = GPIO->GPLR[PortNum(gpioNo)].WORDVAL & (0x01 << GpioNum(gpioNo)); 

  retVal = (gpioVal? GPIO_IO_HIGH : GPIO_IO_LOW);

  return retVal;
}

/****************************************************************************//**
 * @brief      Configurate GPIOx interrupt type
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 * @param[in]  intType: Interrupt type, should be GPIO_INT_RISING_EDGE, 
                        GPIO_INT_FALLING_EDGE, GPIO_INT_BOTH_EDGES or GPIO_INT_DISABLE.
 *             
 * @return none
 *
 *******************************************************************************/
void GPIO_IntConfig(GPIO_NO_Type gpioNo, GPIO_Int_Type intType)
{
  CHECK_PARAM(IS_GPIO_NO(gpioNo));
  CHECK_PARAM(IS_GPIO_INTTYPE(intType));

  /* Enable/Disabe GPIO Rising/Falling interrupt function */  
  switch(intType)
  {
    case GPIO_INT_RISING_EDGE:
      GPIO->GSRER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      GPIO->GCFER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      break;

    case GPIO_INT_FALLING_EDGE:
      GPIO->GCRER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      GPIO->GSFER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      break;

    case GPIO_INT_BOTH_EDGES:
      GPIO->GSRER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      GPIO->GSFER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      break;

    case GPIO_INT_DISABLE:
      GPIO->GCRER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      GPIO->GCFER[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
      break;
      
    default:
      break;
  }
}

/****************************************************************************//**
 * @brief      Mask/Unmask GPIOx interrupt function
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 * @param[in]  intMask: Mask/Unmask GPIO Edge detect, 
 *                      should be MASK or UNMASK.
 *
 * @return none
 *
 *******************************************************************************/
void GPIO_IntMask(GPIO_NO_Type gpioNo, IntMask_Type intMask)
{
  CHECK_PARAM(IS_GPIO_NO(gpioNo));
  CHECK_PARAM(IS_INTMASK(intMask));

  /* Set interrupt mask */
  if(intMask == UNMASK)
  {
    GPIO->APMASK[PortNum(gpioNo)].WORDVAL |= (0x01 << GpioNum(gpioNo));
  }
  else
  {
    GPIO->APMASK[PortNum(gpioNo)].WORDVAL &= ~(0x01 << GpioNum(gpioNo));
  }
}

/****************************************************************************//**
 * @brief      Get GPIOx pin interrupt status
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 *
 * @return  The state value of GPIOx interrupt: RESET or SET.
 *
 *******************************************************************************/
FlagStatus GPIO_GetIntStatus(GPIO_NO_Type gpioNo)
{
  FlagStatus bitStatus = RESET;

  CHECK_PARAM(IS_GPIO_NO(gpioNo));

  if(GPIO->GEDR[PortNum(gpioNo)].WORDVAL & (0x01 << GpioNum(gpioNo)))
  {
    bitStatus = SET;
  }

  return bitStatus;  
}

/****************************************************************************//**
 * @brief      Clear GPIOx pin interrupt 
 *
 * @param[in]  gpioNo:  Select the GPIO pin.
 *
 * @return  none
 *
 *******************************************************************************/
void GPIO_IntClr(GPIO_NO_Type gpioNo)
{
  CHECK_PARAM(IS_GPIO_NO(gpioNo));

  GPIO->GEDR[PortNum(gpioNo)].WORDVAL = (0x01 << GpioNum(gpioNo));
}

/****************************************************************************//**
 * @brief      GPIO interrupt handler 
 *
 * @param   none
 *
 * @return  none
 *
 *******************************************************************************/
void GPIO_IRQHandler(void)
{
  uint32_t i, intStatus[PortTotal];

  for(i = 0; i < PortTotal; i++)
  {
    /* Get current interrupt status */
    intStatus[i] = GPIO->GEDR[i].WORDVAL;

    /* Clear the generated interrupts */
    GPIO->GEDR[i].WORDVAL = intStatus[i];
  }
  
  /* Check which GPIO pin has interrupt */ 
  for(i = GPIO_MinNo; i <= GPIO_MaxNo; i++)
  {
    if(intStatus[PortNum(i)] & (0x01 << GpioNum(i)))
    {
      /* Call interrupt callback function */
      if(intCbfArra[INT_GPIO][i] != NULL)
      {
        intCbfArra[INT_GPIO][i]();
      }
      /* Disable interrupt if interrupt callback is not install */
      else
      {
        GPIO->GCRER[PortNum(i)].WORDVAL = (0x01 << GpioNum(i));
        GPIO->GCFER[PortNum(i)].WORDVAL = (0x01 << GpioNum(i));
      }
    }
  }
}

/*@} end of group GPIO_Public_Functions */

/*@} end of group GPIO_definitions */

/*@} end of group MC200_Periph_Driver */