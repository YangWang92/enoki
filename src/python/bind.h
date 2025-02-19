#pragma once

#include "common.h"
#include <enoki/math.h>
#include <enoki/complex.h>
#include <enoki/matrix.h>
#include <enoki/quaternion.h>
#include <enoki/autodiff.h>
#include <pybind11/functional.h>

extern py::handle array_base, array_name, array_init, array_configure;

template <typename Array>
auto bind_type(py::module_ &m, bool scalar_mode = false) {
    using Scalar = std::conditional_t<Array::IsMask, bool, ek::scalar_t<Array>>;
    using Value = std::conditional_t<Array::IsMask, ek::mask_t<ek::value_t<Array>>,
                                     ek::value_t<Array>>;
    constexpr VarType Type = ek::var_type_v<Scalar>;

    const char *prefix = "Array";
    if constexpr (ek::is_complex_v<Array>)
        prefix = "Complex";
    if constexpr (ek::is_quaternion_v<Array>)
        prefix = "Quaternion";
    if constexpr (ek::is_matrix_v<Array>)
        prefix = "Matrix";

    py::tuple shape;
    if constexpr (Array::Depth == 1)
        shape = py::make_tuple(Array::Size);
    else if constexpr (Array::Depth == 2)
        shape = py::make_tuple(Array::Size, Value::Size);
    else if constexpr (Array::Depth == 3)
        shape = py::make_tuple(Array::Size, Value::Size, Value::Value::Size);
    else if constexpr (Array::Depth == 4)
        shape = py::make_tuple(Array::Size, Value::Size, Value::Value::Size,
                               Value::Value::Value::Size);

    PyTypeObject *value_obj;
    if constexpr (std::is_scalar_v<Value>) {
        if constexpr (std::is_same_v<bool, Value>)
            value_obj = &PyBool_Type;
        else if constexpr (std::is_integral_v<Value>)
            value_obj = &PyLong_Type;
        else
            value_obj = &PyFloat_Type;
    } else {
        auto &reg_types = py::detail::get_internals().registered_types_cpp;
        auto it = reg_types.find(std::type_index(typeid(Value)));
        if (it == reg_types.end())
            ek::enoki_raise("bind_type(): value type was not bound!");
        value_obj = it->second->type;
    }

    py::object type = py::cast(Type),
               name = array_name(prefix, type, shape, scalar_mode);

    auto cls = py::class_<Array>(
        m, PyUnicode_AsUTF8AndSize(name.ptr(), nullptr), array_base);

    array_configure(cls, shape, type, py::handle((PyObject *) value_obj));
    register_implicit_conversions(typeid(Array));

    return cls;
}

template <typename Array>
void bind_basic_methods(py::class_<Array> &cls) {
    using Value = std::conditional_t<Array::IsMask, ek::mask_t<ek::value_t<Array>>,
                                     ek::value_t<Array>>;
    cls.def("entry_", [](const Array &a, size_t i) -> Value { return a.entry(i); })
       .def("set_entry_", [](Array &a, size_t i, const Value &value) {
           a.set_entry(i, value);
       });

    if constexpr (ek::is_dynamic_array_v<Array> ||
                  (!ek::is_jit_array_v<Array> && !ek::is_mask_v<Array>))
        cls.def("data_", [](const Array &a) {
            enoki::eval(a);
            return (uintptr_t) a.data();
        });

    if constexpr (ek::is_dynamic_array_v<Array>) {
        cls.def("__len__", &Array::size);
        cls.def("init_", [](Array &a, size_t size) { a.init_(size); });
    }

    if constexpr (ek::array_depth_v<Array> > 1)
        cls.def(
            "entry_ref_",
            [](Array &a, size_t i) -> Value & { return a.entry(i); },
            py::return_value_policy::reference_internal);
}

template <typename Array>
void bind_generic_constructor(py::class_<Array> &cls) {
    cls.def(
        "__init__",
        [](py::detail::value_and_holder &v_h, py::args args) {
            v_h.value_ptr() = new Array();
            array_init(py::handle((PyObject *) v_h.inst), args);
        }, py::detail::is_new_style_constructor());
}

template <typename Array> auto bind(py::module_ &m, bool scalar_mode = false) {
    auto cls = bind_type<Array>(m, scalar_mode);
    bind_generic_constructor(cls);
    bind_basic_methods(cls);
    return cls;
};

template <typename Array>
auto bind_full(py::class_<Array> &cls, bool scalar_mode = false) {
    static_assert(ek::array_depth_v<Array> == 1);
    bind_basic_methods(cls);

    using Scalar = std::conditional_t<Array::IsMask, bool, ek::scalar_t<Array>>;
    using Mask = ek::mask_t<ek::float32_array_t<Array>>;

    cls.def(py::init<Scalar>())
       .def("assign", [](Array &a, const Array &b) {
           if (&a != &b)
               a = b;
       });

    if constexpr (Array::IsFloat)
        cls.def(py::init([](ssize_t value) { return new Array((Scalar) value); }));

    if constexpr (!Array::IsMask) {
        cls.def(py::init<const ek::  int32_array_t<Array> &>());
        cls.def(py::init<const ek:: uint32_array_t<Array> &>());
        cls.def(py::init<const ek::  int64_array_t<Array> &>());
        cls.def(py::init<const ek:: uint64_array_t<Array> &>());
        cls.def(py::init<const ek::float32_array_t<Array> &>());
        cls.def(py::init<const ek::float64_array_t<Array> &>());
    }

    cls.def("or_",     [](const Array &a, const Array &b) { return a.or_(b); });
    cls.def("and_",    [](const Array &a, const Array &b) { return a.and_(b); });
    cls.def("xor_",    [](const Array &a, const Array &b) { return a.xor_(b); });
    cls.def("andnot_", [](const Array &a, const Array &b) { return a.andnot_(b); });
    cls.def("not_", &Array::not_);

    cls.def("ior_",    [](Array *a, const Array &b) { *a = a->or_(b);  return a;});
    cls.def("iand_",   [](Array *a, const Array &b) { *a = a->and_(b); return a;});
    cls.def("ixor_",   [](Array *a, const Array &b) { *a = a->xor_(b); return a;});

    if constexpr (std::is_same_v<Mask, ek::mask_t<Array>>) {
        cls.def("eq_", &Array::eq_);
        cls.def("neq_", &Array::neq_);
    } else {
        cls.def("eq_", [](const Array &a, const Array &b) -> Mask { return a.eq_(b); });
        cls.def("neq_", [](const Array &a, const Array &b) -> Mask { return a.neq_(b); });
    }

    cls.attr("zero_") = py::cpp_function(&Array::zero_);
    cls.attr("full_") = py::cpp_function(
        [](Scalar v, size_t size, bool eval) { return ek::full<Array>(v, size, eval); });

    if constexpr (!Array::IsMask) {
        cls.attr("arange_") = py::cpp_function(&Array::arange_);
        cls.attr("linspace_") = py::cpp_function(&Array::linspace_);
    }

    cls.attr("select_") = py::cpp_function([](const Mask &m, const Array &t,
                                              const Array &f) {
        return Array::select_(static_cast<const ek::mask_t<Array>>(m), t, f);
    });

    if constexpr (Array::IsMask) {
        cls.def("all_", &Array::all_);
        cls.def("any_", &Array::any_);
        cls.def("count_", &Array::count_);
    } else {
        if constexpr (sizeof(Scalar) == 4) {
            cls.def_static("reinterpret_array_",
                           [](const ek::int32_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
            cls.def_static("reinterpret_array_",
                           [](const ek::uint32_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
            cls.def_static("reinterpret_array_",
                           [](const ek::float32_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
        } else {
            cls.def_static("reinterpret_array_",
                           [](const ek::int64_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
            cls.def_static("reinterpret_array_",
                           [](const ek::uint64_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
            cls.def_static("reinterpret_array_",
                           [](const ek::float64_array_t<Array> &a) {
                               return ek::reinterpret_array<Array>(a);
                           });
        }

        cls.def("add_", &Array::add_);
        cls.def("sub_", &Array::sub_);
        cls.def("mul_", &Array::mul_);
        cls.def("mod_", &Array::mod_);
        cls.def(Array::IsFloat ? "truediv_" : "floordiv_", &Array::div_);

        cls.def("iadd_", [](Array *a, const Array &b) { *a = a->add_(b); return a; });
        cls.def("isub_", [](Array *a, const Array &b) { *a = a->sub_(b); return a; });
        cls.def("imul_", [](Array *a, const Array &b) { *a = a->mul_(b); return a; });
        cls.def("imod_", [](Array *a, const Array &b) { *a = a->mod_(b); return a; });

        cls.def(Array::IsFloat ? "itruediv_" : "ifloordiv_",
                [](Array *a, const Array &b) { *a = a->div_(b); return a; });

        cls.def("dot_", &Array::dot_);
        cls.def("hsum_", &Array::hsum_);
        cls.def("hprod_", &Array::hprod_);
        cls.def("hmin_", &Array::hmin_);
        cls.def("hmax_", &Array::hmax_);

        if constexpr (ek::is_dynamic_v<Array> &&
                      ek::array_depth_v<Array> == 1) {
            if constexpr (ek::is_jit_array_v<Array>) {
                cls.def("dot_async_", &Array::dot_async_);
                cls.def("hsum_async_", &Array::hsum_async_);
                cls.def("hprod_async_", &Array::hprod_async_);
                cls.def("hmin_async_", &Array::hmin_async_);
                cls.def("hmax_async_", &Array::hmax_async_);
            }
        }

        cls.def("and_", [](const Array &a, const Mask &b) {
            return a.and_(static_cast<const ek::mask_t<Array> &>(b));
        });

        cls.def("iand_", [](Array *a, const Mask &b) {
            *a = a->and_(static_cast<const ek::mask_t<Array> &>(b));
            return a;
        });

        cls.def("or_", [](const Array &a, const Mask &b) {
            return a.or_(static_cast<const ek::mask_t<Array> &>(b));
        });

        cls.def("ior_", [](Array *a, const Mask &b) {
            *a = a->or_(static_cast<const ek::mask_t<Array> &>(b));
            return a;
        });

        cls.def("xor_", [](const Array &a, const Mask &b) {
            return a.xor_(static_cast<const ek::mask_t<Array> &>(b));
        });

        cls.def("ixor_", [](Array *a, const Mask &b) {
            *a = a->xor_(static_cast<const ek::mask_t<Array> &>(b));
            return a;
        });

        cls.def("andnot_", [](const Array &a, const Mask &b) {
            return a.andnot_(static_cast<const ek::mask_t<Array> &>(b));
        });

        cls.def("abs_", &Array::abs_);
        cls.def("min_", &Array::min_);
        cls.def("max_", &Array::max_);

        if constexpr (std::is_same_v<Mask, ek::mask_t<Array>>) {
            cls.def("lt_", &Array::lt_);
            cls.def("le_", &Array::le_);
            cls.def("gt_", &Array::gt_);
            cls.def("ge_", &Array::ge_);
        } else {
            cls.def("lt_", [](const Array &a, const Array &b) -> Mask { return a.lt_(b); });
            cls.def("le_", [](const Array &a, const Array &b) -> Mask { return a.le_(b); });
            cls.def("gt_", [](const Array &a, const Array &b) -> Mask { return a.gt_(b); });
            cls.def("ge_", [](const Array &a, const Array &b) -> Mask { return a.ge_(b); });
        }

        cls.def("fmadd_", &Array::fmadd_);

        cls.def("neg_", &Array::neg_);
        cls.def("hsum_", &Array::hsum_);
        cls.def("hprod_", &Array::hprod_);
        cls.def("hmax_", &Array::hmax_);
        cls.def("hmin_", &Array::hmin_);
    }

    if constexpr (ek::is_dynamic_v<Array>) {
        using UInt32 = ek::uint32_array_t<Array>;
        cls.def_static("gather_",
                [](const Array &source, const UInt32 &index, const Mask &mask, bool permute) {
                    if (permute)
                        return ek::gather<Array, true>(source, index, mask);
                    else
                        return ek::gather<Array, false>(source, index, mask);
                });
        cls.def("scatter_",
                [](const Array &value, Array &target, const UInt32 &index, const Mask &mask, bool permute) {
                    if (permute)
                        ek::scatter<true>(target, value, index, mask);
                    else
                        ek::scatter<false>(target, value, index, mask);
                }, "target"_a.noconvert(), "index"_a, "mask"_a, "permute"_a);

        if constexpr (Array::IsMask) {
            cls.def("compress_", [](const Array &source) {
                return ek::compress(source);
            });
        } else {
            cls.def("scatter_add_",
                    [](const Array& value, Array& target, const UInt32& index, const Mask& mask) {
                        ek::scatter_add(target, value, index, mask);
                    }, "target"_a.noconvert(), "index"_a, "mask"_a);
        }
    }

    if constexpr (ek::is_jit_array_v<Array>)
        cls.def("resize_", [](Array &value, size_t size) { value.resize(size); });

    if constexpr (Array::IsFloat) {
        cls.def("sqrt_",  &Array::sqrt_);
        cls.def("floor_", &Array::floor_);
        cls.def("ceil_",  &Array::ceil_);
        cls.def("round_", &Array::round_);
        cls.def("trunc_", &Array::trunc_);
        cls.def("rcp_",   &Array::rcp_);
        cls.def("rsqrt_", &Array::rsqrt_);
    } else if constexpr(Array::IsIntegral) {
        cls.def("mulhi_", &Array::mulhi_);
        cls.def("tzcnt_", [](const Array &a) { return ek::tzcnt(a); });
        cls.def("lzcnt_", [](const Array &a) { return ek::lzcnt(a); });
        cls.def("popcnt_", [](const Array &a) { return ek::popcnt(a); });
        cls.def("sl_", [](const Array &a, const Array &b) { return a.sl_(b); });
        cls.def("sr_", [](const Array &a, const Array &b) { return a.sr_(b); });
        cls.def("isl_", [](Array *a, const Array &b) { *a = a->sl_(b); return a; });
        cls.def("isr_", [](Array *a, const Array &b) { *a = a->sr_(b); return a; });
    }

    if constexpr (Array::IsFloat) {
        cls.def("sin_", [](const Array &a) { return ek::sin(a); });
        cls.def("cos_", [](const Array &a) { return ek::cos(a); });
        cls.def("sincos_", [](const Array &a) { return ek::sincos(a); });
        cls.def("tan_", [](const Array &a) { return ek::tan(a); });
        cls.def("csc_", [](const Array &a) { return ek::csc(a); });
        cls.def("sec_", [](const Array &a) { return ek::sec(a); });
        cls.def("cot_", [](const Array &a) { return ek::cot(a); });
        cls.def("asin_", [](const Array &a) { return ek::asin(a); });
        cls.def("acos_", [](const Array &a) { return ek::acos(a); });
        cls.def("atan_", [](const Array &a) { return ek::atan(a); });
        cls.def("atan2_", [](const Array &y, const Array &x) { return ek::atan2(y, x); });
        cls.def("exp_", [](const Array &a) { return ek::exp(a); });
        cls.def("exp2_", [](const Array &a) { return ek::exp2(a); });
        cls.def("log_", [](const Array &a) { return ek::log(a); });
        cls.def("log2_", [](const Array &a) { return ek::log2(a); });
        cls.def("pow_", [](const Array &x, Scalar y) { return ek::pow(x, y); });
        cls.def("pow_", [](const Array &x, const Array &y) { return ek::pow(x, y); });
        cls.def("sinh_", [](const Array &a) { return ek::sinh(a); });
        cls.def("cosh_", [](const Array &a) { return ek::cosh(a); });
        cls.def("sincosh_", [](const Array &a) { return ek::sincosh(a); });
        cls.def("tanh_", [](const Array &a) { return ek::tanh(a); });
        cls.def("asinh_", [](const Array &a) { return ek::asinh(a); });
        cls.def("acosh_", [](const Array &a) { return ek::acosh(a); });
        cls.def("atanh_", [](const Array &a) { return ek::atanh(a); });
        cls.def("cbrt_", [](const Array &a) { return ek::cbrt(a); });
        cls.def("erf_", [](const Array &a) { return ek::erf(a); });
        cls.def("erfinv_", [](const Array &a) { return ek::erfinv(a); });
        cls.def("lgamma_", [](const Array &a) { return ek::lgamma(a); });
        cls.def("tgamma_", [](const Array &a) { return ek::tgamma(a); });
    }

    if constexpr (Array::IsJIT || Array::IsDiff) {
        cls.def("set_label_", [](const Array &a, const char *name) { a.set_label_(name); });
        cls.def("label_", [](const Array &a) { return a.label_(); });
    }

    if constexpr (!ek::is_mask_v<Array> || ek::is_dynamic_v<Array>) {
        cls.def_static("load_", [](uintptr_t ptr, size_t size) {
            return enoki::load<Array>((const void *) ptr, size);
        });
    }

    if constexpr (ek::is_jit_array_v<Array>) {
        cls.def_static("map_", [](uintptr_t ptr, size_t size, std::function<void (void)> callback) {
            Array result = Array::map_((void *) ptr, size, false);
            if (callback) {
                std::function<void (void)> *func = new std::function<void (void)>(std::move(callback));
                jitc_var_set_free_callback(ek::detach(result).index(), [](void *arg) {
                    std::function<void(void)> *func2 =
                        (std::function<void(void)> *) arg;
                    (*func2)();
                    delete func2;
                }, func);
            }
            return result;
        }, "ptr"_a, "size"_a, "callback"_a = py::none());
    }

    if constexpr (Array::IsJIT)
        cls.def("migrate_", &Array::migrate_);

    if constexpr (Array::IsDiff || (!Array::IsDiff && Array::IsJIT))
        cls.def("index", &Array::index);

    if constexpr (!Array::IsDiff && Array::IsJIT)
        cls.def("set_index_",
                [](Array &a, uint32_t index) { *a.index_ptr() = index; });

    if constexpr (Array::IsDiff) {
        cls.def(py::init<ek::detached_t<Array>>());
        cls.def("detach_", py::overload_cast<>(&Array::detach_),
                py::return_value_policy::reference_internal);
        if constexpr (Array::IsFloat) {
            cls.def("grad_", &Array::grad_);
            cls.def("set_grad_", &Array::set_grad_);
            cls.def("accum_grad_", &Array::accum_grad_);
            cls.def("set_grad_enabled_", &Array::set_grad_enabled_);
            cls.def("set_grad_suspended_", &Array::set_grad_suspended_);
            cls.def("enqueue_", &Array::enqueue_);
            cls.def("graphviz_", &Array::graphviz_);
            cls.def_static("traverse_", &Array::traverse_, py::call_guard<py::gil_scoped_release>());

            cls.def_static("create_", [](uint32_t index,
                                         const ek::detached_t<Array> &value) {
                ek::detail::ad_inc_ref_impl<ek::detached_t<Array>>(index);
                return Array::create(index, ek::detached_t<Array>(value));
            });
        }
    }

    bind_generic_constructor(cls);

    return cls;
}

#define ENOKI_BIND_ARRAY_TYPES_DYN(Module, Guide, Scalar)                      \
    auto d_b = bind<ek::mask_t<ek::DynamicArray<ek::float32_array_t<Guide>>>>( \
        Module, Scalar);                                                       \
    auto d_i32 =                                                               \
        bind<ek::DynamicArray<ek::int32_array_t<Guide>>>(Module, Scalar);      \
    auto d_u32 =                                                               \
        bind<ek::DynamicArray<ek::uint32_array_t<Guide>>>(Module, Scalar);     \
    auto d_i64 =                                                               \
        bind<ek::DynamicArray<ek::int64_array_t<Guide>>>(Module, Scalar);      \
    auto d_u64 =                                                               \
        bind<ek::DynamicArray<ek::uint64_array_t<Guide>>>(Module, Scalar);     \
    auto d_f32 =                                                               \
        bind<ek::DynamicArray<ek::float32_array_t<Guide>>>(Module, Scalar);    \
    auto d_f64 =                                                               \
        bind<ek::DynamicArray<ek::float64_array_t<Guide>>>(Module, Scalar);    \
    (void) d_i32; (void) d_u32; (void) d_i64; (void) d_u64; (void) d_f32;      \
    (void) d_f64; (void) d_b;

#define ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, Dim)                 \
    bind<ek::mask_t<ek::Array<ek::float32_array_t<Guide>, Dim>>>(Module,       \
                                                                 Scalar);      \
    bind<ek::Array<ek::int32_array_t<Guide>, Dim>>(Module, Scalar);            \
    bind<ek::Array<ek::uint32_array_t<Guide>, Dim>>(Module, Scalar);           \
    bind<ek::Array<ek::int64_array_t<Guide>, Dim>>(Module, Scalar);            \
    bind<ek::Array<ek::uint64_array_t<Guide>, Dim>>(Module, Scalar);           \
    bind<ek::Array<ek::float32_array_t<Guide>, Dim>>(Module, Scalar);          \
    bind<ek::Array<ek::float64_array_t<Guide>, Dim>>(Module, Scalar);

#define ENOKI_BIND_COMPLEX_TYPES(Module, Guide, Scalar)                        \
    bind<ek::Complex<ek::float32_array_t<Guide>>>(Module, Scalar);             \
    bind<ek::Complex<ek::float64_array_t<Guide>>>(Module, Scalar);             \

#define ENOKI_BIND_QUATERNION_TYPES(Module, Guide, Scalar)                     \
    bind<ek::Quaternion<ek::float32_array_t<Guide>>>(Module, Scalar);          \
    bind<ek::Quaternion<ek::float64_array_t<Guide>>>(Module, Scalar);

#define ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide, Scalar, Dim)                \
    bind<ek::Matrix<ek::float32_array_t<Guide>, Dim>>(Module, Scalar);         \
    bind<ek::Matrix<ek::float64_array_t<Guide>, Dim>>(Module, Scalar);

#define ENOKI_BIND_ARRAY_TYPES(Module, Guide, Scalar)                          \
    ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, 0)                       \
    ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, 1)                       \
    ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, 2)                       \
    ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, 3)                       \
    ENOKI_BIND_ARRAY_TYPES_DIM(Module, Guide, Scalar, 4)                       \
    ENOKI_BIND_ARRAY_TYPES_DYN(Module, Guide, Scalar)                          \
    bind<ek::mask_t<ek::Array<ek::Array<Guide, 2>, 2>>>(Module, Scalar);       \
    bind<ek::mask_t<ek::Array<ek::Array<Guide, 3>, 3>>>(Module, Scalar);       \
    bind<ek::mask_t<ek::Array<ek::Array<Guide, 4>, 4>>>(Module, Scalar);       \
    ENOKI_BIND_COMPLEX_TYPES(Module, Guide, Scalar)                            \
    ENOKI_BIND_QUATERNION_TYPES(Module, Guide, Scalar)                         \
    ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide, Scalar, 2)                      \
    ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide, Scalar, 3)                      \
    ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide, Scalar, 4)                      \
                                                                               \
    using Guide1 = ek::Array<Guide, 1>;                                        \
    using Guide4 = ek::Array<Guide, 4>;                                        \
    bind<ek::mask_t<ek::Array<ek::float32_array_t<Guide1>, 2>>>(Module,        \
                                                                Scalar);       \
    bind<ek::mask_t<ek::Array<ek::float32_array_t<Guide4>, 2>>>(Module,        \
                                                                Scalar);       \
    bind<ek::mask_t<ek::Array<Guide1, 4>>>(Module, Scalar);                    \
    bind<ek::mask_t<ek::Array<ek::Array<Guide1, 4>, 4>>>(Module, Scalar);      \
    bind<ek::mask_t<ek::Array<ek::Array<Guide4, 4>, 4>>>(Module, Scalar);      \
    bind<ek::Array<ek::float32_array_t<Guide1>, 4>>(Module, Scalar);           \
    bind<ek::Array<ek::float64_array_t<Guide1>, 4>>(Module, Scalar);           \
    bind<ek::Array<ek::float32_array_t<Guide4>, 4>>(Module, Scalar);           \
    bind<ek::Array<ek::float64_array_t<Guide4>, 4>>(Module, Scalar);           \
    ENOKI_BIND_COMPLEX_TYPES(Module, Guide1, Scalar)                           \
    ENOKI_BIND_COMPLEX_TYPES(Module, Guide4, Scalar)                           \
    ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide1, Scalar, 4)                     \
    ENOKI_BIND_MATRIX_TYPES_DIM(Module, Guide4, Scalar, 4)                     \

#define ENOKI_BIND_ARRAY_BASE(Module, Guide, Scalar)                           \
    auto a_msk =                                                               \
        bind_type<ek::mask_t<ek::float32_array_t<Guide>>>(Module, Scalar);     \
    auto a_i32 = bind_type<ek::int32_array_t<Guide>>(Module, Scalar);          \
    auto a_u32 = bind_type<ek::uint32_array_t<Guide>>(Module, Scalar);         \
    auto a_i64 = bind_type<ek::int64_array_t<Guide>>(Module, Scalar);          \
    auto a_u64 = bind_type<ek::uint64_array_t<Guide>>(Module, Scalar);         \
    auto a_f32 = bind_type<ek::float32_array_t<Guide>>(Module, Scalar);        \
    auto a_f64 = bind_type<ek::float64_array_t<Guide>>(Module, Scalar);        \
    Module.attr("Int32") = Module.attr("Int");                                 \
    Module.attr("UInt32") = Module.attr("UInt");                               \
    Module.attr("Float32") = Module.attr("Float");                             \
    bind_full(a_i32, Scalar);                                                  \
    bind_full(a_u32, Scalar);                                                  \
    bind_full(a_i64, Scalar);                                                  \
    bind_full(a_u64, Scalar);                                                  \
    bind_full(a_f32, Scalar);                                                  \
    bind_full(a_f64, Scalar);                                                  \
    bind_full(a_msk, Scalar);
