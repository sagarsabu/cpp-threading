#pragma once

namespace Sage::Thread
{

enum class EventT
{
    Exit,
    Test
};

struct Event
{
    explicit Event(EventT evenType) : m_evenType{ evenType } { }

    virtual ~Event() = default;

    EventT Type() const { return m_evenType; }

private:
    const EventT m_evenType;
};

} // namespace Sage::Thread

