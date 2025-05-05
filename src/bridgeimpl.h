#ifndef BRIDGEIMPL_H
#define BRIDGEIMPL_H

#include <pybind11/pybind11.h> // Might need object/cast
#include <string>
// Forward declare types used in signatures
namespace FlySight {
class DependencyKey;
class SessionData;
}
namespace py = pybind11;

// Declare functions implemented in bridge_impl.cpp
std::string get_key_name(const FlySight::DependencyKey &key);
py::object session_get_time_series(FlySight::SessionData &session, const std::string &key);

#endif // BRIDGEIMPL_H
