/*
 * Application.cpp
 *
 * Copyright 2021 Akihiro Yamamoto.
 * Licensed under the Apache License, Version 2.0
 * <https://spdx.org/licenses/Apache-2.0.html>
 *
 */
#include <ST7032iLcd.hpp>

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;

static ST7032iLcd i2c_lcd(hi2c1);

using PwmTimerCounts = uint16_t;
constexpr static const PwmTimerCounts PwmPeriod =
    40000; // 40000 counts = 20 milliseconds
constexpr static const PwmTimerCounts ServoAngle0 =
    PwmPeriod * 0.5 / 20.0; // 0.5 milliseconds / 20 milliseconds
constexpr static const PwmTimerCounts ServoAngle180 =
    PwmPeriod * 2.4 / 20.0; // 2.4 milliseconds / 20 milliseconds

// degree: 0 to 180
static void moveTo(uint8_t degree) {
  uint32_t count = ServoAngle0 + (ServoAngle180 - ServoAngle0) * degree / 180;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, count);
  std::string buff(50, ' ');
  std::snprintf(buff.data(), buff.size(), u8"ｶｸﾄﾞ ｢%3d｣ ﾄﾞ", degree);
  i2c_lcd.setDdramAddress(0);
  i2c_lcd.putString(buff);
}

extern "C" void application_setup() {
  i2c_lcd.init();
  moveTo(0);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

extern "C" void application_loop() {
  constexpr static const uint8_t step_angles = 45;
  constexpr static const uint8_t num_of_detents = 1 + 180 / step_angles;
  constexpr static const uint8_t slow_down = 30;
  static uint16_t i = 0;

  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
  HAL_Delay(200);
  moveTo(i / slow_down * step_angles);
  i = (i + 1) % (slow_down * num_of_detents);
}
