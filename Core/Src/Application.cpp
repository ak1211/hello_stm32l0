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

// H-brigde pin class
template <GPIO_TypeDef *PORT(), uint32_t PIN> class HbridgePin {
public:
  static GPIO_PinState state() {
    if (((PORT()->ODR) & PIN) == 0) {
      return GPIO_PIN_RESET;
    } else {
      return GPIO_PIN_SET;
    }
  }
  static void turnOn() { HAL_GPIO_WritePin(PORT(), PIN, GPIO_PIN_SET); }
  static void turnOff() { HAL_GPIO_WritePin(PORT(), PIN, GPIO_PIN_RESET); }
};

// two H-brigdes port declaration
constexpr GPIO_TypeDef *HbHighA_GPIO_Port() { return HighA_GPIO_Port; }
constexpr GPIO_TypeDef *HbHighC_GPIO_Port() { return HighC_GPIO_Port; }
constexpr GPIO_TypeDef *HbHighB_GPIO_Port() { return HighB_GPIO_Port; }
constexpr GPIO_TypeDef *HbHighD_GPIO_Port() { return HighD_GPIO_Port; }
constexpr GPIO_TypeDef *HbLowA_GPIO_Port() { return LowA_GPIO_Port; }
constexpr GPIO_TypeDef *HbLowC_GPIO_Port() { return LowC_GPIO_Port; }
constexpr GPIO_TypeDef *HbLowB_GPIO_Port() { return LowB_GPIO_Port; }
constexpr GPIO_TypeDef *HbLowD_GPIO_Port() { return LowD_GPIO_Port; }

// two H-brigdes pins declaration
using HbPinHighA = HbridgePin<HbHighA_GPIO_Port, HighA_Pin>;
using HbPinHighC = HbridgePin<HbHighC_GPIO_Port, HighC_Pin>;
using HbPinHighB = HbridgePin<HbHighB_GPIO_Port, HighB_Pin>;
using HbPinHighD = HbridgePin<HbHighD_GPIO_Port, HighD_Pin>;
using HbPinLowA = HbridgePin<HbLowA_GPIO_Port, LowA_Pin>;
using HbPinLowC = HbridgePin<HbLowC_GPIO_Port, LowC_Pin>;
using HbPinLowB = HbridgePin<HbLowB_GPIO_Port, LowB_Pin>;
using HbPinLowD = HbridgePin<HbLowD_GPIO_Port, LowD_Pin>;

using ExcitingACBD = uint8_t;
constexpr ExcitingACBD ExA = 0b1000;
constexpr ExcitingACBD ExC = 0b0100;
constexpr ExcitingACBD ExB = 0b0010;
constexpr ExcitingACBD ExD = 0b0001;

using HiACBDLoACBD = uint8_t;
static inline HiACBDLoACBD fromExcitingACBD(ExcitingACBD acbd) {
  return (acbd & 0xf) << 4 | (acbd & 0xf);
}
//
static inline void excitingCoil(HiACBDLoACBD hiloACBD) {
  {
    ExcitingACBD hi =
        (HbPinHighA::state() ? ExA : 0) | (HbPinHighC::state() ? ExC : 0) |
        (HbPinHighB::state() ? ExB : 0) | (HbPinHighD::state() ? ExD : 0);
    ExcitingACBD lo =
        (HbPinLowA::state() ? ExA : 0) | (HbPinLowC::state() ? ExC : 0) |
        (HbPinLowB::state() ? ExB : 0) | (HbPinLowD::state() ? ExD : 0);
    if (((hi << 4) | lo) == hiloACBD) {
      return;
    }
  }
  // clang-format off
  //
  if (!(hiloACBD & (ExA << 4))) { HbPinHighA::turnOff(); }
  if (!(hiloACBD & (ExC << 4))) { HbPinHighC::turnOff(); }
  if (!(hiloACBD & (ExB << 4))) { HbPinHighB::turnOff(); }
  if (!(hiloACBD & (ExD << 4))) { HbPinHighD::turnOff(); }
  if (!(hiloACBD & ExA)) { HbPinLowA::turnOff(); }
  if (!(hiloACBD & ExC)) { HbPinLowC::turnOff(); }
  if (!(hiloACBD & ExB)) { HbPinLowB::turnOff(); }
  if (!(hiloACBD & ExD)) { HbPinLowD::turnOff(); }
  asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
  asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
  // 10
  asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
  asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
  // 20
  //
  if (hiloACBD & (ExA << 4)) { HbPinHighA::turnOn(); }
  if (hiloACBD & (ExC << 4)) { HbPinHighC::turnOn(); }
  if (hiloACBD & (ExB << 4)) { HbPinHighB::turnOn(); }
  if (hiloACBD & (ExD << 4)) { HbPinHighD::turnOn(); }
  if (hiloACBD & ExA) { HbPinLowA::turnOn(); }
  if (hiloACBD & ExC) { HbPinLowC::turnOn(); }
  if (hiloACBD & ExB) { HbPinLowB::turnOn(); }
  if (hiloACBD & ExD) { HbPinLowD::turnOn(); }
  // clang-format on
}

// short brake = turn ON all lower side switch.
static inline void shortBrake() { excitingCoil(0b00001111); }

//
enum class Rotation : int8_t { CW = -1, CCW = 1 };

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
  int8_t sign = (stepCounter == 0) ? 0 : ((stepCounter < 0) ? (-1) : 1);
  switch (sign) {
  case 0:
    std::snprintf(buff.data(), buff.size(), u8" HOME position. ");
    break;
  case static_cast<int8_t>(Rotation::CW):
    std::snprintf(buff.data(), buff.size(), u8"CW %5ld pulses",
                  std::abs(stepCounter));
    break;
  case static_cast<int8_t>(Rotation::CCW):
    std::snprintf(buff.data(), buff.size(), u8"CCW %5ld pulses",
                  std::abs(stepCounter));
    break;
  default:
    break;
  }
  i2c_lcd.setDdramAddress(0x40);
  i2c_lcd.putString(buff);
}

typedef void (*Procedure)();

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
