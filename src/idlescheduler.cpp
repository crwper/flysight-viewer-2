#include "idlescheduler.h"

#include <algorithm>

namespace FlySight {

IdleScheduler::IdleScheduler(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &IdleScheduler::tick);
}

void IdleScheduler::registerTask(TaskId id, const TaskDef &def)
{
    Entry entry{id, def};

    // Insert in priority-sorted order (ascending — lowest number = highest priority)
    auto it = std::lower_bound(m_tasks.begin(), m_tasks.end(), entry,
        [](const Entry &a, const Entry &b) {
            return a.def.priority < b.def.priority;
        });
    m_tasks.insert(it, entry);
}

void IdleScheduler::wake()
{
    if (!m_timer.isActive())
        m_timer.start();
}

void IdleScheduler::cancel(TaskId id)
{
    for (auto &entry : m_tasks) {
        if (entry.id == id) {
            if (entry.def.onComplete)
                entry.def.onComplete(true);

            if (m_activeTask == id)
                m_activeTask = -1;

            wake();
            return;
        }
    }
}

void IdleScheduler::tick()
{
    // Find the highest-priority task with work
    Entry *found = nullptr;
    for (auto &entry : m_tasks) {
        if (entry.def.hasWork && entry.def.hasWork()) {
            found = &entry;
            break;
        }
    }

    // No work — go idle
    if (!found) {
        if (!m_idle) {
            m_idle = true;
            m_activeTask = -1;
            emit schedulerIdle();
        }
        return;
    }

    // Task switch detection
    if (found->id != m_activeTask) {
        m_activeTask = found->id;
        m_idle = false;
        emit activeTaskChanged(found->id, found->def.cancellable);

        if (found->def.progress) {
            Progress p = found->def.progress();
            emit progressChanged(found->id, p.remaining, p.total);
        }
    }

    // Execute one step
    if (found->def.step)
        found->def.step();

    // Report progress after step
    if (found->def.progress) {
        Progress p = found->def.progress();
        emit progressChanged(found->id, p.remaining, p.total);
    }

    // Check completion
    if (found->def.hasWork && !found->def.hasWork()) {
        if (found->def.onComplete)
            found->def.onComplete(false);
        m_activeTask = -1;
    }

    // Re-arm for next tick
    m_timer.start();
}

} // namespace FlySight
