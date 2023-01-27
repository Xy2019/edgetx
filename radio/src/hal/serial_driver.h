/*
 * Copyright (C) EdgeTx
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <stdint.h>


enum SerialEncoding {
  ETX_Encoding_8N1,
  ETX_Encoding_8E2,
  ETX_Encoding_PXX1_PWM,
};

enum SerialDirection {
  ETX_Dir_None = 0,
  ETX_Dir_RX = 1,
  ETX_Dir_TX = 2,
  ETX_Dir_TX_RX = 3,
};

typedef struct {
  uint32_t baudrate;    // = 0;
  uint8_t encoding;     // = ETX_Encoding_8N1;
  uint8_t direction;    // = ETX_Dir_None;

} etx_serial_init;

struct etx_serial_callbacks_t {
  uint8_t (*on_send)(uint8_t* data);
  void (*on_receive)(uint8_t data);
  void (*on_idle)();
  void (*on_error)();
};

typedef struct {

  // Init serial communication
  void* (*init)(void* hw_def, const etx_serial_init* params);

  // De-Init serial communication
  void (*deinit)(void* ctx);

  // Send a single byte
  void (*sendByte)(void* ctx, uint8_t byte);

  // Send a buffer
  void (*sendBuffer)(void* ctx, const uint8_t* data, uint32_t size);

  // Is TX phase completed
  uint8_t (*txCompleted)(void* ctx);
  
  // Wait for last send operation to complete
  void (*waitForTxCompleted)(void* ctx);
  
  // 2-wire half-duplex
  void (*enableRx)(void* ctx);
  
  // Fetch byte from internal buffer
  int (*getByte)(void* ctx, uint8_t* data);

  // Clear internal buffer
  void (*clearRxBuffer)(void* ctx);

  // Get current baudrate
  uint32_t (*getBaudrate)(void*);

  // Set baudrate
  void (*setBaudrate)(void*, uint32_t baudrate);
  
  // Callbacks
  void (*setReceiveCb)(void* ctx, void (*on_receive)(uint8_t*, uint32_t));
  void (*setIdleCb)(void* ctx, void (*on_idle)());
  void (*setBaudrateCb)(void* ctx, void (*on_set_baudrate)(uint32_t));

} etx_serial_driver_t;
