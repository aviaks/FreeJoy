
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"
#include "usbd_customhid.h"
#include "periphery.h"
#include "flash.h"
#include "analog.h"
#include "buttons.h"

#define REPORT_LEN_16			MAX_AXIS_NUM + MAX_BUTTONS_NUM/16
#define REPORT_LEN_8			REPORT_LEN_16 * 2

/* Private variables ---------------------------------------------------------*/
app_config_t config;
volatile uint8_t adc_data_ready = 0;
joy_report_t joy_report;

//uint8_t report_data[64];

bool calibration_started = false;

uint16_t cal_min[8] = {4095};
uint16_t cal_max[8] = {0};

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  The application entry point.
  *
  * @retval None
  */


void CalibrationLoop() {
  GPIO_PinState state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);
  
  if (state) {
    if (!calibration_started) {
      for (int axis=0; axis < 8; axis++) {
        config.axis_config[axis].calib_max = 0;
        config.axis_config[axis].calib_min = 4095;
        config.axis_config[axis].calib_center = AnalogRawGet(axis);
      }
      calibration_started = true;
    }

    for (int axis=0; axis < 8; axis++) {
      if (AnalogRawGet(axis) > config.axis_config[axis].calib_max) {
        config.axis_config[axis].calib_max = AnalogRawGet(axis);
      } else if (AnalogRawGet(axis) < config.axis_config[axis].calib_min) {
        config.axis_config[axis].calib_min = AnalogRawGet(axis);
      }
    }
  } else {
    if (calibration_started) {
      ConfigSet(&config);
      ConfigGet(&config);
      calibration_started = false;
    }
  }
  // if (calibration_started) {
  //   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
  // } else {
  //   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
  // }
}

void BatteryMonitoringLoop() {
  uint16_t bat_voltage = BatteryVoltageGet();

  uint16_t thr1 = 2500;
  uint16_t thr2 = 2370;
  uint16_t deadband = 30;

  if (bat_voltage >= thr1 + deadband) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
  }
  if ((bat_voltage >= thr2 + deadband) && (bat_voltage < thr1 - deadband)) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  } 
  if (bat_voltage < thr2 - deadband) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  }
}


uint32_t som_fan_shutdown_time = 0;

void TempMonitoringLoop() {
  // Android
  // device/rockchip/rk3399/nanopc-t4/pwm_fan.sh
  uint16_t mh_threshold = 1500;
  uint32_t som_enabled_time_ms = 30000;
  uint32_t now = HAL_GetTick();

  uint16_t temp_voltage = TempVoltageGet();

  GPIO_PinState som_wants_fan = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);

  if (som_wants_fan) {
    som_fan_shutdown_time = now + som_enabled_time_ms;
  }

  if (now < som_fan_shutdown_time || temp_voltage > mh_threshold) {
    // Fan ON, high temperature
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
  } else {
    // Fan OFF, normal temperature
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
  }
}


int main(void)
{
	uint32_t millis;
  uint32_t prev_millis1;
  uint32_t prev_millis2;
	
  HAL_Init();
	
	SystemClock_Config();
	
	MX_USB_DEVICE_Init();
	
  // Uncomment ConfigSet for new chips which flashed first time
	// ConfigSet((app_config_t *) &init_config);
	ConfigGet(&config);

	GPIO_Init(&config);
	ADC_Init(&config);

  while (1)
  {
		millis = HAL_GetTick();
		
    // 100 Hz loop
		if (millis - prev_millis1 > 10) {
			prev_millis1 = millis;
			
			joy_report.id = JOY_REPORT_ID;
			
			ButtonsGet(joy_report.button_data);
			AnalogGet(joy_report.axis_data);	

			USBD_CUSTOM_HID_SendReport(	&hUsbDeviceFS, (uint8_t *)&(joy_report.id), sizeof(joy_report)-sizeof(joy_report.dummy));
    }

    // 2 Hz loop
    if (millis - prev_millis2 > 500) {
      prev_millis2 = millis;

      CalibrationLoop();
      // BatteryMonitoringLoop();
      TempMonitoringLoop();
    }
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

