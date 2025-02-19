#if defined(ENOKI_ENABLE_JIT) && defined(ENOKI_ENABLE_AUTODIFF)
#include "bind.h"
#include "random.h"
#include <enoki/autodiff.h>
#include <enoki/llvm.h>

struct CustomOp : ek::detail::DiffCallback {
    CustomOp(py::handle handle) : m_handle(handle) {
        m_handle.inc_ref();
    }

    virtual void forward() override {
        py::gil_scoped_acquire gsa;
        m_handle.attr("forward")();
    }

    virtual void backward() override {
        py::gil_scoped_acquire gsa;
        m_handle.attr("backward")();
    }

    ~CustomOp() {
        py::gil_scoped_acquire gsa;
        m_handle.dec_ref();
    }

    py::handle m_handle;
};

void export_llvm_ad(py::module_ &m) {
    py::module_ llvm_ad = m.def_submodule("llvm").def_submodule("ad");

    using Guide = ek::DiffArray<ek::LLVMArray<float>>;
    ENOKI_BIND_ARRAY_BASE(llvm_ad, Guide, false);
    ENOKI_BIND_ARRAY_TYPES(llvm_ad, Guide, false);

    bind_pcg32<Guide>(llvm_ad);
    llvm_ad.attr("Loop") = m.attr("llvm").attr("Loop");

    py::module_ detail = llvm_ad.def_submodule("detail");
    detail.def("ad_add_edge", [](int32_t src_index, int32_t dst_index,
                                 py::handle cb) {
        ek::detail::ad_add_edge<ek::LLVMArray<float>>(
            src_index, dst_index, cb.is_none() ? nullptr : new CustomOp(cb));
    }, "src_index"_a, "dst_index"_a, "cb"_a = py::none());
}
#endif
