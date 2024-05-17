#pragma once

#include <thread>
#include <functional>

namespace Sage::ExitHandler
{

using ExitHandle = std::function<void()>;

void AttachExitHandler(ExitHandle&& theExitHandle);

} // namespace Sage::ExitHandler
