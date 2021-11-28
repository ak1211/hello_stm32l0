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

static ST7032iLcd i2c_lcd(hi2c1);

extern "C" void application_setup() {
  static const char *lines[] = {u8"Strawberry Linux", u8"I2C ｴｷｼｮｳ ﾓｼﾞｭｰﾙ"};
  i2c_lcd.init();
  i2c_lcd.puts(lines[0]);
  i2c_lcd.setDdramAddress(0x40);
  i2c_lcd.putString(lines[1]);
}

extern "C" void application_loop() {
  static ST7032iLcd::IconCode i = 0;

  i2c_lcd.showIcon(0x1fff ^ (1 << i));
  i = (i + 1) % 13;

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  HAL_Delay(200);
}
