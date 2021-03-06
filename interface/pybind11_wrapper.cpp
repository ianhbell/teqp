#include "pybind11_wrapper.hpp"

#include "teqpversion.hpp"

namespace py = pybind11;

// The implementation of each prototype are in separate files to move the compilation into 
// multiple compilation units so that multiple processors can be used
// at the same time to carry out the compilation
// 
// This speeds up things a lot on linux, but not much in MSVC
void add_vdW(py::module &m);
void add_PCSAFT(py::module& m);
void add_CPA(py::module& m);
void add_multifluid(py::module& m);
void add_multifluid_mutant(py::module& m);

/// Instantiate "instances" of models (really wrapped Python versions of the models), and then attach all derivative methods
void init_teqp(py::module& m) {
    add_vdW(m);
    add_PCSAFT(m);
    add_CPA(m);
    add_multifluid(m);
    add_multifluid_mutant(m);

    // Some functions for timing overhead of interface
    m.def("___mysummer", [](const double &c, const Eigen::ArrayXd &x) { return c*x.sum(); });
    using RAX = Eigen::Ref<Eigen::ArrayXd>;
    using namespace pybind11::literals; // for "arg"_a
    m.def("___mysummerref", [](const double& c, const RAX x) { return c * x.sum(); }, "c"_a, "x"_a.noconvert());
    m.def("___myadder", [](const double& c, const double& d) { return c+d; });
}

PYBIND11_MODULE(teqp, m) {
    m.doc() = "TEQP: Templated Equation of State Package";
    m.attr("__version__") = TEQPVERSION;
    init_teqp(m);
}