/** @file   hal_port.h
 *  @brief  Mock port typedef bindings for dp_hal.
 *
 *  Maps dp::hal type aliases to mock implementations for host testing. */

#ifndef HAL_PORT_H_
#define HAL_PORT_H_

#include "mock_adc.h"
#include "mock_dac.h"
#include "mock_gpio.h"
#include "mock_i2c.h"
#include "mock_spi.h"
#include "mock_timer.h"
#include "mock_uart.h"

namespace dp::hal {

using DpUart = mock::MockUart;
using DpGpioPin = mock::MockGpioPin;
using DpSpiBus = mock::MockSpiBus;
using DpI2cBus = mock::MockI2cBus;
using DpAdc = mock::MockAdc;
using DpDac = mock::MockDac;
using DpTimer = mock::MockTimer;

}  // namespace dp::hal

#endif  // HAL_PORT_H_
