#pragma once

#include <thread>
#include <functional>

namespace Sage::ExitHandler
{

using ExitHandle = std::function<void()>;

void CreateHandler(const ExitHandle&& theExitHandle);

} // namespace Sage::ExitHandler
