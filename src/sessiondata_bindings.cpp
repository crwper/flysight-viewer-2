#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <QString>
#include <QVariant>
#include <QDateTime>
#include "sessiondata.h"

namespace py = pybind11;
using namespace FlySight;

void register_sessiondata(py::module_ &m) {
    py::class_<SessionData>(m, "SessionData")
        .def("setCalculatedMeasurement",
             [](SessionData &self,
                const std::string &sensorKey,
                const std::string &measurementKey,
                const py::array_t<double, py::array::c_style | py::array::forcecast> &data) {
                 // py::array::forcecast will attempt to convert if the input isn't exactly float64, c-style.
                 // py::array::c_style implies it's already contiguous in the way C expects.

                 if (data.ndim() != 1) { // Check if it's a 1D array
                     throw std::runtime_error("Input NumPy array for setCalculatedMeasurement must be 1D.");
                 }
                 if (data.size() == 0 && data.ndim() == 0) { // Handles 0-D arrays that might result from empty lists cast to array
                     // Allow empty arrays if they are 1D (e.g. np.array([]))
                 } else if (data.size() == 0 && data.ndim() !=1) {
                     throw std::runtime_error("Input NumPy array for setCalculatedMeasurement is 0D but expected 1D for empty array.");
                 }


                 QVector<double> q_data(data.size());
                 // The py::array::c_style in the template argument helps,
                 // but direct memcpy is safest if we are sure about the data type.
                 // py::array::forcecast ensures it's double.
                 if (data.size() > 0) { // Avoid reading data() on an empty array if it's problematic
                     std::memcpy(q_data.data(), data.data(), static_cast<size_t>(data.size()) * sizeof(double));
                 }

                 self.setCalculatedMeasurement(QString::fromStdString(sensorKey),
                                               QString::fromStdString(measurementKey),
                                               q_data);
             },
             py::arg("sensorKey"),
             py::arg("measurementKey"),
             py::arg("data"),
             "Sets a calculated measurement value directly into the session's C++ cache. "
             "Use with caution, intended for plugins that compute multiple related outputs at once.")

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
                 QVariant v = self.getAttribute(QString::fromStdString(key));
                 if (!v.isValid()) {
                     return py::none();
                 }
                 // QDateTime → ISO8601 string with ms + 'Z'
                 if (v.canConvert<QDateTime>()) {
                     QDateTime dt = v.toDateTime();
                     // 1. Convert the time value to UTC
                     QDateTime utcDt = dt.toUTC();
                     // 2. Format the UTC time using the desired ISO format
                     QString s = utcDt.toString(Qt::ISODateWithMs);
                     return py::cast(s.toStdString());
                 }
                 // numeric?
                 bool ok = false;
                 double d = v.toDouble(&ok);
                 if (ok) {
                     return py::cast(d);
                 }
                 // plain string?
                 if (v.type() == QVariant::String) {
                     return py::cast(v.toString().toStdString());
                 }
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
