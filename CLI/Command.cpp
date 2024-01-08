/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Command.h"

namespace Octopus
{

HashMap<String, PrimaryCommandRegister> PrimaryCommandRegister::s_registers;
HashMap<String, SubcommandRegister> SubcommandRegister::s_registers;

} // namespace Octopus
