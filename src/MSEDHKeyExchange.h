/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 The aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */

#ifndef D_MSE_DH_KEY_EXCHANGE_H
#define D_MSE_DH_KEY_EXCHANGE_H

#include <array>
#include <cstddef>

namespace aria2 {

constexpr size_t MSE_DH_PUBLIC_KEY_LENGTH = 96;
constexpr size_t MSE_DH_PRIVATE_KEY_LENGTH = 20;

using MSEDHPublicKey =
    std::array<unsigned char, MSE_DH_PUBLIC_KEY_LENGTH>;
using MSEDHPrivateKey =
    std::array<unsigned char, MSE_DH_PRIVATE_KEY_LENGTH>;

} // namespace aria2

#endif // D_MSE_DH_KEY_EXCHANGE_H
