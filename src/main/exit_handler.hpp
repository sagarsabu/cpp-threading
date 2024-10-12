#pragma once

#include <thread>
#include <functional>

namespace Sage::ExitHandler
{

using ExitHandle = std::function<void()>;

std::jthread Create(ExitHandle&& theExitHandle);

} // namespace Sage::ExitHandler
