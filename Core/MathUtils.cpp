/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "MathUtils.h"

#include <random>

namespace Octopus
{

static std::random_device s_random_device;
static std::mt19937_64 s_random_engine(s_random_device());
static std::uniform_int_distribution<u64> s_distribution;

ResultOr<u64> generate_random_unsigned(u64 closed_min, u64 closed_max)
{
    if (closed_min > closed_max)
        return Result(Result::InvalidParameter);

    const u64 generated_number = s_distribution(s_random_engine);
    const u64 generated_in_range = closed_min + generated_number % (closed_max - closed_min + 1);

    return generated_in_range;
}

} // namespace Octopus
