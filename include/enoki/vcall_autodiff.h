/*
    enoki/vcall.h -- Vectorized method call support, autodiff part

    Enoki is a C++ template library for efficient vectorization and
    differentiation of numerical kernels on modern processor architectures.

    Copyright (c) 2020 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#include <enoki/custom.h>

NAMESPACE_BEGIN(enoki)
NAMESPACE_BEGIN(detail)

template <typename Self, typename Result, typename Func, typename FuncFwd, typename FuncRev, typename... Args>
struct DiffVCall : CustomOp<leaf_array_t<Result, Args...>, Result, Self, Func, FuncFwd, FuncRev, Args...> {
    using Base = CustomOp<leaf_array_t<Result, Args...>, Result, Self, Func, FuncFwd, FuncRev, Args...>;

    static constexpr bool ClearPrimal = false;

    detached_t<Result> eval(const detached_t<Self> &self,
                            const Func &func,
                            const FuncFwd &func_fwd,
                            const FuncRev &func_rev,
                            const detached_t<Args> &... args) override {
        return detach(dispatch_jit_symbolic<Result>(func, self, args...));
    }

    template <size_t... Is>
    void forward_impl(std::index_sequence<Is...>) {
        const detached_t<Self> &self = detach(Base::m_grad_input->template get<0>());
        const FuncFwd &func_fwd = Base::m_grad_input->template get<2>();
        using ResultFwd = detached_t<Result>;

        ResultFwd grad_out = dispatch_jit_symbolic<ResultFwd>(
            func_fwd, self, detail::tuple(Base::template grad_in<4 + Is>()...),
            Base::template value_in<4 + Is>()...);

        if (jitc_mode() != JitMode::SymbolicRequired)
            enoki::eval(grad_out);

        Base::set_grad_out(grad_out);
    }

    template <size_t... Is>
    void backward_impl(std::index_sequence<Is...>) {
        const detached_t<Self> &self = detach(Base::m_grad_input->template get<0>());
        const FuncRev &func_rev = Base::m_grad_input->template get<3>();
        using ResultRev = detail::tuple<detached_t<Args>...>;

        ResultRev grad_in = dispatch_jit_symbolic<ResultRev>(
            func_rev, self, Base::grad_out(),
            Base::template value_in<4 + Is>()...);

        if (jitc_mode() != JitMode::SymbolicRequired)
            enoki::eval(grad_in);

        (Base::template set_grad_in<4 + Is>(grad_in.template get<Is>()), ...);
    }

    void forward() override {
        forward_impl(std::make_index_sequence<sizeof...(Args)>());
    }

    void backward() override {
        backward_impl(std::make_index_sequence<sizeof...(Args)>());
    }

    const char *name() const override {
        return "vcall";
    }
};

template <typename Result, typename Func, typename FuncFwd, typename FuncRev,
          typename Self, typename... Args>
ENOKI_INLINE Result dispatch_autodiff(const Func &func, const FuncFwd &func_fwd,
                                      const FuncRev &func_rev, const Self &self,
                                      const Args &... args) {
    using Type = leaf_array_t<Result, Args...>;

    if constexpr (is_diff_array_v<Type> && std::is_floating_point_v<scalar_t<Type>>) {
        return custom<DiffVCall<Self, Result, Func, FuncFwd, FuncRev, Args...>>(
            self, func, func_fwd, func_rev, args...);
    } else {
        return detach(dispatch_jit_symbolic<Result>(func, detach(self), args...));
    }
}

NAMESPACE_END(detail)
NAMESPACE_END(enoki)
