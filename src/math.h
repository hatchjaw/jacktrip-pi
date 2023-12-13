/**
 * JackTrip client for bare-metal Raspberry Pi
 * Copyright (C) 2023 Thomas Rushton
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JACKTRIP_PI_MATH_H
#define JACKTRIP_PI_MATH_H

#define MATH_PI 3.141592654f
#define MATH_2_PI 6.283185307f

/**
 * Compute the sine of an angle via truncated Taylor Series approximation.
 * @param x Angle, in radians.
 * @return The sine of x.
 * @note
 */
float sin(float x) {
    auto x2 = x * x, x3 = x2 * x, x5 = x3 * x2, x7 = x5 * x2, x9 = x7 * x2;
    return x
           - x3 / 6.f
           + x5 / 120.f
           - x7 / 5040.f
           + x9 / 362880.f;
}

#endif //JACKTRIP_PI_MATH_H
