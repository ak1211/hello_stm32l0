/*
 * Application.cpp
 *
 * Copyright 2021 Akihiro Yamamoto.
 * Licensed under the Apache License, Version 2.0
 * <https://spdx.org/licenses/Apache-2.0.html>
 *
 */
#include "main.h"

#include <ST7032iLcd.hpp>
#include <array>
#include <cmath>

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;

static ST7032iLcd i2c_lcd(hi2c1);

using ExcitingACBD = uint8_t;
constexpr const ExcitingACBD ExA = 0b1000;
constexpr const ExcitingACBD ExC = 0b0100;
constexpr const ExcitingACBD ExB = 0b0010;
constexpr const ExcitingACBD ExD = 0b0001;

using HiACBDLoACBD = uint8_t;

static inline HiACBDLoACBD fromExcitingACBD(ExcitingACBD acbd) {
  return (acbd & 0xf) << 4 | (acbd & 0xf);
}

//
static inline void excitingCoil(HiACBDLoACBD hiloACBD) {
  uint32_t odr = GPIOA->ODR;
  if ((odr & 0xff) != hiloACBD) {
    GPIOA->BSRR = (~hiloACBD & 0xff) << 16;
    // clang-format off
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    // 10
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    // 20
    // clang-format on
    GPIOA->BSRR = hiloACBD & 0xff;
  }
}

// turn off all transistor
static inline void turnOffAll() { GPIOA->BSRR = 0xff << 16; }

// short brake = turn ON all lower side switch.
static inline void shortBrake() { excitingCoil(0b00001111); }

// Low A (PA0)
static inline void turnOnLowA() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
}
static inline void turnOffLowA() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
}
// Low C (PA1)
static inline void turnOnLowC() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
}
static inline void turnOffLowC() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
}
// Low B (PA2)
static inline void turnOnLowB() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
}
static inline void turnOffLowB() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
}
// Low D (PA3)
static inline void turnOnLowD() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}
static inline void turnOffLowD() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
}

// High A (PA4)
static inline void turnOnHighA() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}
static inline void turnOffHighA() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}
// High C (PA5)
static inline void turnOnHighC() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}
static inline void turnOffHighC() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}
// High B (PA6)
static inline void turnOnHighB() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}
static inline void turnOffHighB() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}
// High D (PA7)
static inline void turnOnHighD() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
}
static inline void turnOffHighD() {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}

enum class Rotation : int8_t { CW = 1, CCW = -1 };

// Wave drive (one phase on)
static inline int32_t waveDrive(int32_t steps, Rotation r) {
  constexpr static const std::array<ExcitingACBD, 4> pulses{
      ExA, // A
      ExB, // B
      ExC, // C
      ExD, // D
  };
  uint32_t x = (pulses.size() + (steps & 3)) & 3;
  excitingCoil(fromExcitingACBD(pulses[x]));
  return steps + static_cast<int32_t>(r);
}

// Full-step drive (two phases on)
static inline int32_t fullStepDrive(int32_t steps, Rotation r) {
  constexpr static const std::array<ExcitingACBD, 4> pulses{
      ExA | ExB, // AB
      ExB | ExC, // BC
      ExC | ExD, // CD
      ExD | ExA, // DA
  };
  uint32_t x = (pulses.size() + (steps & 3)) & 3;
  excitingCoil(fromExcitingACBD(pulses[x]));
  return steps + static_cast<int32_t>(r);
}

// Half-step drive
static inline int32_t halfStepDrive(int32_t steps, Rotation r) {
  constexpr static const std::array<ExcitingACBD, 8> pulses{
      ExA,       // A
      ExA | ExB, // AB
      ExB,       // B
      ExB | ExC, // BC
      ExC,       // C
      ExC | ExD, // CD
      ExD,       // D
      ExD | ExA, // DA
  };
  uint32_t x = (pulses.size() + (steps & 7)) & 7;
  excitingCoil(fromExcitingACBD(pulses[x]));
  return steps + static_cast<int32_t>(r);
}

extern "C" void application_setup() {
  HAL_Delay(300); // time wait for LCD prepare
  shortBrake();
  //
  i2c_lcd.init();
  std::string buff(50, ' ');
  std::snprintf(buff.data(), buff.size(), u8"ｽﾃｯﾋﾟﾝｸﾞﾓｰﾀｰ ﾃｽﾄ");
  i2c_lcd.setDdramAddress(0);
  i2c_lcd.putString(buff);
  //
  HAL_TIM_Base_Start_IT(&htim2);
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
}

static volatile int32_t stepCounter = 0;
static volatile Rotation rotation = Rotation::CW;

constexpr static const int32_t RightAngle = 400 / 2;

static void showPosition() {
  std::string buff(50, ' ');
  switch (rotation) {
  case Rotation::CW:
    std::snprintf(buff.data(), buff.size(), "CW %+5ld pulses", stepCounter);
    break;
  case Rotation::CCW:
    std::snprintf(buff.data(), buff.size(), "CCW %+5ld pulses", stepCounter);
    break;
  }
  i2c_lcd.setDdramAddress(0x40);
  i2c_lcd.putString(buff);
}

typedef void (*Procedure)();

static void procHomePosition() {
  i2c_lcd.setDdramAddress(0x40);
  i2c_lcd.puts(u8" HOME position. ");
  HAL_Delay(1000);
  HAL_TIM_Base_Start_IT(&htim2);
}
static void procStopPosition() {
  showPosition();
  HAL_Delay(1000);
  HAL_TIM_Base_Start_IT(&htim2);
}
static void procReturnPosition() {
  rotation = (rotation == Rotation::CW) ? Rotation::CCW : Rotation::CW;
  showPosition();
  HAL_Delay(1000);
  HAL_TIM_Base_Start_IT(&htim2);
}

static Procedure getProcedure(int32_t counter) {
  switch (std::abs(counter)) {
  case 0 * RightAngle: // home position
    return procHomePosition;
  case 1 * RightAngle: // 90
    return procStopPosition;
  case 2 * RightAngle: // 180
  case 3 * RightAngle: // 270
  case 4 * RightAngle: // 360
  case 5 * RightAngle: // 450
  case 6 * RightAngle: // 540
  case 7 * RightAngle: // 630
  case 8 * RightAngle: // 720
  case 9 * RightAngle: // 810
    return nullptr;
  case 10 * RightAngle: // 900
    return procReturnPosition;
  default:
    return nullptr;
  }
}

extern "C" void application_loop() {
  if (Procedure p = getProcedure(stepCounter); p != nullptr) {
    (*p)();
  } else {
    showPosition();
    HAL_Delay(1);
  }
}

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == htim2.Instance) {
    stepCounter = halfStepDrive(stepCounter, rotation);
    if (getProcedure(stepCounter) != nullptr) {
      HAL_TIM_Base_Stop_IT(&htim2);
    }
  }
}
