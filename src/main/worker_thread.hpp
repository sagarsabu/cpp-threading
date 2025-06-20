#pragma once

#include "threading/thread.hpp"

namespace Sage
{

// Worker thread

class WorkerThread final : public Thread
{
public:
    explicit WorkerThread(TimerThread& timerThread);

private:
    void HandleEvent(UniqueThreadEvent threadEvent) override;

private:
    static inline std::atomic<uint> s_id{ 0 };
};
} // namespace Sage
