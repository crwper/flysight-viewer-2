#ifndef PLUGINHOST_H
#define PLUGINHOST_H

#include <QString>
#include <memory>

namespace FlySight {
class SessionData;
}
namespace pybind11 {
class scoped_interpreter;
}

class PluginHost {
public:
    // Singleton accessor
    static PluginHost& instance();

    // Call once—before you register your built-in CalculatedValues,
    // but after Qt application is up.
    // `pluginDir` is where your .py files live (e.g. "<exe>/plugins")
    void initialise(const QString& pluginDir);

private:
    PluginHost() = default;
    std::unique_ptr<pybind11::scoped_interpreter> m_interp;
};

#endif // PLUGINHOST_H
