#include <pybind11/pybind11.h>
#include "python_output_redirector.h"

namespace py = pybind11;
using namespace FlySight;

// Forward declare the registration functions defined in other files
void register_dependencykey(py::module_ &m);
void register_sessiondata(py::module_ &m);

// Register PythonOutputRedirector
void register_python_output_redirector(py::module_ &m) {
    py::class_<PythonOutputRedirector>(m, "PythonOutputRedirector_CPP")
        .def(py::init<>())
        .def("write", &PythonOutputRedirector::write, py::arg("message"))
        .def("flush", &PythonOutputRedirector::flush);
}

// Define the module - NO problematic includes here!
#pragma push_macro("slots")
#undef slots
PYBIND11_MODULE(flysight_cpp_bridge, m) {
    m.doc() = "C++ bridge module for FlySight Python plugins";

    // Call registration functions from other files
    register_dependencykey(m);
    register_sessiondata(m);
    register_python_output_redirector(m);

    // Add any module-level functions or submodules here if needed
}
#pragma pop_macro("slots")
