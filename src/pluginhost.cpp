/*****************************************************************************
 *  FlySight Viewer – Plugin host
 *
 *  Boots CPython (via pybind11) and registers Python-side plug-ins.
 *  • If <exe>/python exists  → use the bundled “embeddable” runtime.
 *  • Otherwise               → fall back to the developer’s system Python.
 *****************************************************************************/

/* Qt headers – compile with _DEBUG still defined */
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QVariant>
#include <QDebug>

/* STL */
#include <memory>
#include <optional>
#include <string>

#pragma push_macro("slots")
#undef  slots                    // avoid Qt's `slots` macro clash

/* Python headers */
#if defined(_MSC_VER)            // mask _DEBUG only while including Python.h
#  pragma push_macro("_DEBUG")
#  undef  _DEBUG
#endif
#include <Python.h>              // PEP-587 API (needs release mode)
#if defined(_MSC_VER)
#  pragma pop_macro("_DEBUG")
#endif

#include <pybind11/embed.h>      // pybind11 does its own _DEBUG masking
#include <pybind11/numpy.h>
#pragma pop_macro("slots")

/* FlySight headers */
#include "pluginhost.h"
#include "sessiondata.h"
#include "dependencykey.h"
#include "plotregistry.h"
#include "python_output_redirector.h"

namespace py = pybind11;
using   namespace FlySight;

/* ────────────────────────────────────────────────────────────────────────────
 *  Utility
 * ────────────────────────────────────────────────────────────────────────── */
static QString pyStatusToString(const PyStatus& s)
{
    return PyStatus_Exception(s) ?
               QString::fromUtf8(s.err_msg ? s.err_msg : "unknown") :
               QStringLiteral("success");
}

/* ────────────────────────────────────────────────────────────────────────────
 *  Singleton
 * ────────────────────────────────────────────────────────────────────────── */
PluginHost& PluginHost::instance()
{
    static PluginHost inst;
    return inst;
}

/* ────────────────────────────────────────────────────────────────────────────
 *  Initialise interpreter and load plug-ins
 * ────────────────────────────────────────────────────────────────────────── */
void PluginHost::initialise(const QString& pluginDir)
{
    if (m_interp)                       // already initialised
        return;

    const QString appDir   = QCoreApplication::applicationDirPath();
    const QString embedDir = QDir(appDir).filePath(QStringLiteral("python"));
    const bool    useEmbed = QDir(embedDir).exists();

    /* ------------------------------------------------------------------ */
    /* 1. Boot CPython                                                    */
    /* ------------------------------------------------------------------ */
    try {
        if (useEmbed) {
            /* Build a PyConfig pointing at the bundled runtime */
            PyConfig cfg;
            PyConfig_InitPythonConfig(&cfg);          // "normal" configuration

#ifdef _WIN32
            const std::wstring wHome = embedDir.toStdWString();
            PyStatus st = PyConfig_SetString(&cfg, &cfg.home, wHome.c_str());
#else
            const std::string  home  = embedDir.toStdString();
            PyStatus st = PyConfig_SetString(&cfg, &cfg.home, home.c_str());
#endif
            if (PyStatus_Exception(st)) {
                qCritical().noquote() << "PyConfig_SetString failed:"
                                      << pyStatusToString(st);
                PyConfig_Clear(&cfg);
                return;
            }

            /* Add runtime-local import paths */
#ifdef _WIN32
            const std::wstring zip  = wHome + L"\\python313.zip";
            const std::wstring site = wHome + L"\\site-packages";
            PyWideStringList_Append(&cfg.module_search_paths, wHome.c_str());
            PyWideStringList_Append(&cfg.module_search_paths, zip.c_str());
            PyWideStringList_Append(&cfg.module_search_paths, site.c_str());
#else
            const std::string zip  = home + "/python313.zip";
            const std::string site = home + "/site-packages";
            PyStringList_Append(&cfg.module_search_paths, home.c_str());
            PyStringList_Append(&cfg.module_search_paths, zip.c_str());
            PyStringList_Append(&cfg.module_search_paths, site.c_str());
#endif
            cfg.module_search_paths_set = 1;

#ifdef _WIN32
            /* Ensure Windows can locate .pyd extension modules */
            const QString newPath =
                qEnvironmentVariable("PATH") + u';' + embedDir;
            _wputenv_s(L"PATH", newPath.toStdWString().c_str());
#endif
            m_interp = std::make_unique<py::scoped_interpreter>(&cfg, 0, nullptr, false);
            PyConfig_Clear(&cfg);

            qInfo().noquote() << "[PluginHost] Embedded Python booted from"
                              << QDir::toNativeSeparators(embedDir);
        } else {
            m_interp = std::make_unique<py::scoped_interpreter>();
            qInfo() << "[PluginHost] Using system Python runtime.";
        }
    } catch (const py::error_already_set& e) {
        qCritical().noquote()
        << "[PluginHost] Python error while starting interpreter:" << e.what();
        return;
    } catch (const std::exception& e) {
        qCritical().noquote()
        << "[PluginHost] Failed to start interpreter:" << e.what();
        return;
    }

    // Declare sys module variable here to be used by both redirection and path modification
    py::module_ sys;

    // Import C++ bridge module FIRST
    try {
        qDebug() << "[PluginHost] Importing flysight_cpp_bridge from C++...";
        py::module_::import("flysight_cpp_bridge"); // This initializes the module and registers types
        qDebug() << "[PluginHost] flysight_cpp_bridge imported successfully by C++.";
    } catch (const py::error_already_set& e) {
        qCritical().noquote() << "[PluginHost] CRITICAL: Failed to import flysight_cpp_bridge from C++:" << e.what();
        // ... (error handling) ...
        return;
    }

    // Setup Python output redirection
    try {
        qDebug() << "[PluginHost] Attempting to import 'sys' module for redirection.";
        sys = py::module_::import("sys");
        qDebug() << "[PluginHost] 'sys' module imported. Creating redirector.";
        // PythonOutputRedirector should now be a "known registered type" by pybind11
        py::object redirector_instance = py::cast(new PythonOutputRedirector(), py::return_value_policy::take_ownership);
        qDebug() << "[PluginHost] Redirector instance created. Assigning to sys.stdout/stderr.";
        sys.attr("stdout") = redirector_instance;
        sys.attr("stderr") = redirector_instance;
        qInfo() << "[PluginHost] Python stdout/stderr redirected to qDebug().";
    } catch (const py::error_already_set& e) {
        qCritical().noquote() << "[PluginHost] Failed to redirect Python stdout/stderr (after bridge import):" << e.what();
        if (Py_IsInitialized()) PyErr_Print();
    }

    /* ------------------------------------------------------------------ */
    /* 2.  Put <plugins>/ on sys.path & import SDK                        */
    /* ------------------------------------------------------------------ */
    qDebug() << "[PluginHost] Preparing to modify sys.path. Plugin dir:" << pluginDir;
    if (pluginDir.isEmpty()) {
        qWarning() << "[PluginHost] pluginDir is empty. Aborting further plugin loading.";
        return;
    }

    if (!sys) { // Check if sys was successfully imported earlier
        qCritical() << "[PluginHost] 'sys' module was not imported successfully earlier. Cannot modify sys.path.";
        return;
    }

    try {
        std::string pluginDirStd = pluginDir.toStdString();
        qDebug() << "[PluginHost] Adding to sys.path:" << pluginDir;
        sys.attr("path").attr("insert")(0, pluginDirStd.c_str()); // Use the 'sys' object obtained earlier
        qDebug() << "[PluginHost] Successfully added" << pluginDir << "to sys.path.";
    } catch (const py::error_already_set& e) {
        qCritical().noquote() << "[PluginHost] Python error modifying sys.path with '" << pluginDir << "':" << e.what();
        if (Py_IsInitialized()) PyErr_Print();
        return;
    } catch (const std::exception& e) {
        qCritical().noquote() << "[PluginHost] C++ error modifying sys.path with '" << pluginDir << "':" << e.what();
        return;
    }

    py::module sdk;
    try {
        qDebug() << "[PluginHost] Attempting to import flysight_plugin_sdk...";
        sdk = py::module::import("flysight_plugin_sdk");
        qDebug() << "[PluginHost] flysight_plugin_sdk imported successfully.";
    } catch (const py::error_already_set& e) {
        qCritical().noquote() << "[PluginHost] Python error importing flysight_plugin_sdk. Ensure it's in Python path ('" << pluginDir << "') and has no internal import errors (like flysight_cpp_bridge).";
        if (Py_IsInitialized()) PyErr_Print();
        return;
    } catch (const std::exception& e) {
        qCritical().noquote() << "[PluginHost] C++ error importing flysight_plugin_sdk:" << e.what();
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 3.  Import every .py file in the plug-in directory                 */
    /* ------------------------------------------------------------------ */
    QDir dir(pluginDir);
    for (const QFileInfo& fi : dir.entryInfoList({ "*.py" }, QDir::Files)) {
        try {
            py::module::import(fi.baseName().toStdString().c_str());
        } catch (const py::error_already_set& e) {
            qWarning().noquote() << "[PluginHost] Plug-in" << fi.fileName()
            << "failed to import:" << e.what();
        }
    }

    /* ------------------------------------------------------------------ */
    /* 4.  Register calculated attributes                                 */
    /* ------------------------------------------------------------------ */
    for (py::handle h : sdk.attr("_attributes")) {
        py::object plugin = h.cast<py::object>();
        const QString key =
            QString::fromStdString(plugin.attr("name").cast<std::string>());

        QList<DependencyKey> deps;
        for (py::handle hi : plugin.attr("inputs")().cast<py::list>()) {
            py::object dk = hi.cast<py::object>();
            int kind      = dk.attr("kind").cast<int>();

            if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                deps.append(DependencyKey::attribute(
                    QString::fromStdString(dk.attr("attributeKey").cast<std::string>())));
            } else {
                deps.append(DependencyKey::measurement(
                    QString::fromStdString(dk.attr("sensorKey").cast<std::string>()),
                    QString::fromStdString(dk.attr("measurementKey").cast<std::string>())));
            }
        }

        SessionData::registerCalculatedAttribute(
            key, deps,
            [plugin](SessionData& s) -> std::optional<QVariant> {
                py::gil_scoped_acquire gil;
                py::object out = plugin.attr("compute")(py::cast(&s,
                                                                 py::return_value_policy::reference));
                if (out.is_none()) return std::nullopt;

                if (py::isinstance<py::float_>(out) ||
                    py::isinstance<py::int_>(out))
                    return QVariant::fromValue(out.cast<double>());

                if (py::isinstance<py::str>(out)) {
                    const QString txt =
                        QString::fromStdString(out.cast<std::string>());
                    const QDateTime dt =
                        QDateTime::fromString(txt, Qt::ISODateWithMs);
                    return dt.isValid() ? QVariant::fromValue(dt)
                                        : QVariant::fromValue(txt);
                }
                return std::nullopt;
            });
    }

    /* ------------------------------------------------------------------ */
    /* 5.  Register calculated measurements                               */
    /* ------------------------------------------------------------------ */
    for (py::handle h : sdk.attr("_measurements")) {
        py::object plugin = h.cast<py::object>();
        const QString sensor =
            QString::fromStdString(plugin.attr("sensor").cast<std::string>());
        const QString name =
            QString::fromStdString(plugin.attr("name").cast<std::string>());

        QList<DependencyKey> deps;
        for (py::handle hi : plugin.attr("inputs")().cast<py::list>()) {
            py::object dk = hi.cast<py::object>();
            int kind      = dk.attr("kind").cast<int>();

            if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                deps.append(DependencyKey::attribute(
                    QString::fromStdString(dk.attr("attributeKey").cast<std::string>())));
            } else {
                deps.append(DependencyKey::measurement(
                    QString::fromStdString(dk.attr("sensorKey").cast<std::string>()),
                    QString::fromStdString(dk.attr("measurementKey").cast<std::string>())));
            }
        }

        SessionData::registerCalculatedMeasurement(
            sensor, name, deps,
            [plugin](SessionData& s) -> std::optional<QVector<double>> {
                py::gil_scoped_acquire gil;
                py::object out = plugin.attr("compute")(py::cast(&s,
                                                                 py::return_value_policy::reference));
                if (out.is_none()) return std::nullopt;

                auto buf = out.cast<
                    py::array_t<double,
                                py::array::c_style | py::array::forcecast>>();
                return QVector<double>(buf.data(), buf.data() + buf.shape(0));
            });
    }

    /* ------------------------------------------------------------------ */
    /* 6.  Register simple plot definitions                               */
    /* ------------------------------------------------------------------ */
    for (py::handle h : sdk.attr("_simple_plots")) {
        py::object plt = h.cast<py::object>();
        PlotRegistry::instance().registerPlot({
            QString::fromStdString(plt.attr("category").cast<std::string>()),
            QString::fromStdString(plt.attr("name").cast<std::string>()),
            plt.attr("units").is_none() ? QString{} :
                QString::fromStdString(plt.attr("units").cast<std::string>()),
            QColor(QString::fromStdString(
                plt.attr("color").cast<std::string>())),
            QString::fromStdString(plt.attr("sensor").cast<std::string>()),
            QString::fromStdString(
                plt.attr("measurement").cast<std::string>())
        });
    }

    /* ------------------------------------------------------------------ */
    /* 7.  Summary                                                        */
    /* ------------------------------------------------------------------ */
    qInfo() << "[PluginHost] Registered"
            << py::len(sdk.attr("_attributes"))   << "attributes,"
            << py::len(sdk.attr("_measurements")) << "measurements and"
            << py::len(sdk.attr("_simple_plots")) << "plots.";
}
