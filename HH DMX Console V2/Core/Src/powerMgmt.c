/*
 * powerMgmt.c
 *
 *  Created on: May 14, 2020
 *      Author: Kyle
 */

#include "stdint.h"
#include "stdbool.h"
#include "main.h"
#include "oled.h"
#include "cli.h"
#include "ui.h"
#include "eeprom.h"
#include "powerMgmt.h"
#include "usb_iface.h"

extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim15;
extern OPAMP_HandleTypeDef hopamp2;
								//Note that voltage will be lower while loaded down
#define ADCBAT_DIVIDEND 125
#define ADCBAT_MINVOLTAGE 21	//2.00V or less = 0 bars
#define ADCBAT_MAXVOLTAGE 29	//3.00V or more = 8 bars

uint32_t ADCValue = 0;

extern uint8_t presetData[CLI_PRESET_COUNT][512];

enum powerStates
{
	POWER_STATE_NONE,
	POWER_STATE_USB,
	POWER_STATE_BATTERY
} curPowerState;

enum powerButtonStates
{
	POWER_BUTTON_PRESSED_ON,
	POWER_BUTTON_RELEASED_ON,
	POWER_BUTTON_PRESSED_OFF,
} pwrButtonState;

uint8_t curBatteryLevel;
uint8_t finalBatteryLevel;
uint8_t initCount;

void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef * hadc)
{
	HAL_ADC_Stop_IT(&hadc2);

	ADCValue = HAL_ADC_GetValue(&hadc2) / ADCBAT_DIVIDEND;

	if(ADCValue <= 20) POWER_Shutdown();

	if(finalBatteryLevel != 0) ADCValue = ((finalBatteryLevel * 2) + ADCValue) / 3;
	finalBatteryLevel = ADCValue;

	HAL_TIM_Base_Start_IT(&htim15);
}

void POWER_UpdateStatus(enum powerStates newPowerState, uint8_t newBatteryLevel)
{
	if(newPowerState == POWER_STATE_USB && curPowerState != POWER_STATE_USB)
	{
		OLED_DrawPowerSymbolPlug(117, 0);
		OLED_DrawArea(0, 117, 11);
		curPowerState = newPowerState;
	}
	else
	{
		if(newBatteryLevel <= ADCBAT_MINVOLTAGE) newBatteryLevel = 0;
		else if(newBatteryLevel >= ADCBAT_MAXVOLTAGE) newBatteryLevel = 8;
		else newBatteryLevel = (newBatteryLevel - ADCBAT_MINVOLTAGE);
		if(newPowerState == POWER_STATE_BATTERY && (curPowerState != POWER_STATE_BATTERY
				|| newBatteryLevel != curBatteryLevel))
		{
			OLED_DrawPowerSymbolBattery(newBatteryLevel, 117, 0);
			OLED_DrawArea(0, 117, 11);
			curBatteryLevel = newBatteryLevel;
			curPowerState = newPowerState;
		}
	}
}

void POWER_CheckStatus()
{
	HAL_TIM_Base_Stop_IT(&htim15);
	if(HAL_GPIO_ReadPin(GPIOA, USB_VCC_DETECT_Pin))
		POWER_UpdateStatus(POWER_STATE_USB, 0);
	else POWER_UpdateStatus(POWER_STATE_BATTERY, finalBatteryLevel);
	HAL_ADC_Start_IT(&hadc2);
}

void POWER_Init()
{
	HAL_OPAMP_Start(&hopamp2);
	HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
	HAL_TIM_Base_Start_IT(&htim15);
}

void POWER_Shutdown()
{
	if(OLED_IsInitialized())
	{
		CLI_AddToCommand(BtnClear);
		OLED_String("Goodbye", 7, 0, 1);
		OLED_DrawPage(1);
	}
	else
	{
		while(!OLED_IsReady())
			UI_ProcessQueue();
	}

	for(uint8_t i = 0; i < CLI_PRESET_COUNT; i++)
	{
		while(EEPROM_IsBusy())
		    UI_ProcessQueue();

	    EEPROM_WriteBlock(i * 512, presetData[i], 512);
	}
	while(EEPROM_IsBusy())
		  UI_ProcessQueue();
	  HAL_GPIO_WritePin(GPIOA, PWRON_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(GPIOA, PWRON_Pin, GPIO_PIN_RESET);
}

void POWER_CheckPowerButton()
{
	if(!HAL_GPIO_ReadPin(GPIOA, PBSTAT_Pin))		//Button Pressed
	{
		if(pwrButtonState == POWER_BUTTON_RELEASED_ON)
			pwrButtonState = POWER_BUTTON_PRESSED_OFF;
	}
	else											//Button Released
	{
		if(pwrButtonState == POWER_BUTTON_PRESSED_ON)
			pwrButtonState = POWER_BUTTON_RELEASED_ON;
		if(pwrButtonState == POWER_BUTTON_PRESSED_OFF)
			POWER_Shutdown();
	}
}
