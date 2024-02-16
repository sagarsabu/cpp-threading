#pragma once

#include <functional>

namespace Sage::ExitHandler
{

using ExitHandlerCallBack = std::function<void()>;

void Setup();

void WaitForExit(const ExitHandlerCallBack&& theExitHandle);

} // namespace Sage::ExitHandler
