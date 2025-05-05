#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <QString>
#include <QVariant>
#include "sessiondata.h"

namespace py = pybind11;
using namespace FlySight;

void register_sessiondata(py::module_ &m) {
    py::class_<SessionData>(m, "SessionData")
    // session.getMeasurement(sensor, measurement) → list[float]
    .def("getMeasurement",
         [](SessionData &self,
            std::string sensor,
            std::string measurement)
         {
             auto qv = self.getMeasurement(
                 QString::fromStdString(sensor),
                 QString::fromStdString(measurement));
             return std::vector<double>(qv.begin(), qv.end());
         },
         py::arg("sensorKey"),
         py::arg("measurementKey"))

        // session.getAttribute(key) → float, str, or None
        .def("getAttribute",
             [](SessionData &self, std::string key) -> py::object {
                 QVariant v = self.getAttribute(
                     QString::fromStdString(key));
                 if (!v.isValid()) {
                     return py::none();
                 }
                 // numeric?
                 bool ok = false;
                 double d = v.toDouble(&ok);
                 if (ok) {
                     return py::cast(d);
                 }
                 // string?
                 if (v.type() == QVariant::String) {
                     return py::cast(v.toString().toStdString());
                 }
                 // fallback
                 return py::none();
             },
             py::arg("key"))

        // Optional helpers in Python if you want them
        .def("hasMeasurement",
             [](SessionData &self,
                std::string sensor,
                std::string measurement)
             {
                 return self.hasMeasurement(
                     QString::fromStdString(sensor),
                     QString::fromStdString(measurement));
             },
             py::arg("sensorKey"),
             py::arg("measurementKey"))

        .def("hasAttribute",
             [](SessionData &self, std::string key) {
                 return self.hasAttribute(QString::fromStdString(key));
             },
             py::arg("key"))
        ;
}
