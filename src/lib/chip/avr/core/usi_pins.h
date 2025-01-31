/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * This file maps the USI_SDA and USI_SCL macros to specific pins, depending
 * on chip type.
 */

#pragma once

#include "core/hardware.h"

#if defined(__AVR_ATtiny24__) | defined(__AVR_ATtiny24A__) | \
    defined(__AVR_ATtiny44__) | defined(__AVR_ATtiny44A__) | \
    defined(__AVR_ATtiny84__) | defined(__AVR_ATtiny84A__)
#define USI_SDA GPIO_A6
#define USI_DO GPIO_A5
#define USI_SCL GPIO_A4
#define USI_START_VECT USI_STR_vect
#define USI_OVF_VECT USI_OVF_vect
#elif defined(__AVR_AT90Tiny2313__) | defined(__AVR_ATTiny2313__)
#define USI_SDA GPIO_B5
#define USI_DO GPIO_B6
#define USI_SCL GPIO_B7
#define USI_START_VECT USI_START_vect
#define USI_OVF_VECT USI_OVERFLOW_vect
#elif defined(__AVR_ATtiny26__) | defined(__AVR_ATtiny25__) | \
    defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define USI_SDA GPIO_B0
#define USI_DO GPIO_B1
#define USI_SCL GPIO_B2
#define USI_START_VECT USI_START_vect
#define USI_OVF_VECT USI_OVF_vect
#else
#error USI pin definitions needed for this chip
#endif
