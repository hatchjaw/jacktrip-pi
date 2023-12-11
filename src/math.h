//
// Created by tar on 11/12/23.
//

#ifndef JACKTRIP_PI_MATH_H
#define JACKTRIP_PI_MATH_H

float sin(float x) {
    auto x2 = x * x, x3 = x2 * x, x5 = x3 * x2, x7 = x5 * x2, x9 = x7 * x2;
    return x
           - x3 / 6.f
           + x5 / 120.f
           - x7 / 5040.f
           + x9 / 362880.f;
}

#endif //JACKTRIP_PI_MATH_H
