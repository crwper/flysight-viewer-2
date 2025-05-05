#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#pragma pop_macro("slots")

#include "pluginhost.h"
#include "sessiondata.h"
#include "dependencykey.h"
#include "plotregistry.h"

#include <optional>
#include <string>
#include <QDir>
#include <QDebug>
#include <QVariant>

namespace py = pybind11;
using namespace FlySight;

PluginHost& PluginHost::instance() {
    static PluginHost inst;
    return inst;
}

void PluginHost::initialise(const QString& pluginDir) {
    if (!m_interp) {
        qInfo() << "[PluginHost] Booting embedded Python";
        m_interp = std::make_unique<py::scoped_interpreter>();
    }

    // 1) Add pluginDir to Python sys.path
    py::module sys = py::module::import("sys");
    sys.attr("path").attr("insert")(0, pluginDir.toStdString().c_str());

    // 2) Import our SDK module
    py::module sdk;
    try {
        sdk = py::module_::import("flysight_plugin_sdk");
    } catch (const py::error_already_set &e) {
        qCritical() << "Failed to import flysight_plugin_sdk:" << e.what();
        return;
    }

    // 3) Load every .py file in pluginDir
    QDir dir(pluginDir);
    QStringList filters;
    filters << "*.py";
    for (const QFileInfo& fi : dir.entryInfoList(filters, QDir::Files)) {
        const std::string mod = fi.baseName().toStdString();
        try {
            py::module::import(mod.c_str());
        } catch (py::error_already_set& e) {
            qWarning() << "[PluginHost] Failed to import"
                       << fi.fileName() << ":" << e.what();
        }
    }

    // 4) Register Attribute plugins
    for (py::handle h : sdk.attr("_attributes")) {
        py::object plugin = h.cast<py::object>();
        QString key = QString::fromStdString(
            plugin.attr("name").cast<std::string>());

        // Build dependency list
        QList<DependencyKey> deps;
        try {
            // call the Python inputs() and force it into a list
            py::object raw = plugin.attr("inputs")();
            py::list  inputs = raw.cast<py::list>();
            // iterate the list
            for (py::handle h : inputs) {
                // h is a Python DependencyKey instance; extract its fields
                py::object dk = h.cast<py::object>();
                int kind = dk.attr("kind").cast<int>();  // enum value

                if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                    // attributeKey
                    std::string name = dk.attr("attributeKey").cast<std::string>();
                    deps.append(DependencyKey::attribute(QString::fromStdString(name)));
                } else {
                    // measurement: sensorKey + measurementKey
                    std::string sensor = dk.attr("sensorKey").cast<std::string>();
                    std::string meas   = dk.attr("measurementKey").cast<std::string>();
                    deps.append(DependencyKey::measurement(
                        QString::fromStdString(sensor),
                        QString::fromStdString(meas)));
                }
            }
        } catch (const py::cast_error &e) {
            qWarning() << "[PluginHost] inputs() did not return a list of DependencyKey:";
            qWarning() << e.what();
        } catch (const py::error_already_set &e) {
            qWarning() << "[PluginHost] exception in inputs():";
            qWarning() << e.what();
        }

        // Register into SessionData
        SessionData::registerCalculatedAttribute(
            key, deps,
            [plugin](SessionData& session)
            -> std::optional<QVariant>
            {
                py::gil_scoped_acquire gil;
                py::object out = plugin
                                     .attr("compute")(py::cast(&session,
                                                               py::return_value_policy::reference));
                // if Python returned None => no result
                if (out.is_none()) {
                    return std::nullopt;
                }
                if (py::isinstance<py::float_>(out) ||
                    py::isinstance<py::int_>(out))
                {
                    return QVariant::fromValue(
                        out.cast<double>());
                }
                if (py::isinstance<py::str>(out)) {
                    QString s = QString::fromStdString(out.cast<std::string>());
                    // try to parse an ISO timestamp into QDateTime
                    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
                    if (dt.isValid()) {
                        return QVariant::fromValue(dt);
                    } else {
                        // fallback to plain string
                        return QVariant::fromValue(s);
                    }
                }
                return std::nullopt;
            }
            );
    }

    // 5) Register Measurement plugins
    for (py::handle h : sdk.attr("_measurements")) {
        py::object plugin = h.cast<py::object>();
        QString sensor = QString::fromStdString(
            plugin.attr("sensor").cast<std::string>());
        QString name   = QString::fromStdString(
            plugin.attr("name").cast<std::string>());

        // Build dependency list
        QList<DependencyKey> deps;
        try {
            // call the Python inputs() and force it into a list
            py::object raw = plugin.attr("inputs")();
            py::list  inputs = raw.cast<py::list>();
            // iterate the list
            for (py::handle h : inputs) {
                // h is a Python DependencyKey instance; extract its fields
                py::object dk = h.cast<py::object>();
                int kind = dk.attr("kind").cast<int>();  // enum value

                if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                    // attributeKey
                    std::string name = dk.attr("attributeKey").cast<std::string>();
                    deps.append(DependencyKey::attribute(QString::fromStdString(name)));
                } else {
                    // measurement: sensorKey + measurementKey
                    std::string sensor = dk.attr("sensorKey").cast<std::string>();
                    std::string meas   = dk.attr("measurementKey").cast<std::string>();
                    deps.append(DependencyKey::measurement(
                        QString::fromStdString(sensor),
                        QString::fromStdString(meas)));
                }
            }
        } catch (const py::cast_error &e) {
            qWarning() << "[PluginHost] inputs() did not return a list of DependencyKey:";
            qWarning() << e.what();
        } catch (const py::error_already_set &e) {
            qWarning() << "[PluginHost] exception in inputs():";
            qWarning() << e.what();
        }

        SessionData::registerCalculatedMeasurement(
            sensor, name, deps,
            [plugin](SessionData& session)
            -> std::optional<QVector<double>>
            {
                py::gil_scoped_acquire gil;
                py::object out = plugin
                                     .attr("compute")(py::cast(&session,
                                                               py::return_value_policy::reference));
                // if Python returned None => no result
                if (out.is_none()) {
                    return std::nullopt;
                }
                auto buf = out.cast<
                    py::array_t<double,
                                py::array::c_style |
                                    py::array::forcecast>>();
                auto ptr = buf.data();
                int n = static_cast<int>(buf.shape(0));
                return QVector<double>(ptr, ptr + n);
            }
            );
    }

    // 6) register any plugin-provided “simple plots”
    for (py::handle h : sdk.attr("_simple_plots")) {
        py::object plt = h.cast<py::object>();

        // Extract each attribute
        auto category    = QString::fromStdString(plt.attr("category").cast<std::string>());
        auto name        = QString::fromStdString(plt.attr("name").cast<std::string>());

        // units may be None
        QString units;
        if (!plt.attr("units").is_none())
            units = QString::fromStdString(plt.attr("units").cast<std::string>());

        // color is a CSS string we can feed directly to QColor
        auto colorStr = plt.attr("color").cast<std::string>();
        QColor color(QString::fromStdString(colorStr));

        auto sensor      = QString::fromStdString(plt.attr("sensor").cast<std::string>());
        auto measurement = QString::fromStdString(plt.attr("measurement").cast<std::string>());

        PlotRegistry::instance().registerPlot(
            { category, name, units, color, sensor, measurement }
            );
    }

    qInfo() << "[PluginHost] Registered"
            << py::len(sdk.attr("_attributes"))
            << "attributes, "
            << py::len(sdk.attr("_measurements"))
            << "measurements, and "
            << py::len(sdk.attr("_simple_plots"))
            << "plots.";
}
