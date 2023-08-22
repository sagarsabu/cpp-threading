#pragma once

namespace Sage::Log
{

[[gnu::format(printf, 1, 2)]]
void info(const char* msg, ...);

} // namespace Sage::Log
