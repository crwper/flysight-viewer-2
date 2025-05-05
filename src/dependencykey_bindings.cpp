#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dependencykey.h"

namespace py = pybind11;
using namespace FlySight;

void register_dependencykey(py::module_ &m) {
    // Bind the DependencyKey struct
    py::class_<DependencyKey> dk(m, "DependencyKey");
    dk
        // default ctor
        .def(py::init<>())
        // expose the enum value
        .def_readwrite("kind", &DependencyKey::type)
        // for Attributes
        .def_readwrite("attributeKey", &DependencyKey::attributeKey)
        // for Measurements we split the QPair into two string props:
        .def_property("sensorKey",
                      [](DependencyKey const &self) {
                          return self.measurementKey.first.toStdString();
                      },
                      [](DependencyKey &self, std::string v) {
                          self.measurementKey.first = QString::fromStdString(v);
                      })
        .def_property("measurementKey",
                      [](DependencyKey const &self) {
                          return self.measurementKey.second.toStdString();
                      },
                      [](DependencyKey &self, std::string v) {
                          self.measurementKey.second = QString::fromStdString(v);
                      })
        // static factories
        .def_static("attribute",
                    &DependencyKey::attribute,
                    py::arg("key"))
        .def_static("measurement",
                    &DependencyKey::measurement,
                    py::arg("sensorKey"),
                    py::arg("measurementKey"))
        ;

    // Bind the nested enum
    py::enum_<DependencyKey::Type>(dk, "Type")
        .value("Attribute",   DependencyKey::Type::Attribute)
        .value("Measurement", DependencyKey::Type::Measurement)
        .export_values();
}
