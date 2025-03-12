#pragma once

#include <functional>
#include <thread>

namespace Sage::ExitHandler
{

using ExitHandle = std::function<void()>;

std::jthread Create(ExitHandle&& theExitHandle);

} // namespace Sage::ExitHandler
