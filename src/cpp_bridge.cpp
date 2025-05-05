#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declare the registration functions defined in other files
void register_dependencykey(py::module_ &m);
void register_sessiondata(py::module_ &m);

// Define the module - NO problematic includes here!
#pragma push_macro("slots")
#undef slots
PYBIND11_MODULE(flysight_cpp_bridge, m) {
    m.doc() = "C++ bridge module for FlySight Python plugins";

    // Call registration functions from other files
    register_dependencykey(m);
    register_sessiondata(m);

    // Add any module-level functions or submodules here if needed
}
#pragma pop_macro("slots")
