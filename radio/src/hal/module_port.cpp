/*
 * Copyright (C) EdgeTX
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

#include "module_port.h"
#include "dataconstants.h" // MAX_MODULES
#include "myeeprom.h"      // g_eeGeneral

#include <string.h>

extern const etx_module_t* const _modules[];
extern const uint8_t _n_modules;

static etx_module_state_t _module_states[MAX_MODULES];
static uint8_t _module_power;

#if defined(CONFIGURABLE_MODULE_PORT)
// supplemental configurable port
static etx_module_port_t _extra_module_port;

void modulePortConfigExtra(const etx_module_port_t* port)
{
  memcpy(&_extra_module_port, port, sizeof(_extra_module_port));
}
#endif

void modulePortInit()
{
  memset(_module_states, 0, sizeof(_module_states));
  _module_power = 0;

#if defined(CONFIGURABLE_MODULE_PORT)
  memset(&_extra_module_port, 0, sizeof(_extra_module_port));
  _extra_module_port.type = 0xFF; // some invalid type
#endif
}

static void modulePortClear(etx_module_state_t* st)
{
  memset(st, 0, sizeof(etx_module_state_t));
}

static void _init_serial_driver(etx_module_driver_t* d, const etx_module_port_t* port,
                                const etx_serial_init* params)
{
  auto drv = port->drv.serial;
  d->ctx = drv->init(port->hw_def, params);
  d->port = port;

  // S.PORT specific HW settings
  if (port->port == ETX_MOD_PORT_SPORT && params->baudrate >= 400000) {

    // enable over-sampling by 8
    if (drv->setHWOption) drv->setHWOption(d->ctx, ETX_HWOption_OVER8);

    // one-bit
    if (g_eeGeneral.uartSampleMode == UART_SAMPLE_MODE_ONEBIT) {
      if (drv->setHWOption) drv->setHWOption(d->ctx, ETX_HWOption_ONEBIT);
    }
  }

  // setup polarity
  if (port->set_inverted) {
    port->set_inverted(params->polarity == ETX_Pol_Inverted);
  }
}

static void _init_timer_driver(etx_module_driver_t* d, const etx_module_port_t* port,
                               const etx_timer_config_t* cfg)
{
  auto drv = port->drv.timer;
  d->ctx = drv->init(port->hw_def, cfg);
  d->port = port;

  // setup polarity
  if (port->set_inverted) {
    port->set_inverted(false);
  }
}

static bool _match_port(const etx_module_port_t* p, uint8_t type, uint8_t port,
                        uint8_t polarity, bool softserial_fallback)
{
  if (p->type == type && p->port == port) {
    // either polarity matches or can be set
    if (polarity != ETX_Pol_Inverted ||
        p->set_inverted ||
        port == ETX_MOD_PORT_SOFT_INV ||
        port == ETX_MOD_PORT_SPORT_INV) {
      return true;
    }
  }

  // soft-serial fallback (1. USART -> 2. SOFT-SERIAL)
  if (softserial_fallback && polarity == ETX_Pol_Inverted) {
    if (port == ETX_MOD_PORT_UART && p->port == ETX_MOD_PORT_SOFT_INV) {
      return true;
    }

    if (port == ETX_MOD_PORT_SPORT && p->port == ETX_MOD_PORT_SPORT_INV) {
      return true;
    }
  }
  
  return false;
}

static const etx_module_port_t* _find_port(uint8_t module, uint8_t type,
                                           uint8_t port, uint8_t polarity,
                                           bool softserial_fallback)
{
  if (module >= MAX_MODULES || module >= _n_modules || !_modules[module])
    return nullptr;

  uint8_t n_ports = _modules[module]->n_ports;
  const etx_module_port_t* p = _modules[module]->ports;
  const etx_module_port_t* found_port = nullptr;

  while(n_ports > 0) {

    if (_match_port(p, type, port, polarity, softserial_fallback)) {
      found_port = p;
      break;
    }

    ++p; --n_ports;
  }

#if defined(CONFIGURABLE_MODULE_PORT)
  if (!found_port &&
      polarity == ETX_Pol_Normal &&
      module == EXTERNAL_MODULE &&
      _extra_module_port.type == type &&
      _extra_module_port.port == port) {
    found_port = &_extra_module_port;
  }
#endif

  return found_port;
}

const etx_module_t* modulePortGetModuleDescription(uint8_t module)
{
  if (module >= _n_modules) return nullptr;
  return _modules[module];
}

const etx_module_port_t* modulePortFind(uint8_t module, uint8_t type,
                                        uint8_t port, uint8_t polarity)
{
  return _find_port(module, type, port, polarity, false);
}

void modulePortSetPower(uint8_t module, uint8_t enable)
{
  if (module >= _n_modules) return;
  auto mod = _modules[module];
  if (mod && mod->set_pwr) {
    mod->set_pwr(enable);
    if (enable) {
      _module_power |= (1 << module);
    } else {
      _module_power &= ~(1 << module);
    }
  }
}

bool modulePortPowered(uint8_t module)
{
  if (module >= _n_modules) return false;
  return _module_power & (1 << module);
}

etx_module_state_t* modulePortInitSerial(uint8_t module, uint8_t port,
                                         const etx_serial_init* params,
                                         bool softserial_fallback)
{
  const etx_module_port_t* found_port = _find_port(module, ETX_MOD_TYPE_SERIAL,
                                                   port, params->polarity,
                                                   softserial_fallback);
  if (!found_port) return nullptr;

  auto state = &(_module_states[module]);

  const uint8_t duplex = ETX_Dir_TX_RX;
  uint8_t dir = params->direction & duplex;

  if (dir == duplex) {

    // init RX first, in case TX was already done previously
    _init_serial_driver(&state->rx, found_port, params);

    // do not overwrite TX state if it has already been set:
    // -> support using S.PORT in bidir mode
    if (!state->tx.port) {
      state->tx.port = state->rx.port;
      state->tx.ctx = state->rx.ctx;
    }
  } else if (dir == ETX_Dir_TX) {
    _init_serial_driver(&state->tx, found_port, params);
  } else if (dir == ETX_Dir_RX) {
    _init_serial_driver(&state->rx, found_port, params);
  }

  return state;
}

etx_module_state_t* modulePortInitTimer(uint8_t module, uint8_t port,
                                        const etx_timer_config_t* cfg)
{
  const etx_module_port_t* found_port = _find_port(module, ETX_MOD_TYPE_TIMER,
                                                   port, ETX_Pol_Normal, false);
  if (!found_port) return nullptr;

  auto state = &(_module_states[module]);
  _init_timer_driver(&state->tx, found_port, cfg);

  return state;
}

static void _deinit_driver(etx_module_driver_t* d)
{
  auto p = d->port;
  auto ctx = d->ctx;
    
  if (p->type == ETX_MOD_TYPE_SERIAL) {
    auto drv = p->drv.serial;
    drv->deinit(ctx);
  } else if (p->type == ETX_MOD_TYPE_TIMER) {
    auto drv = p->drv.timer;
    drv->deinit(ctx);
  }
}

void modulePortDeInit(etx_module_state_t* st)
{
  if (st->tx.port != nullptr) {
    _deinit_driver(&st->tx);
  }

  if (st->rx.port != nullptr && st->rx.port != st->tx.port) {
    _deinit_driver(&st->rx);
  }

  modulePortClear(st);
}

etx_module_state_t* modulePortGetState(uint8_t module)
{
  if (module >= MAX_MODULES) return nullptr;
  return &(_module_states[module]);
}

uint8_t modulePortGetModule(etx_module_state_t* st)
{
  return st - _module_states;
}
