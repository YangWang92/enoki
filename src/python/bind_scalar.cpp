#include "bind.h"
#include "random.h"

void export_scalar(py::module_ &m) {
    py::module_ scalar = m.def_submodule("scalar");

    scalar.attr("Bool")    = py::handle((PyObject *) &PyBool_Type);
    scalar.attr("Float32") = py::handle((PyObject *) &PyFloat_Type);
    scalar.attr("Float64") = py::handle((PyObject *) &PyFloat_Type);
    scalar.attr("Int32")   = py::handle((PyObject *) &PyLong_Type);
    scalar.attr("Int64")   = py::handle((PyObject *) &PyLong_Type);
    scalar.attr("UInt32")  = py::handle((PyObject *) &PyLong_Type);
    scalar.attr("UInt64")  = py::handle((PyObject *) &PyLong_Type);

    scalar.attr("Float")   = scalar.attr("Float32");
    scalar.attr("Int")     = scalar.attr("Int32");
    scalar.attr("UInt")    = scalar.attr("UInt32");

    ENOKI_BIND_ARRAY_TYPES(scalar, float, true);

    bind_full(d_i32, true); bind_full(d_u32, true); bind_full(d_i64, true);
    bind_full(d_u64, true); bind_full(d_f32, true); bind_full(d_f64, true);
    bind_full(d_b, true);

    bind_pcg32<uint64_t>(scalar);

    struct LoopDummy { LoopDummy(py::args) { }};
    py::class_<LoopDummy>(scalar, "Loop")
        .def(py::init<py::args>())
        .def("cond", [](LoopDummy&, bool value) { return value; });
}
