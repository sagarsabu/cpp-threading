#pragma once

namespace Sage::Thread
{

enum EventT
{
    Exit = 0,
    ManagerStart = 1000
};

struct Event
{
    virtual ~Event() = default;

    int Type() const { return m_evenType; }

protected:
    explicit Event(int evenType) : m_evenType{ evenType } { }

private:
    const int m_evenType;
};

struct ExitEvent final : public Event
{
    ExitEvent() :
        Event{ EventT::Exit }
    { }
};


} // namespace Sage::Thread

