/*
 * ST7032iLcd.cpp
 *
 * Copyright 2021 Akihiro Yamamoto.
 * Licensed under the Apache License, Version 2.0
 * <https://spdx.org/licenses/Apache-2.0.html>
 *
 */
#include <ST7032iLcd.hpp>

// clang-format off
inline void i2c_lcd_wait_a_moment() {
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    // 10
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    asm("NOP");asm("NOP");asm("NOP");asm("NOP");asm("NOP");
    // 20
}
// clang-format on

bool ST7032iLcd::init(uint8_t contrast) {
  sendCommands({
      0b00111000, // function set
      0b00111001, // function set
      0b00010100, // interval osc
      0b01101100, // follower control
  });
  setContrast(contrast);
  HAL_Delay(200);

  // second step
  sendCommands({
      0b00111000, // function set
      0b00001100, // Display On
      0b00000001, // Clear Display
  });
  HAL_Delay(2);

  return true;
}

void ST7032iLcd::setContrast(uint8_t contrast) {
  uint8_t lo = contrast & 0xf;
  uint8_t hi = (contrast >> 4) & 0x3;
  sendCommands({
      static_cast<uint8_t>(0b01110000 | lo), // contrast Low
      static_cast<uint8_t>(0b01011100 | hi), // contast High/icon/power
  });
}

inline bool Top4bitHigh(uint8_t x) { return (x & 0b11111000) == 0b11110000; }
inline bool Top3bitHigh(uint8_t x) { return (x & 0b11110000) == 0b11100000; }
inline bool Top2bitHigh(uint8_t x) { return (x & 0b11100000) == 0b11000000; }
constexpr static const uint16_t CodePointHankakuKatakanaBegin = 0xff61;
constexpr static const uint16_t CodePointHankakuKatakanaEnd = 0xff9f;
inline bool isHankakuKatakana(uint16_t x) {
  return (CodePointHankakuKatakanaBegin <= x &&
          x <= CodePointHankakuKatakanaEnd);
}

// utf-8 to ascii & sjis kana
void ST7032iLcd::putString(const std::string &s) {
  uint8_t buff[LCD_NUM_OF_ROW_CHARACTERS];
  std::size_t buff_idx = 0;

  for (std::size_t idx = 0;
       buff_idx < LCD_NUM_OF_ROW_CHARACTERS && idx < s.size();) {
    if (Top4bitHigh(s[idx])) {
      // utf8 4-byte encoded character
      // map to '?'
      buff[buff_idx++] = '?';
      idx += 4;
    } else if (Top3bitHigh(s[idx])) {
      // utf8 3-byte encoded character
      uint8_t top4bit = s[idx + 0] & 0x0f;
      uint8_t mid6bit = s[idx + 1] & 0x3f;
      uint8_t low6bit = s[idx + 2] & 0x3f;
      uint16_t cp = (top4bit << 12) | (mid6bit << 6) | (low6bit << 0);
      if (isHankakuKatakana(cp)) {
        // Hankaku katakana
        buff[buff_idx++] = 0b10100001 + (cp - CodePointHankakuKatakanaBegin);
      } else {
        // map to '?'
        buff[buff_idx++] = '?';
      }
      idx += 3;
    } else if (Top2bitHigh(s[idx])) {
      // utf8 2-byte encoded character
      // map to '?'
      buff[buff_idx++] = '?';
      idx += 2;
    } else {
      // utf8 1-byte encoded character
      buff[buff_idx++] = s[idx];
      idx += 1;
    }
  }
  master_transmit(i2c, i2c_address, I2C_LCD_CBYTE_DATA, buff_idx, buff);
}

struct Icon {
  ST7032iLcd::IconCode icon_code;
  uint8_t addr;
  uint8_t bit;
};

static const Icon I2C_LCD_ICON_DATA[13] = {
    {ST7032iLcd::IconA, 0x00, 0b10000}, // S1  ANTENNA
    {ST7032iLcd::IconB, 0x02, 0b10000}, // S11 TEL
    {ST7032iLcd::IconC, 0x04, 0b10000}, // S21
    {ST7032iLcd::IconD, 0x06, 0b10000}, // S31

    {ST7032iLcd::IconE, 0x07, 0b10000}, // S36 UP ARROW
    {ST7032iLcd::IconF, 0x07, 0b01000}, // S37 DOWN ARROW

    {ST7032iLcd::IconG, 0x09, 0b10000}, // S46 LOCKED
    {ST7032iLcd::IconH, 0x0b, 0b10000}, // S56

    {ST7032iLcd::IconI, 0x0d, 0b10000}, // S66 BATTERY LOW
    {ST7032iLcd::IconJ, 0x0d, 0b01000}, // S67 BATTERY MID
    {ST7032iLcd::IconK, 0x0d, 0b00100}, // S68 BATTERY HIGH
    {ST7032iLcd::IconL, 0x0d, 0b00010}, // S69 BATTERY FRAME

    {ST7032iLcd::IconM, 0x0f, 0b10000}, // S76
};

void ST7032iLcd::showIcon(IconCode bitflag) {
  std::array<uint8_t, 16> buff{0};

  for (const Icon &i : I2C_LCD_ICON_DATA) {
    if (bitflag & i.icon_code) {
      buff[i.addr] |= i.bit;
    }
  }
  for (uint8_t i = 0; i < buff.size(); ++i) {
    sendCommands({
        0b00111001,                           // function set
        static_cast<uint8_t>(0b01000000 | i), // set icon address
    });
    sendDatum(buff[i]);
  }
}

void ST7032iLcd::master_transmit(I2C_HandleTypeDef &i2c, uint8_t i2c_address,
                                 CommByte cbyte, size_t size,
                                 const uint8_t *data) {
  if (1 <= size) {
    size_t i;
    std::vector<uint8_t> buff(size * 2);

    for (i = 0; i < (size - 1); ++i) {
      buff[i * 2 + 0] = I2C_LCD_CBYTE_CONTINUATION | cbyte;
      buff[i * 2 + 1] = data[i];
    }
    buff[i * 2 + 0] = cbyte;
    buff[i * 2 + 1] = data[i];
    HAL_I2C_Master_Transmit(&i2c, i2c_address << 1, buff.data(), buff.size(),
                            LCD_TIMEOUT);
    i2c_lcd_wait_a_moment();
  }
}
