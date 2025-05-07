/*****************************************************************************
 *  FlySight 2 Viewer – Plugin host (Option‑1 refactor)
 *
 *  Boots CPython **via pybind11’s scoped_interpreter that takes a PyConfig***
 *  – so there is exactly **one** initialisation call.
 *
 *  Key changes:
 *    • Build a PyConfig the same way as before (PEP‑587).
 *    • Pass that pointer to the 4‑argument constructor of
 *      `py::scoped_interpreter` (available since pybind11 v2.11).
 *    • Remove the explicit `Py_InitializeFromConfig()` call (and therefore
 *      the double‑initialisation that raised “The interpreter is already
 *      running”).
 *
 *  All plugin‑loading logic after the interpreter boot remains untouched.
 *****************************************************************************/

#pragma push_macro("slots")
#undef  slots                         /* avoid Qt's `slots` vs. Python clash   */

#include <Python.h>                   /*  PEP 587 API                          */
#include <pybind11/embed.h>           /*  py::scoped_interpreter, py::module   */
#include <pybind11/numpy.h>
#pragma pop_macro("slots")

/* FlySight headers --------------------------------------------------------- */
#include "pluginhost.h"
#include "sessiondata.h"
#include "dependencykey.h"
#include "plotregistry.h"

/* Qt headers --------------------------------------------------------------- */
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QVariant>
#include <QDebug>

/* std / STL ---------------------------------------------------------------- */
#include <optional>
#include <string>

namespace py = pybind11;
using   namespace FlySight;

// ────────────────────────────────────────────────────────────────────────────
//  Singleton
// ────────────────────────────────────────────────────────────────────────────
PluginHost& PluginHost::instance()
{
    static PluginHost inst;
    return inst;
}

// ────────────────────────────────────────────────────────────────────────────
//  Helper – convert PyStatus to QString
// ────────────────────────────────────────────────────────────────────────────
static QString statusToString(const PyStatus& st)
{
    if (PyStatus_Exception(st)) {
        return QString::fromUtf8(st.err_msg ? st.err_msg : "unknown");
    }
    return "success";
}

// ────────────────────────────────────────────────────────────────────────────
//  Boot interpreter + load plugins (Option‑1 implementation)
// ────────────────────────────────────────────────────────────────────────────
void PluginHost::initialise(const QString& pluginDir)
{
    /* ------------------------------------------------------------------
     * 1.  Boot the interpreter (exactly once, via pybind11)
     * ----------------------------------------------------------------*/
    if (!m_interp) {
        /* Location of the bundled runtime:  {app}/python               */
        const QString pyHomeQt =
            QDir(QCoreApplication::applicationDirPath()).filePath("python");

#ifdef _WIN32
        static const std::wstring pyHomeW = pyHomeQt.toStdWString();
#else
        static const std::string  pyHome  = pyHomeQt.toStdString();
#endif

        /* --- 1.1  Build a PyConfig ----------------------------------- */
        PyConfig cfg;
        PyConfig_InitPythonConfig(&cfg);          /* “normal” (non‑isolated) */

#ifdef _WIN32
        PyStatus st = PyConfig_SetString(&cfg, &cfg.home, pyHomeW.c_str());
#else
        PyStatus st = PyConfig_SetString(&cfg, &cfg.home, pyHome.c_str());
#endif
        if (PyStatus_Exception(st)) {
            qCritical().noquote() << "PyConfig_SetString failed:" << statusToString(st);
            PyConfig_Clear(&cfg);
            return;
        }

        /* --- 1.1.b  Tell CPython where to import from ---------------- */
#ifdef _WIN32
        const std::wstring zipPathW  = pyHomeW + L"\\python313.zip";
        const std::wstring sitePkgsW = pyHomeW + L"\\site-packages";
        PyWideStringList_Append(&cfg.module_search_paths, pyHomeW.c_str());
        PyWideStringList_Append(&cfg.module_search_paths, zipPathW.c_str());
        PyWideStringList_Append(&cfg.module_search_paths, sitePkgsW.c_str());
#else
        const std::string zipPath  = pyHome  + "/python313.zip";
        const std::string sitePkgs = pyHome  + "/site-packages";
        PyStringList_Append(&cfg.module_search_paths, pyHome.c_str());
        PyStringList_Append(&cfg.module_search_paths, zipPath.c_str());
        PyStringList_Append(&cfg.module_search_paths, sitePkgs.c_str());
#endif
        cfg.module_search_paths_set = 1;   /* we provided the list */

        /* --- 1.2  On Windows extend PATH so the loader can locate *.pyd */
#ifdef _WIN32
        QString newPath = qEnvironmentVariable("PATH") + u';' + pyHomeQt;
        _wputenv_s(L"PATH", newPath.toStdWString().c_str());
#endif

        /* --- 1.3  Initialise interpreter via scoped_interpreter ----- */
        try {
            m_interp = std::make_unique<py::scoped_interpreter>(&cfg, 0, nullptr, false);
            /* After construction the interpreter is initialised.        */
            qInfo().noquote() << "[PluginHost] Embedded Python booted from"
                              << QDir::toNativeSeparators(pyHomeQt);
            qInfo() << "[PluginHost] scoped_interpreter created successfully.";
        } catch (const py::error_already_set &e) {
            qCritical() << "[PluginHost] pybind11 exception during scoped_interpreter creation:" << e.what();
            if (PyErr_Occurred()) PyErr_Print();
            PyConfig_Clear(&cfg);
            return;
        } catch (const std::exception &e) {
            qCritical() << "[PluginHost] C++ std::exception during scoped_interpreter creation:" << e.what();
            PyConfig_Clear(&cfg);
            return;
        } catch (...) {
            qCritical() << "[PluginHost] Unknown exception during scoped_interpreter creation.";
            PyConfig_Clear(&cfg);
            return;
        }
        PyConfig_Clear(&cfg);   /* safe – interpreter made its own copy */

        /* Optional smoke‑test ---------------------------------------- */
        try {
            py::module_::import("sys");
            qInfo() << "[PluginHost] Successfully imported 'sys'.";
        } catch (const py::error_already_set &e) {
            qCritical() << "[PluginHost] Failed to import 'sys':" << e.what();
        }
    }

    /* ------------------------------------------------------------------
     * 2.  Add the plugins folder to sys.path
     * ----------------------------------------------------------------*/
    py::module sys = py::module::import("sys");
    sys.attr("path").attr("insert")(0, pluginDir.toStdString().c_str());

    /* ------------------------------------------------------------------
     * 3.  Import FlySight’s Python‑side SDK
     * ----------------------------------------------------------------*/
    py::module sdk;
    try {
        sdk = py::module::import("flysight_plugin_sdk");
    } catch (const py::error_already_set& e) {
        qCritical() << "Failed to import flysight_plugin_sdk:" << e.what();
        return;
    }

    /* ------------------------------------------------------------------
     * 4.  Import every *.py file in the plugins folder
     * ----------------------------------------------------------------*/
    {
        QDir dir(pluginDir);
        for (const QFileInfo& fi : dir.entryInfoList({ "*.py" }, QDir::Files)) {
            const std::string mod = fi.baseName().toStdString();
            try {
                py::module::import(mod.c_str());
            } catch (const py::error_already_set& e) {
                qWarning() << "[PluginHost] Failed to import" << fi.fileName() << ":" << e.what();
            }
        }
    }

    /* ------------------------------------------------------------------
     * 5.  Register Attribute plugins
     * ----------------------------------------------------------------*/
    for (py::handle h : sdk.attr("_attributes")) {
        py::object plugin = h.cast<py::object>();
        const QString key = QString::fromStdString(plugin.attr("name").cast<std::string>());

        QList<DependencyKey> deps;
        try {
            py::list inputs = plugin.attr("inputs")().cast<py::list>();
            for (py::handle hi : inputs) {
                py::object dk = hi.cast<py::object>();
                int kind      = dk.attr("kind").cast<int>();

                if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                    deps.append(DependencyKey::attribute(QString::fromStdString(dk.attr("attributeKey").cast<std::string>())));
                } else {
                    deps.append(DependencyKey::measurement(QString::fromStdString(dk.attr("sensorKey").cast<std::string>()),
                                                           QString::fromStdString(dk.attr("measurementKey").cast<std::string>())));
                }
            }
        } catch (const py::error_already_set& e) {
            qWarning() << "[PluginHost] exception in inputs():" << e.what();
        }

        SessionData::registerCalculatedAttribute(
            key, deps,
            [plugin](SessionData& session) -> std::optional<QVariant> {
                py::gil_scoped_acquire gil;
                py::object out = plugin.attr("compute")(py::cast(&session, py::return_value_policy::reference));

                if (out.is_none()) return std::nullopt;

                if (py::isinstance<py::float_>(out) || py::isinstance<py::int_>(out))
                    return QVariant::fromValue(out.cast<double>());

                if (py::isinstance<py::str>(out)) {
                    QString s = QString::fromStdString(out.cast<std::string>());
                    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
                    return dt.isValid() ? QVariant::fromValue(dt)
                                        : QVariant::fromValue(s);
                }
                return std::nullopt;
            });
    }

    /* ------------------------------------------------------------------
     * 6.  Register Measurement plugins
     * ----------------------------------------------------------------*/
    for (py::handle h : sdk.attr("_measurements")) {
        py::object plugin = h.cast<py::object>();
        QString sensor = QString::fromStdString(plugin.attr("sensor").cast<std::string>());
        QString name   = QString::fromStdString(plugin.attr("name").cast<std::string>());

        QList<DependencyKey> deps;
        try {
            py::list inputs = plugin.attr("inputs")().cast<py::list>();
            for (py::handle hi : inputs) {
                py::object dk = hi.cast<py::object>();
                int kind      = dk.attr("kind").cast<int>();

                if (kind == static_cast<int>(DependencyKey::Type::Attribute)) {
                    deps.append(DependencyKey::attribute(QString::fromStdString(dk.attr("attributeKey").cast<std::string>())));
                } else {
                    deps.append(DependencyKey::measurement(QString::fromStdString(dk.attr("sensorKey").cast<std::string>()),
                                                           QString::fromStdString(dk.attr("measurementKey").cast<std::string>())));
                }
            }
        } catch (const py::error_already_set& e) {
            qWarning() << "[PluginHost] exception in inputs():" << e.what();
        }

        SessionData::registerCalculatedMeasurement(
            sensor, name, deps,
            [plugin](SessionData& session) -> std::optional<QVector<double>> {
                py::gil_scoped_acquire gil;
                py::object out = plugin.attr("compute")(py::cast(&session, py::return_value_policy::reference));
                if (out.is_none()) return std::nullopt;

                auto buf = out.cast<py::array_t<double, py::array::c_style | py::array::forcecast>>();
                return QVector<double>(buf.data(), buf.data() + buf.shape(0));
            });
    }

    /* ------------------------------------------------------------------
     * 7.  Register “simple plot” definitions
     * ----------------------------------------------------------------*/
    for (py::handle h : sdk.attr("_simple_plots")) {
        py::object plt = h.cast<py::object>();
        PlotRegistry::instance().registerPlot({
            QString::fromStdString(plt.attr("category").cast<std::string>()),
            QString::fromStdString(plt.attr("name").cast<std::string>()),
            plt.attr("units").is_none() ? QString{} : QString::fromStdString(plt.attr("units").cast<std::string>()),
            QColor(QString::fromStdString(plt.attr("color").cast<std::string>())),
            QString::fromStdString(plt.attr("sensor").cast<std::string>()),
            QString::fromStdString(plt.attr("measurement").cast<std::string>())
        });
    }

    /* ------------------------------------------------------------------
     * 8.  Summary
     * ----------------------------------------------------------------*/
    qInfo() << "[PluginHost] Registered"
            << py::len(sdk.attr("_attributes"))   << "attributes,"
            << py::len(sdk.attr("_measurements")) << "measurements, and"
            << py::len(sdk.attr("_simple_plots")) << "plots.";
}
