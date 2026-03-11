/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>      // <-- FÜGE DIESEN HEADER HINZU
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
// I2C Adresse für A2=LOW (Write Command/Data)
#define LCD_ADDR_WRITE_CMD  0x78
#define LCD_ADDR_WRITE_DATA 0x7A

// Display Parameter
#define DISPLAY_WIDTH  160
#define DISPLAY_HEIGHT 64
#define DISPLAY_PAGES  (DISPLAY_HEIGHT / 8) // 8 Pixel pro Seite
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


// Fortlaufender I2C-Scan für Oszilloskop-Debugging
void I2C_ContinuousScan(void) {
    static uint8_t count = 0;
    uint8_t address;
    char buf[64];

    // Alle möglichen I2C-Adressen durchgehen (0x01 bis 0x77)
    for(address = 0x01; address < 0x78; address++) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1,
                                                          (address << 1),
                                                          0, 10);  // Kurzes Timeout

        if(status == HAL_OK) {
            // Gerät gefunden - über UART ausgeben
            sprintf(buf, "0x%02X ", address);
            HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 10);
            count++;
        }
    }

    // Zeilenende nach jedem Scan
    sprintf(buf, "\r\nScan #%d\r\n", ++count);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 10);
}

/* USER CODE END 0 */
// Funktion zum Senden eines einzelnen Bytes über I2C mit Header
// mode: 0 = Command, 1 = Data
static void LCD_SendByte(uint8_t byte, uint8_t mode) {
    uint8_t buffer[2];

    // Header Byte bilden
    // Bit 7: CD (Content: 0=Cmd, 1=Data)
    // Bit 6: RW (Read/Write: 0=Write)
    // Bits 5-0: 0 (Reserved)
    buffer[0] = (mode << 6);

    buffer[1] = byte;

    // Über I2C senden (Blocking Mode)
    // HAL_I2C_Master_Transmit(&hi2c1, Adresse, Buffer, Größe, Timeout)
    // Adresse ist 7-Bit, also 0x78 (nicht 0xF0 wie bei 8-Bit)
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR_WRITE_CMD, buffer, 2, 100) != HAL_OK) {
        // Fehlerbehandlung: Hier könnte man ein LED blinken lassen oder UART nutzen
        // Error_Handler();
    }
}

// Funktion zum Senden eines Datenblocks (z.B. Grafikdaten)
static void LCD_SendDataBlock(uint8_t *data, uint16_t size) {
    uint8_t header[2];
    header[0] = 0x40; // CD=1 (Data), RW=0 (Write)
    header[1] = 0x00; // Dummy Byte (laut Datenblatt bei Read, aber oft nötig für Sync)

    // Header senden
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR_WRITE_DATA, header, 2, 100) != HAL_OK) {
        return;
    }

    // Daten senden
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR_WRITE_DATA, data, size, 100) != HAL_OK) {
        return;
    }
}

// Initialisierung des Displays (Bottom View / 6 Uhr)
void LCD_Init(void) {
    // Kurze Verzögerung nach Power-On (falls nötig)
    HAL_Delay(100);

    // 1. Set LCD Mapping Control (Bottom View: Normal)
    // Kommando: 0xC0 (SEG/COM normal)
    LCD_SendByte(0xC0, 0);

    // 2. Set COM End (Last COM electrode = 103 -> 0xF1)
    // Kommando: 0xF1
    LCD_SendByte(0xF1, 0);

    // 3. Set Display Startline (Line 0)
    // Kommando: 0x40 (LSB) + 0x50 (MSB) ?
    // Laut Tabelle: Set Scroll Line LSB (0x40) und MSB (0x50)
    LCD_SendByte(0x40, 0); // LSB
    LCD_SendByte(0x50, 0); // MSB

    // 4. Set Panel Loading (28..38nF) -> 0x2B
    LCD_SendByte(0x2B, 0);

    // 5. Set LCD Bias Ratio (1/12) -> 0xEB
    LCD_SendByte(0xEB, 0);

    // 6. Set Vbias Potentiometer (Kontrast) -> 0x81 + Wert
    // Wert 0x5F (helle Anzeige)
    LCD_SendByte(0x81, 0);
    LCD_SendByte(0x5F, 0);

    // 7. Set RAM Address Control (Auto-Increment) -> 0x89
    // AC1=1 (Page Increment), AC0=1 (Wrap around)
    LCD_SendByte(0x89, 0);

    // 8. Set Display Enable (Ein) -> 0xAF
    LCD_SendByte(0xAF, 0);

    // Optional: Bildschirm löschen (alle Bytes 0x00)
    // Wir schreiben 160 * 8 = 1280 Bytes? Nein, 160 Spalten * 8 Seiten = 1280 Bytes
    // Aber wir können es einfacher machen: Setze Cursor auf 0,0 und schreibe Nullen
    // Für den Test reicht es oft, einfach ein Muster zu schreiben.
}

// Hilfsfunktion: Setze Cursor (Spalte, Seite)
void LCD_SetCursor(uint8_t col, uint8_t page) {
    // Set Column Address LSB (0x00 + lower 4 bits)
    LCD_SendByte(0x00 | (col & 0x0F), 0);
    // Set Column Address MSB (0x10 + upper 4 bits)
    LCD_SendByte(0x10 | ((col >> 4) & 0x0F), 0);

    // Set Page Address (0x20 + page)
    LCD_SendByte(0x20 | (page & 0x1F), 0);
}

// Einfache Funktion, um ein Rechteck oder Muster zu zeichnen
void LCD_DrawTestPattern(void) {
    uint8_t page = 0;
    uint8_t col = 0;

    // Wir füllen das Display mit einem Muster
    // 160 Spalten, 8 Seiten (64 Pixel hoch)

    for(page = 0; page < 8; page++) {
        LCD_SetCursor(0, page);

        // Datenblock für eine Seite (160 Bytes)
        uint8_t row_data[160];

        for(col = 0; col < 160; col++) {
            // Muster: Wenn Spalte gerade, Pixel an, sonst aus?
            // Oder ein Schachbrettmuster
            if ((col + page) % 2 == 0) {
                row_data[col] = 0xFF; // Alle 4 Pixel an (da 1 Byte = 4 Pixel)
            } else {
                row_data[col] = 0x00; // Alle aus
            }
        }

        // Daten senden
        LCD_SendDataBlock(row_data, 160);
    }
}

// Funktion, um einen einfachen Balken zu zeichnen (z.B. Kontrasttest)
void LCD_DrawProgressBar(void) {
    uint8_t page = 0;
    uint8_t col = 0;

    // Seite 0 füllen
    LCD_SetCursor(0, 0);
    uint8_t bar_data[160];

    for(col = 0; col < 160; col++) {
        if(col < 80) {
            bar_data[col] = 0xFF; // Hälfte voll
        } else {
            bar_data[col] = 0x00;
        }
    }
    LCD_SendDataBlock(bar_data, 160);

    // Seite 1 füllen (leer lassen)
    LCD_SetCursor(0, 1);
    for(col = 0; col < 160; col++) bar_data[col] = 0x00;
    LCD_SendDataBlock(bar_data, 160);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1000);

  /* USER CODE END 2 */

  // Endlosschleife mit fortlaufendem Scan
  while(1)
  {
    /* USER CODE BEGIN WHILE */

    // I2C-Scan durchführen
    I2C_ContinuousScan();

    // Pause zwischen Scans (für Oszilloskop sichtbar)
    // 100ms = 10 Scans pro Sekunde
    // 500ms = 2 Scans pro Sekunde (besser für Oszilloskop)
    HAL_Delay(1000);

    /* USER CODE END WHILE */
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 4;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_MDC_Pin RMII_RXD0_Pin RMII_RXD1_Pin */
  GPIO_InitStruct.Pin = RMII_MDC_Pin|RMII_RXD0_Pin|RMII_RXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_REF_CLK_Pin RMII_MDIO_Pin RMII_CRS_DV_Pin */
  GPIO_InitStruct.Pin = RMII_REF_CLK_Pin|RMII_MDIO_Pin|RMII_CRS_DV_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : RMII_TXD1_Pin */
  GPIO_InitStruct.Pin = RMII_TXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(RMII_TXD1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_TX_EN_Pin RMII_TXD0_Pin */
  GPIO_InitStruct.Pin = RMII_TX_EN_Pin|RMII_TXD0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
