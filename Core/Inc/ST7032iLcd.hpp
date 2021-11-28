/*
 * ST7032iLcd.hpp
 *
 * Copyright 2021 Akihiro Yamamoto.
 * Licensed under the Apache License, Version 2.0
 * <https://spdx.org/licenses/Apache-2.0.html>
 *
 */

#ifndef INC_ST7032ILCD_HPP_
#define INC_ST7032ILCD_HPP_
#include "main.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

class ST7032iLcd {
public:
  //
  ST7032iLcd(I2C_HandleTypeDef &h, uint8_t addr = 0x3e)
      : i2c(h), i2c_address(addr) {}
  //
  bool init(uint8_t contrast = 0b101000);
  void setContrast(uint8_t contrast);
  //
  template <std::size_t N>
  void sendCommands(const std::array<uint8_t, N> &cmds) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_COMMAND, N, cmds.data());
  }
  void sendCommands(const std::vector<uint8_t> &cmds) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_COMMAND, cmds.size(),
                    cmds.data());
  }
  void sendCommand(uint8_t cmd) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_COMMAND, 1, &cmd);
  }
  //
  template <std::size_t N> void sendData(const std::array<uint8_t, N> &data) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_DATA, N, data.data());
  }
  void sendData(const std::vector<uint8_t> &data) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_DATA, data.size(),
                    data.data());
  }
  void sendDatum(uint8_t datum) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_DATA, 1, &datum);
  }
  //
  void puts(const char *s) {
    master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_DATA, std::strlen(s),
                    reinterpret_cast<const uint8_t *>(s));
  }
  void putString(const std::string &s);
  //
  using Command = uint8_t;
  const static constexpr Command CmdClearDisplay = 0b00000001;
  const static constexpr Command CmdReturnHome = 0b00000010;
  //
  void setDdramAddress(uint8_t addr) { sendCommand(0x80 | (addr & 0x7f)); }
  //
  using IconCode = uint16_t;
  const static constexpr IconCode IconA = 1 << 12;
  const static constexpr IconCode IconB = 1 << 11;
  const static constexpr IconCode IconC = 1 << 10;
  const static constexpr IconCode IconD = 1 << 9;
  const static constexpr IconCode IconE = 1 << 8;
  const static constexpr IconCode IconF = 1 << 7;
  const static constexpr IconCode IconG = 1 << 6;
  const static constexpr IconCode IconH = 1 << 5;
  const static constexpr IconCode IconI = 1 << 4;
  const static constexpr IconCode IconJ = 1 << 3;
  const static constexpr IconCode IconK = 1 << 2;
  const static constexpr IconCode IconL = 1 << 1;
  const static constexpr IconCode IconM = 1 << 0;
  //
  void showIcon(IconCode bitflag);

private:
  I2C_HandleTypeDef &i2c;
  const uint8_t i2c_address;
  //
  const static constexpr uint8_t LCD_TIMEOUT = 100;
  const static constexpr uint8_t LCD_NUM_OF_ROW_CHARACTERS = 16;
  //
  using CommByte = uint8_t;
  const static constexpr CommByte I2C_LCD_CBYTE_COMMAND = 0x00;
  const static constexpr CommByte I2C_LCD_CBYTE_DATA = 0x40;
  const static constexpr CommByte I2C_LCD_CBYTE_CONTINUATION = 0x80;
  //
  static void master_transmit(I2C_HandleTypeDef &i2c, uint8_t i2c_address,
                              CommByte cbyte, size_t size, const uint8_t *data);
};

#endif /* INC_ST7032ILCD_H_ */
