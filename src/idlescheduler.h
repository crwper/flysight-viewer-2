#ifndef IDLESCHEDULER_H
#define IDLESCHEDULER_H

#include <functional>

#include <QObject>
#include <QTimer>
#include <QVector>

namespace FlySight {

struct Progress {
    int remaining;
    int total;
};

using StepFn     = std::function<void()>;
using BoolFn     = std::function<bool()>;
using ProgressFn = std::function<Progress()>;
using CompleteFn = std::function<void(bool cancelled)>;

struct TaskDef {
    int        priority;
    StepFn     step;
    BoolFn     hasWork;
    ProgressFn progress;
    CompleteFn onComplete;
    bool       cancellable = false;
};

using TaskId = int;

class IdleScheduler : public QObject
{
    Q_OBJECT
public:
    explicit IdleScheduler(QObject *parent = nullptr);

    void registerTask(TaskId id, const TaskDef &def);
    void wake();
    void cancel(TaskId id);

signals:
    void activeTaskChanged(int id, bool cancellable);
    void progressChanged(int id, int remaining, int total);
    void schedulerIdle();

private slots:
    void tick();

private:
    struct Entry {
        TaskId  id;
        TaskDef def;
    };

    QTimer          m_timer;
    QVector<Entry>  m_tasks;
    TaskId          m_activeTask = -1;
    bool            m_idle       = true;
};

} // namespace FlySight

#endif // IDLESCHEDULER_H
