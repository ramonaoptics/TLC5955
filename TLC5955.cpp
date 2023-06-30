/*
   TLC5955 Control Library
   Used to control the TI TLC5955 LED driver chip
   Zack Phillips - zkphil@berkeley.edu
   Product Page: http://www.ti.com/product/tlc5955
   Copyright (c) 2018, Zachary F. Phillips
   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL Z. PHILLIPS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "TLC5955.h"
#include <SPI.h>
#include <Arduino.h>

/* Bit Quantities (Change to match other TLC driver chips) */
#define GS_BITS 16
#define MC_BITS 3
#define BC_BITS 7
#define DC_BITS 7
#define FC_BITS 5
#define CONTROL_ZERO_BITS 389   /* Bits required for correct control reg size */
#define TOTAL_REGISTER_SIZE 76
#define LATCH_DELAY 1
#define CONTROL_MODE_ON 1
#define CONTROL_MODE_OFF 0


void TLC5955::init(uint8_t gslat, uint8_t spi_mosi, uint8_t spi_clk, uint8_t gsclk)
{

  _gslat = gslat;
  _spi_clk = spi_clk;
  _spi_mosi = spi_mosi;
  _gsclk = gsclk;

  mSettings = SPISettings(spi_baud_rate, MSBFIRST, SPI_MODE0);

  // Initialize SPI library
  SPI.setMOSI(_spi_mosi);
  SPI.begin();

  // Set up latch
  pinMode(_gslat, OUTPUT);
  digitalWrite(_gslat, LOW);

  // set up gsclk
  pinMode(_gsclk, OUTPUT);
  setGsclkFreq(gsclk_frequency);

  // Set default color channel indicies
  setRgbPinOrder(rgb_order_default[0], rgb_order_default[1], rgb_order_default[2]);
}

void TLC5955::setSpiBaudRate(uint32_t new_baud_rate)
{
  // Store old baud rate
  spi_baud_rate = new_baud_rate;

  mSettings = SPISettings(spi_baud_rate, MSBFIRST, SPI_MODE0);
}

uint32_t TLC5955::getSpiBaudRate()
{
  // Return current baud rate
  return spi_baud_rate;
}

void TLC5955::setGsclkFreq(uint32_t new_gsclk_frequency)
{
  // Store previous gsclk frequency
  gsclk_frequency = new_gsclk_frequency;

  analogWriteFrequency(_gsclk, gsclk_frequency);
  analogWriteResolution(1);
  analogWrite(_gsclk, 1);
}

uint32_t TLC5955::getGsclkFreq()
{
  return gsclk_frequency;
}

void TLC5955::setRgbPinOrder(uint8_t rPos, uint8_t grPos, uint8_t bPos)
{
  if (COLOR_CHANNEL_COUNT == 3)
  {
    for (int8_t chip = _tlc_count - 1; chip >= 0; chip--)
    {
      for (int8_t channel = 0; channel < LEDS_PER_CHIP; channel++)
      {
        _rgb_order[chip][channel][0] = rPos;
        _rgb_order[chip][channel][1] = grPos;
        _rgb_order[chip][channel][2] = bPos;
      }
    }
  }
}

void TLC5955::setPinOrderSingle(uint16_t led_number, uint8_t color_channel_index, uint8_t position)
{
  uint8_t chip = (uint16_t)floor(led_number / LEDS_PER_CHIP);
  uint8_t channel = (uint8_t)(led_number - LEDS_PER_CHIP * chip);        // Turn that LED on
  _rgb_order[chip][channel][color_channel_index] = position;
}

void TLC5955::setRgbPinOrderSingle(uint16_t led_number, uint8_t rPos, uint8_t grPos, uint8_t bPos)
{
  uint8_t chip = (uint16_t)floor(led_number / LEDS_PER_CHIP);
  uint8_t channel = (uint8_t)round(led_number - LEDS_PER_CHIP * chip);        // Turn that LED on
  _rgb_order[chip][channel][0] = rPos;
  _rgb_order[chip][channel][1] = grPos;
  _rgb_order[chip][channel][2] = bPos;
}

void TLC5955::setAllLed(uint16_t gsvalue)
{
  for (int8_t chip = _tlc_count - 1; chip >= 0; chip--)
  {
    for (int8_t a = 0; a < LEDS_PER_CHIP; a++)
    {
      for (int8_t b = 0; b < COLOR_CHANNEL_COUNT; b++)
        _grayscale_data[chip][a][b] = gsvalue;
    }
  }
}

void TLC5955::setAllLedRgb(uint16_t red, uint16_t green, uint16_t blue)
{
  if (COLOR_CHANNEL_COUNT == 3)
  {
    for (int8_t chip = _tlc_count - 1; chip >= 0; chip--)
    {
      for (int8_t channel = 0; channel < LEDS_PER_CHIP; channel++)
      {
        _grayscale_data[chip][channel][2] = blue;
        _grayscale_data[chip][channel][1] = green;
        _grayscale_data[chip][channel][0] = red;
      }
    }
  }
}

void TLC5955::setControlModeBit(bool is_control_mode)
{
  // Make sure latch is low
  digitalWrite(_gslat, LOW);

  // Turn off SPI Temporarily
  SPI.end();

  // Enable digital IO
  pinMode(_spi_mosi, OUTPUT);
  pinMode(_spi_clk, OUTPUT);

  // Manually write control sequence
  if (is_control_mode)
  {
    // Manually Write control sequence
    digitalWrite(_spi_mosi, HIGH);          // Set MSB to HIGH
  }
  else
  {
    digitalWrite(_spi_mosi, LOW); // Set MSB to LOW
  }
  digitalWrite(_spi_clk, LOW);                  // Clock
  // Pulse
  digitalWrite(_spi_clk, HIGH);
  digitalWrite(_spi_clk, LOW);

  SPI.begin();
}

int TLC5955::updateLeds(double* output_current)
{

  double power_output_amps = getTotalCurrent();
  if (output_current != nullptr)
  {
    *output_current = power_output_amps;
  }
  if (enforce_max_current && power_output_amps > max_current_amps)
    return 1;

  for (int16_t chip = (int8_t)_tlc_count - 1; chip >= 0; chip--)
  {
    setControlModeBit(CONTROL_MODE_OFF);
    SPI.beginTransaction(mSettings);
    uint8_t color_channel_ordered;
    for (int8_t led_channel_index = (int8_t)LEDS_PER_CHIP - 1; led_channel_index >= 0; led_channel_index--)
    {
      for (int8_t color_channel_index = (int8_t)COLOR_CHANNEL_COUNT - 1; color_channel_index >= 0; color_channel_index--)
      {
        color_channel_ordered = _rgb_order[chip][led_channel_index][(uint8_t) color_channel_index];

        SPI.transfer16(
            _grayscale_data[chip][led_channel_index][color_channel_ordered]
        );
      }
    }
    SPI.endTransaction();
  }

  latch();

  // 0. comparity check is OK since we know all currents are positive
  if (power_output_amps == 0.) {
    analogWrite(_gsclk, 0);
  } else {
    analogWrite(_gsclk, 1);
  }
  return 0;
}

void TLC5955::clearLeds()
{
    for (int16_t chip = (int8_t)_tlc_count - 1; chip >= 0; chip--)
  {
    setControlModeBit(CONTROL_MODE_OFF);
    SPI.beginTransaction(mSettings);
    for (int8_t led_channel_index = (int8_t)LEDS_PER_CHIP - 1; led_channel_index >= 0; led_channel_index--)
    {
      for (int8_t color_channel_index = (int8_t)COLOR_CHANNEL_COUNT - 1; color_channel_index >= 0; color_channel_index--)
      {
        SPI.transfer16(0);
      }
    }
    SPI.endTransaction();
  }

  latch();
}

void TLC5955::setLed(int led_number, uint16_t red, uint16_t green, uint16_t blue)
{
  int chip = led_number / LEDS_PER_CHIP;
  int channel = led_number % LEDS_PER_CHIP;
  _grayscale_data[chip][channel][2] = blue;
  _grayscale_data[chip][channel][1] = green;
  _grayscale_data[chip][channel][0] = red;
}


void TLC5955::setLed(int led_number, uint16_t rgb)
{
  int chip = led_number / LEDS_PER_CHIP;
  int channel = led_number % LEDS_PER_CHIP;
  _grayscale_data[chip][channel][2] = rgb;
  _grayscale_data[chip][channel][1] = rgb;
  _grayscale_data[chip][channel][0] = rgb;

}

// Assume all LEDs are the same for Dot Correction and Grayscale
// Current per LED per channel in A
void TLC5955::getLedCurrents(double* currents, uint16_t* gs)
{
  for (int i = 0; i < 3; i++)
  {
    currents[i] = maxCurrentValues[_MC[i]] * (0.262 + 0.738 * _DC[i] / 127)
              * (0.1 + 0.9 * _BC[i] / 127) * gs[i] / 65535;
  }
}

// Defines functional bits in settings - see datasheet for what
void TLC5955::setFunctionData(bool DSPRPT, bool TMGRST, bool RFRESH, bool ESPWM, bool LSDVLT)
{
  uint8_t data = 0;
  data |= DSPRPT << 0;
  data |= TMGRST << 1;
  data |= RFRESH << 2;
  data |= ESPWM << 3;
  data |= LSDVLT << 4;
  _function_data = data;
}

double TLC5955::getTotalCurrent()
{
  uint32_t totalCurrent_channel[COLOR_CHANNEL_COUNT];
  for (uint8_t color_channel_index = 0; color_channel_index < COLOR_CHANNEL_COUNT; color_channel_index++)
      totalCurrent_channel[color_channel_index] = 0;

  for (uint8_t color_channel_index = 0; color_channel_index < COLOR_CHANNEL_COUNT; color_channel_index++)
  {
    for (uint8_t chip = 0; chip < _tlc_count; chip++)
      for (uint8_t led_channel_index = 0; led_channel_index < LEDS_PER_CHIP; led_channel_index++)
        totalCurrent_channel[color_channel_index] += _grayscale_data[chip][led_channel_index][color_channel_index];
  }
  // Avoid expensive floating point computation and compute the totalCurrent
  // once, after the inexpensive integer computation has been done
  // https://www.ti.com/lit/ds/symlink/tlc5955.pdf
  // Page 17 (Equation 1)
  double totalCurrent = 0;
  for (uint8_t color_channel_index = 0; color_channel_index < COLOR_CHANNEL_COUNT; color_channel_index++){
    double current = maxCurrentValues[_MC[color_channel_index]]
                    * (0.1 + 0.9 * _BC[color_channel_index] / 127)
                    * (0.262 + 0.738 * _DC[color_channel_index] / 127);
    totalCurrent += totalCurrent_channel[color_channel_index] * current / (0xFFFF);
  }
  return totalCurrent;
}

void TLC5955::setMaxCurrent(uint8_t mc)
{
  setMaxCurrent(mc, mc, mc);
}

void TLC5955::setMaxCurrent(uint8_t mcr, uint8_t mcg, uint8_t mcb)
{
  // Ensure max Current agrees with datasheet (3-bit)
  if (mcr > 7)
    mcr = 7;
  _MC[0] = mcr;

  // Ensure max Current agrees with datasheet (3-bit)
  if (mcg > 7)
    mcg = 7;
  _MC[1] = mcg;

  // Ensure max Current agrees with datasheet (3-bit)
  if (mcb > 7)
    mcb = 7;
  _MC[2] = mcb;
}

void TLC5955::getMaxCurrent(uint8_t* maxCurrent)
{
  maxCurrent[0] = _MC[0];
  maxCurrent[1] = _MC[1];
  maxCurrent[2] = _MC[2];
}

// Set Brightness through CURRENT from 10-100% of value set in function mode
void TLC5955::setBrightnessControl(uint8_t bc)
{
  setBrightnessControl(bc, bc, bc);
}

// Set Brightness through CURRENT from 10-100% of value set in function mode
void TLC5955::setBrightnessControl(uint8_t bcr, uint8_t bcg, uint8_t bcb)
{
  if (bcr > 127)
    bcr = 127;
  _BC[0] = bcr;

  if (bcg > 127)
    bcg = 127;
  _BC[1] = bcg;

  if (bcb > 127)
    bcb = 127;
  _BC[2] = bcb;
}

void TLC5955::getBrightnessControl(uint8_t* brightnessControl)
{
  brightnessControl[0] = _BC[0];
  brightnessControl[1] = _BC[1];
  brightnessControl[2] = _BC[2];
}

void TLC5955::setDotCorrection(uint8_t dc)
{
  setDotCorrection(dc, dc, dc);
}

void TLC5955::setDotCorrection(uint8_t dcr, uint8_t dcg, uint8_t dcb)
{
  if (dcr > 127)
    dcr = 127;
  _DC[0] = dcr;

  if (dcg > 127)
    dcg = 127;
  _DC[1] = dcg;

  if (dcb > 127)
    dcb = 127;
  _DC[2] = dcb;
}

void TLC5955::getDotCorrection(uint8_t* dotCorrection)
{
  dotCorrection[0] = _DC[0];
  dotCorrection[1] = _DC[1];
  dotCorrection[2] = _DC[2];
}

#ifdef KINETISK
#define SPI_CTAR_FMSZ_MASK (0xF << 27)
#define SPI_CTAR_FMSZ_MASK_NOT (0xFFFF'FFFF & (~SPI_CTAR_FMSZ_MASK))
#endif

// Update the Control Register (changes settings)
void TLC5955::updateControl(int repeat)
{
  int a;
#ifdef KINETISK
  uint32_t old_ctar0;
#endif

  for (int repeatCtr = 0; repeatCtr < repeat; repeatCtr++)
  {
    for (int chip = _tlc_count - 1; chip >= 0; chip--)
    {
      _buffer_count = 7;
      setControlModeBit(CONTROL_MODE_ON);
      SPI.beginTransaction(mSettings);
#ifdef KINETISK
      // Get the old ctar0 (associated with SPI.transfer
      // after we have setup the transaction
      old_ctar0 = KINETISK_SPI0.CTAR0;
#endif
      // Table 23
      // DC, MC, BC, FC data writes are selected when the MSB[1:9] bits are 96h (HLLHLHHL).
      //            HLLHLHHL
      SPI.transfer(B10010110);

      // Add CONTROL_ZERO_BITS blank bits to get to correct position for DC/FC
      for (a = 0; a + 16 < CONTROL_ZERO_BITS; a = a + 16)
          SPI.transfer16(0);

      for (; a < CONTROL_ZERO_BITS; a++)
        setBuffer(0);
      // 5-bit Function Data
      for (a = FC_BITS - 1; a >= 0; a--)
        setBuffer((_function_data & (1 << a)));
      // Brightness Control Data
      for (a = BC_BITS - 1; a >= 0; a--)
        setBuffer((_BC[2] & (1 << a)));
      for (a = BC_BITS - 1; a >= 0; a--)
        setBuffer((_BC[1] & (1 << a)));
      for (a = BC_BITS - 1; a >= 0; a--)
        setBuffer((_BC[0] & (1 << a)));
      // Maximum Current Data
      for (a = MC_BITS - 1; a >= 0; a--)
        setBuffer((_MC[2] & (1 << a)));
      for (a = MC_BITS - 1; a >= 0; a--)
        setBuffer((_MC[1] & (1 << a)));
      for (a = MC_BITS - 1; a >= 0; a--)
        setBuffer((_MC[0] & (1 << a)));

      // Dot Correction Data
#ifdef KINETISK
      // This optimization only works because the number of bits in the buffer
      // amounts to exactly a multiple of 8 bits
      // Assume we are using SPI0
      // Of the 769 bits to send, there are exactly
      // 48 * 7 bits to send, == 336 == 42 * 8
      // Therefore setBuffer would have flushed the bits in the transaction
      // Send DC_BITS (7) bits at once
      KINETISK_SPI0.CTAR0 = (old_ctar0 & SPI_CTAR_FMSZ_MASK_NOT) | SPI_CTAR_FMSZ(DC_BITS-1);
      for (a = LEDS_PER_CHIP - 1; a >= 0; a--)
      {
        for (int b = COLOR_CHANNEL_COUNT - 1; b >= 0; b--)
        {
          SPI.transfer(_DC[b]);
        }
      }
      KINETISK_SPI0.CTAR0 = old_ctar0;
#else
      for (a = LEDS_PER_CHIP - 1; a >= 0; a--)
      {
        for (int b = COLOR_CHANNEL_COUNT - 1; b >= 0; b--)
        {
          for (int c = 6; c >= 0; c--)
            setBuffer(_DC[b] & (1 << c));
        }
      }
#endif
      SPI.endTransaction();
    }
    latch();
  }
}

void TLC5955::latch()
{
  delayMicroseconds(LATCH_DELAY);
  digitalWrite(_gslat, HIGH);
  delayMicroseconds(LATCH_DELAY);
  digitalWrite(_gslat, LOW);
  delayMicroseconds(LATCH_DELAY);
}

// Get a single channel's current values
uint16_t TLC5955::getChannelValue(uint16_t channel_number, int color_channel_index)
{
  if (color_channel_index >= COLOR_CHANNEL_COUNT)
    return 0;

  uint8_t chip = (uint16_t)floor(channel_number / LEDS_PER_CHIP);
  uint8_t channel = (uint8_t)(channel_number - LEDS_PER_CHIP * chip);
  return _grayscale_data[chip][channel][color_channel_index];
}

// SPI interface - accumulates single bits, then sends over SPI
// interface once we accumulate 8 bits
void TLC5955::setBuffer(uint8_t bit)
{
  bitWrite(_buffer, _buffer_count, bit);
  _buffer_count--;
  if (_buffer_count == -1)
  {
    SPI.transfer(_buffer);
    _buffer_count = 7;
    _buffer = 0;
  }
}
