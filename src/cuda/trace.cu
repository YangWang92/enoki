#include <cuda.h>
#include <vector>
#include <iostream>
#include <array>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include "common.cuh"

/// Reserved registers for grid-stride loop indexing
#define ENOKI_CUDA_REG_RESERVED 10

/// Enable heavy debug output (instructions, reference counts, etc.)
#define ENOKI_CUDA_DEBUG_TRACE  0

/// Enable moderate debug output
#define ENOKI_CUDA_DEBUG_MODERATE 1

NAMESPACE_BEGIN(enoki)

// Forward declarations
void cuda_inc_ref_ext(uint32_t);
void cuda_inc_ref_int(uint32_t);
void cuda_dec_ref_ext(uint32_t);
void cuda_dec_ref_int(uint32_t);
void cuda_eval(bool log_assembly = false);

// -----------------------------------------------------------------------
//! @{ \name 'Variable' type that is used to record instruction traces
// -----------------------------------------------------------------------

struct Variable {
    /// Data type of this variable
    EnokiType type;

    /// PTX instruction to compute it
    std::string cmd;

    /// Associated comment (mainly for debugging)
    std::string comment;

    /// Number of entries
    size_t size = 1;

    /// Pointer to device memory
    void *data = nullptr;

    /// External (i.e. by Enoki) reference count
    uint32_t ref_count_ext = 0;

    /// Internal (i.e. within the PTX instruction stream) reference count
    uint32_t ref_count_int = 0;

    /// Dependencies of this instruction
    std::array<uint32_t, 3> dep = { 0, 0, 0 };

    /// Does the instruction have side effects (e.g. 'scatter')
    bool side_effect = false;

    /// A variable is 'dirty' if there are pending scatter operations to it
    bool dirty = false;

    /// Free 'data' after this variable is no longer referenced?
    bool free = true;

    /// Size of the (heuristic for instruction scheduling)
    uint32_t subtree_size = 0;

    Variable(EnokiType type) : type(type) { }

    ~Variable() { if (free) cuda_check(cudaFree(data)); }

    Variable(Variable &&a)
        : type(a.type), cmd(std::move(a.cmd)), comment(std::move(a.comment)),
          size(a.size), data(a.data), ref_count_ext(a.ref_count_ext),
          ref_count_int(a.ref_count_int), dep(a.dep),
          side_effect(a.side_effect), dirty(a.dirty),
          free(a.free), subtree_size(a.subtree_size) {
        a.data = nullptr;
        a.ref_count_int = a.ref_count_ext = 0;
        a.dep = { 0, 0, 0 };
    }

    bool is_collected() const {
        return ref_count_int == 0 && ref_count_ext == 0;
    }
};

/// The full instruction trace
static std::vector<Variable> __trace;

/// Contains "live" (externally referenced) variables and statements with side effects
static std::unordered_set<uint32_t> __active;
static std::vector<uint32_t> __dirty;

inline std::vector<Variable> &trace() { return __trace; }

ENOKI_EXPORT void cuda_init() {
    cudaFree(0); // initialize CUDA

    auto &tr = trace();
    tr.clear();
    tr.emplace_back(EnokiType::Invalid);
    tr.emplace_back(EnokiType::UInt64);
    while (tr.size() != ENOKI_CUDA_REG_RESERVED)
        tr.emplace_back(EnokiType::UInt32);
    __active.clear();
}

ENOKI_EXPORT void cuda_shutdown() {
    auto &tr = trace();
    tr.clear();
    __active.clear();
}

ENOKI_EXPORT void *cuda_var_ptr(uint32_t index) {
    return trace()[index].data;
}

ENOKI_EXPORT size_t cuda_var_size(uint32_t index) {
    return trace()[index].size;
}

ENOKI_EXPORT void cuda_var_set_comment(uint32_t index, const char *str) {
    trace()[index].comment = str;
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_var_set_comment(" << index << "): " << str << std::endl;
#endif
}

ENOKI_EXPORT void cuda_var_set_size(uint32_t index, size_t size) {
    trace()[index].size = size;
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_var_set_size(" << index << "): " << size << std::endl;
#endif
}

ENOKI_EXPORT uint32_t cuda_var_register(EnokiType type, size_t size, void *ptr,
                                        uint32_t parent, bool dealloc) {
    auto &tr = trace();
    uint32_t idx = (uint32_t) tr.size();
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_var_register(" << idx << "): " << ptr
              << ", size=" << size << ", parent=" << parent
              << ", dealloc=" << dealloc << std::endl;
#endif
    tr.emplace_back(type);
    auto &var = tr.back();
    var.dep = { parent, 0, 0 };
    var.cmd = "";
    var.data = ptr;
    var.size = size;
    var.free = dealloc;
    cuda_inc_ref_ext(idx);
    cuda_inc_ref_int(parent);
    return idx;
}

ENOKI_EXPORT void cuda_var_free(uint32_t idx) {
    Variable &v = trace()[idx];
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_var_free(" << idx << ") = " << v.data;
    if (!v.free)
        std::cerr << " (not deleted)";
    std::cerr << std::endl;
#endif
    for (int i = 0; i < 3; ++i) {
        cuda_dec_ref_int(v.dep[i]);
        v.dep[i] = 0;
    }
    if (v.free)
        cuda_check(cudaFree(v.data));
    v.data = nullptr;
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Common functionality to distinguish types
// -----------------------------------------------------------------------

ENOKI_EXPORT size_t cuda_register_size(EnokiType type) {
    switch (type) {
        case EnokiType::UInt8:
        case EnokiType::Int8:
        case EnokiType::Bool:    return 1;
        case EnokiType::UInt16:
        case EnokiType::Int16:   return 2;
        case EnokiType::UInt32:
        case EnokiType::Int32:   return 4;
        case EnokiType::Pointer:
        case EnokiType::UInt64:
        case EnokiType::Int64:   return 8;
        case EnokiType::Float32: return 4;
        case EnokiType::Float64: return 8;
        default: return (size_t) -1;
    }
}

ENOKI_EXPORT const char *cuda_register_type(EnokiType type) {
    switch (type) {
        case EnokiType::UInt8: return "u8";
        case EnokiType::Int8: return "s8";
        case EnokiType::UInt16: return "u16";
        case EnokiType::Int16: return "s16";
        case EnokiType::UInt32: return "u32";
        case EnokiType::Int32: return "s32";
        case EnokiType::Pointer:
        case EnokiType::UInt64: return "u64";
        case EnokiType::Int64: return "s64";
        case EnokiType::Float16: return "f16";
        case EnokiType::Float32: return "f32";
        case EnokiType::Float64: return "f64";
        case EnokiType::Bool: return "pred";
        default: return nullptr;
    }
}

ENOKI_EXPORT const char *cuda_register_type_bin(EnokiType type) {
    switch (type) {
        case EnokiType::UInt8:
        case EnokiType::Int8: return "b8";
        case EnokiType::UInt16:
        case EnokiType::Float16:
        case EnokiType::Int16: return "b16";
        case EnokiType::Float32:
        case EnokiType::UInt32:
        case EnokiType::Int32: return "b32";
        case EnokiType::Pointer:
        case EnokiType::Float64:
        case EnokiType::UInt64:
        case EnokiType::Int64: return "b64";
        case EnokiType::Bool: return "pred";
        default: return nullptr;
    }
}

ENOKI_EXPORT const char *cuda_register_name(EnokiType type) {
    switch (type) {
        case EnokiType::UInt8:
        case EnokiType::Int8:   return "%b";
        case EnokiType::UInt16:
        case EnokiType::Int16:   return "%w";
        case EnokiType::UInt32:
        case EnokiType::Int32:   return "%r";
        case EnokiType::Pointer:
        case EnokiType::UInt64:
        case EnokiType::Int64:   return "%rd";
        case EnokiType::Float32: return "%f";
        case EnokiType::Float64: return "%d";
        case EnokiType::Bool:    return "%p";
        default: return nullptr;
    }
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Reference counting (internal means: dependency within
//           JIT trace, external means: referenced by an Enoki array)
// -----------------------------------------------------------------------

ENOKI_EXPORT void cuda_inc_ref_ext(uint32_t index) {
    if (index < ENOKI_CUDA_REG_RESERVED)
        return;
    auto &tr = trace();
    assert(index < tr.size());
    tr[index].ref_count_ext++;

#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_inc_ref_ext(" << index << ") "
              << " -> " << tr[index].ref_count_ext << std::endl;
#endif
}

ENOKI_EXPORT void cuda_inc_ref_int(uint32_t index) {
    if (index < ENOKI_CUDA_REG_RESERVED)
        return;
    auto &tr = trace();
    assert(index < tr.size());
    tr[index].ref_count_int++;

#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_inc_ref_int(" << index << ") "
              << " -> " << tr[index].ref_count_int << std::endl;
#endif
}

ENOKI_EXPORT void cuda_dec_ref_ext(uint32_t index) {
    auto &tr = trace();
    if (index < ENOKI_CUDA_REG_RESERVED || tr.empty())
        return;
    assert(index < tr.size());
    Variable &v = tr[index];

#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_dec_ref_ext(" << index << ") "
              << " -> " << (v.ref_count_ext - 1) << std::endl;
#endif

    if (v.ref_count_ext == 0)
        throw std::runtime_error("cuda_dec_ref_ext(): Negative reference count!");

    v.ref_count_ext--;

    if (v.ref_count_ext == 0)
        __active.erase(index);

    if (v.is_collected())
        cuda_var_free(index);

}

ENOKI_EXPORT void cuda_dec_ref_int(uint32_t index) {
    if (index < ENOKI_CUDA_REG_RESERVED)
        return;
    auto &tr = trace();
    if (tr.empty())
        throw std::runtime_error("CUDA tracing JIT is uninitialized!");

    assert(index < tr.size());

    Variable &v = tr[index];

#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_dec_ref_int(" << index << ") "
              << " -> " << (v.ref_count_int - 1) << std::endl;
#endif

    v.ref_count_int--;

    if (v.is_collected())
        cuda_var_free(index);
}

ENOKI_EXPORT void cuda_var_mark_side_effect(uint32_t index) {
    auto &tr = trace();
    assert(index > ENOKI_CUDA_REG_RESERVED && index < tr.size());
    tr[index].side_effect = true;
}

ENOKI_EXPORT void cuda_var_mark_dirty(uint32_t index) {
    auto &tr = trace();
    assert(index > ENOKI_CUDA_REG_RESERVED && index < tr.size());
    tr[index].dirty = true;
    __dirty.push_back(index);
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name JIT trace append routines
// -----------------------------------------------------------------------

ENOKI_EXPORT uint32_t cuda_trace_append(EnokiType type,
                                        const char *cmd) {
    auto &tr = trace();
    uint32_t idx = (uint32_t) tr.size();
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_trace_append(" << idx << "): " << cmd << std::endl;
#endif
    tr.emplace_back(type);
    auto &var = tr.back();
    var.dep = { 0, 0, 0 };
    var.cmd = cmd;
    var.subtree_size = 1;
    cuda_inc_ref_ext(idx);
    __active.insert(idx);
    return idx;
}

ENOKI_EXPORT uint32_t cuda_trace_append(EnokiType type,
                                        const char *cmd,
                                        uint32_t arg1) {
    auto &tr = trace();

    assert(arg1 < tr.size());
    if (tr[arg1].dirty)
        cuda_eval();

    uint32_t idx = (uint32_t) tr.size();
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_trace_append(" << idx << " <- " << arg1 << "): " << cmd
              << std::endl;
#endif
    tr.emplace_back(type);
    auto &var = tr.back();
    var.size = tr[arg1].size;
    var.dep = { arg1, 0, 0 };
    var.cmd = cmd;
    var.subtree_size = tr[arg1].subtree_size + 1;
    cuda_inc_ref_int(arg1);
    cuda_inc_ref_ext(idx);
    __active.insert(idx);
    return idx;
}

ENOKI_EXPORT uint32_t cuda_trace_append(EnokiType type,
                                        const char *cmd,
                                        uint32_t arg1,
                                        uint32_t arg2) {
    auto &tr = trace();

    assert(arg1 < tr.size() && arg2 < tr.size());
    if (tr[arg1].dirty || tr[arg2].dirty)
        cuda_eval();

    uint32_t idx = (uint32_t) tr.size();
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_trace_append(" << idx << " <- " << arg1 << ", " << arg2
              << "): " << cmd << std::endl;
#endif

    tr.emplace_back(type);
    auto &var = tr.back();
    var.size = std::max(tr[arg1].size, tr[arg2].size);
    var.dep = { arg1, arg2, 0 };
    var.cmd = cmd;
    var.subtree_size = tr[arg1].subtree_size + tr[arg2].subtree_size + 1;
    cuda_inc_ref_int(arg1);
    cuda_inc_ref_int(arg2);
    cuda_inc_ref_ext(idx);
    __active.insert(idx);
    return idx;
}

ENOKI_EXPORT uint32_t cuda_trace_append(EnokiType type,
                                        const char *cmd,
                                        uint32_t arg1,
                                        uint32_t arg2,
                                        uint32_t arg3) {
    auto &tr = trace();

    assert(arg1 < tr.size() && arg2 < tr.size() && arg3 < tr.size());
    if (tr[arg1].dirty || tr[arg2].dirty || tr[arg3].dirty)
        cuda_eval();

    uint32_t idx = (uint32_t) tr.size();
#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "cuda_trace_append(" << idx << " <- " << arg1 << ", " << arg2
              << ", " << arg3 << "): " << cmd << std::endl;
#endif
    tr.emplace_back(type);
    auto &var = tr.back();
    var.size = std::max(std::max(tr[arg1].size, tr[arg2].size), tr[arg3].size);
    var.dep = { arg1, arg2, arg3 };
    var.cmd = cmd;
    var.subtree_size = tr[arg1].subtree_size +
                       tr[arg2].subtree_size +
                       tr[arg3].subtree_size + 1;
    cuda_inc_ref_int(arg1);
    cuda_inc_ref_int(arg2);
    cuda_inc_ref_int(arg3);
    cuda_inc_ref_ext(idx);
    __active.insert(idx);
    return idx;
}

ENOKI_EXPORT void cuda_trace_printf(const char *fmt, uint32_t narg, uint32_t* arg) {
    auto &tr = trace();
    std::ostringstream oss;
    oss << "{" << std::endl
        << "        .global .align 1 .b8 fmt[] = {";
    for (int i = 0;; ++i) {
        oss << (unsigned) fmt[i];
        if (fmt[i] == '\0')
            break;
        oss << ", ";
    }
    oss << "};" << std::endl
        << "        .local .align 8 .b8 buf[" << 8*narg << "];" << std::endl
        << "        .reg.b64 %fmt_r, %buf_r;" << std::endl;
    for (int i = 0; i < narg; ++i) {
        assert(arg[i] < tr.size());

        switch (tr[arg[i]].type) {
            case EnokiType::Float32:
                oss << "        cvt.f64.f32 %d0, $r" << i + 2 << ";" << std::endl
                    << "        st.local.f64 [buf + " << i * 8 << "], %d0;" << std::endl;
                break;

            default:
                oss << "        st.local.$t" << i + 2 << " [buf + " << i * 8 << "], $r" << i + 2 << ";" << std::endl;
                break;
        }
    }
    oss << "        cvta.global.u64 %fmt_r, fmt;" << std::endl
        << "        cvta.local.u64 %buf_r, buf;" << std::endl
        << "        {" << std::endl
        << "            .param .b64 fmt_p;" << std::endl
        << "            .param .b64 buf_p;" << std::endl
        << "            .param .b32 rv_p;" << std::endl
        << "            st.param.b64 [fmt_p], %fmt_r;" << std::endl
        << "            st.param.b64 [buf_p], %buf_r;" << std::endl
        << "            call.uni (rv_p), vprintf, (fmt_p, buf_p);" << std::endl
        << "        }" << std::endl
        << "    }" << std::endl;

    uint32_t idx = 0;
    if (narg == 0)
        cuda_trace_append(EnokiType::UInt32, oss.str().c_str());
    else if (narg == 1)
        cuda_trace_append(EnokiType::UInt32, oss.str().c_str(), arg[0]);
    else if (narg == 2)
        cuda_trace_append(EnokiType::UInt32, oss.str().c_str(), arg[0], arg[1]);
    else if (narg == 3)
        cuda_trace_append(EnokiType::UInt32, oss.str().c_str(), arg[0], arg[1], arg[2]);
    else
        throw std::runtime_error("cuda_trace_print(): at most 3 variables can be printed at once!");

    cuda_var_mark_side_effect(idx);
}

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name JIT trace generation
// -----------------------------------------------------------------------

static void cuda_render_cmd(std::ostringstream &oss,
                            const std::vector<Variable> &tr,
                            const std::unordered_map<uint32_t, uint32_t> &reg_map,
                            uint32_t index) {
    const Variable &var = tr[index];
    const std::string &cmd = var.cmd;

    oss << "    ";
    for (size_t i = 0; i < cmd.length(); ++i) {
        if (cmd[i] != '$' || i + 2 >= cmd.length()) {
            oss << cmd[i];
            continue;
        } else {
            uint8_t type = cmd[i + 1],
                    dep_offset = cmd[i + 2] - '0';

            if (type != 't' && type != 'b' && type != 'r')
                throw std::runtime_error("cuda_render_cmd: invalid '$' template!");

            if (dep_offset < 1 && dep_offset > 4)
                throw std::runtime_error("cuda_render_cmd: out of bounds!");

            uint32_t dep =
                dep_offset == 1 ? index : tr[index].dep[dep_offset - 2];
            if ((size_t) dep >= tr.size())
                throw std::runtime_error("cuda_render_cmd: out of bounds!");
            EnokiType dep_type = tr[dep].type;
            const char *result = nullptr;

            if (type == 't')
                result = cuda_register_type(dep_type);
            else if (type == 'b')
                result = cuda_register_type_bin(dep_type);
            else if (type == 'r')
                result = cuda_register_name(dep_type);

            if (result == nullptr)
                throw std::runtime_error(
                    "CUDABackend: internal error -- unsupported type: " +
                    std::string(cmd) + " marked with type " +
                    std::to_string(dep_type));

            oss << result;

            if (type == 'r') {
                auto it = reg_map.find(dep);
                if (it == reg_map.end())
                    throw std::runtime_error(
                        "CUDABackend: internal error -- variable not found!");
                oss << it->second;
            }

            i += 2;
        }
    }

    if (!cmd.empty() && cmd[cmd.length() - 1] != '\n')
        oss << ";" << std::endl;
}

static std::pair<std::string, std::vector<void *>>
cuda_jit_assemble(size_t size, const std::vector<uint32_t> &sweep) {
    auto &tr = trace();
    std::ostringstream oss;
    std::vector<void *> ptrs;
    size_t n_in = 0, n_out = 0, n_arith = 0;

    oss << ".version 6.3" << std::endl
        << ".target sm_75" << std::endl
        << ".address_size 64" << std::endl
        << std::endl;

#if ENOKI_CUDA_DEBUG_TRACE
    std::cerr << "Register map:" << std::endl;
#endif

    uint32_t n_vars = ENOKI_CUDA_REG_RESERVED;
    std::unordered_map<uint32_t, uint32_t> reg_map;
    for (uint32_t index : sweep) {
#if ENOKI_CUDA_DEBUG_TRACE
        std::cerr << "    " << n_vars << " -> " << index;
        if (!tr[index].comment.empty())
            std::cerr << ": " << tr[index].comment;
        std::cerr << std::endl;
#endif
        reg_map[index] = n_vars++;
    }
    reg_map[2] = 2;

    /**
     * rd0: ptr
     * r1: N
     * r2: index
     * r3: stride
     * r4: threadIdx
     * r5: blockIdx
     * r6: blockDim
     * r7: gridDim
     * rd8, rd9: address I/O
     */

    oss << ".extern .func  (.param .b32 rv) vprintf (" << std::endl
        << "    .param .b64 fmt," << std::endl
        << "    .param .b64 buf" << std::endl
        << ");" << std::endl
        << std::endl;

    oss << ".visible .entry enoki_kernel(.param .u64 ptr," << std::endl
        << "                             .param .u32 size) {" << std::endl
        << "    .reg.b8 %b<" << n_vars << ">;" << std::endl
        << "    .reg.b16 %w<" << n_vars << ">;" << std::endl
        << "    .reg.b32 %r<" << n_vars << ">;" << std::endl
        << "    .reg.b64 %rd<" << n_vars << ">;" << std::endl
        << "    .reg.f32 %f<" << n_vars << ">;" << std::endl
        << "    .reg.f64 %d<" << n_vars << ">;" << std::endl
        << "    .reg.pred %p<" << n_vars << ">;" << std::endl << std::endl
        << "    .reg.u64 %foo;"
        << std::endl
        << "    // Grid-stride loop setup" << std::endl
        << "    ld.param.u64 %rd0, [ptr];" << std::endl
        << "    cvta.to.global.u64 %rd0, %rd0;" << std::endl
        << "    ld.param.u32 %r1, [size];" << std::endl
        << "    mov.u32 %r4, %tid.x;" << std::endl
        << "    mov.u32 %r5, %ctaid.x;" << std::endl
        << "    mov.u32 %r6, %ntid.x;" << std::endl
        << "    mad.lo.u32 %r2, %r5, %r6, %r4;" << std::endl
        << "    setp.ge.u32 %p0, %r2, %r1;" << std::endl
        << "    @%p0 bra L0;" << std::endl
        << std::endl
        << "    mov.u32 %r7, %nctaid.x;" << std::endl
        << "    mul.lo.u32 %r3, %r6, %r7;" << std::endl
        << std::endl
        << "L1:" << std::endl
        << "    // Loop body" << std::endl;

    for (uint32_t index : sweep) {
        Variable &var = tr[index];
        if (var.is_collected() || (var.cmd.empty() && var.data == nullptr))
            throw std::runtime_error(
                "CUDABackend: found invalid/expired variable in schedule! " + std::to_string(index));

        if (var.size != 1 && var.size != size)
            throw std::runtime_error(
                "CUDABackend: encountered arrays of incompatible size!");

        oss << std::endl;
        if (var.data) {
            size_t idx = ptrs.size();
            ptrs.push_back(var.data);

            oss << std::endl
                << "    // Load register " << cuda_register_name(var.type) << reg_map[index];
            if (!var.comment.empty())
                oss << ": " << var.comment;
            oss << std::endl
                << "    ld.global.u64 %rd8, [%rd0 + " << idx * 8 << "];" << std::endl
                << "    cvta.to.global.u64 %rd8, %rd8;" << std::endl;
            if (var.size != 1) {
                oss << "    mul.wide.u32 %rd9, %r2, " << cuda_register_size(var.type) << ";" << std::endl
                    << "    add.u64 %rd8, %rd8, %rd9;" << std::endl;
            }
            if (var.type != EnokiType::Bool) {
                oss << "    ld.global." << cuda_register_type(var.type) << " "
                    << cuda_register_name(var.type) << reg_map[index] << ", [%rd8]"
                    << ";" << std::endl;
            } else {
                oss << "    ld.global.u8 %w1, [%rd8];" << std::endl
                    << "    setp.ne.u16 " << cuda_register_name(var.type) << reg_map[index] << ", %w1, 0;";
            }
            n_in++;
        } else {
            if (!var.comment.empty())
                oss << "    // Compute register "
                    << cuda_register_name(var.type) << reg_map[index] << ": "
                    << var.comment << std::endl;
            cuda_render_cmd(oss, tr, reg_map, index);
            n_arith++;

            if (var.side_effect) {
                cuda_dec_ref_ext(index);
                n_out++;
            }

            if (var.ref_count_ext == 0)
                continue;

            if (var.size != size)
                continue;

            size_t size_in_bytes =
                cuda_var_size(index) * cuda_register_size(var.type);

            cuda_check(cudaMalloc(&var.data, size_in_bytes));
#if ENOKI_CUDA_DEBUG_TRACE
            std::cerr << "cuda_eval(): allocated variable " << index
                      << " -> " << var.data << " (" << size_in_bytes
                      << " bytes)" << std::endl;
#endif
            size_t idx = ptrs.size();
            ptrs.push_back(var.data);
            n_out++;

            oss << std::endl
                << "    // Store register " << cuda_register_name(var.type) << reg_map[index];
            if (!var.comment.empty())
                oss << ": " << var.comment;
            oss << std::endl
                << "    ld.global.u64 %rd8, [%rd0 + " << idx*8 << "];" << std::endl
                << "    cvta.to.global.u64 %rd8, %rd8;" << std::endl
                << "    mul.wide.u32 %rd9, %r2, " << cuda_register_size(var.type) << ";" << std::endl
                << "    add.u64 %rd8, %rd8, %rd9;" << std::endl;

            if (var.type != EnokiType::Bool) {
                oss << "    st.global." << cuda_register_type(var.type) << " [%rd8], "
                    << cuda_register_name(var.type) << reg_map[index] << ";"
                    << std::endl;
            } else {
                oss << "    selp.u16 %w1, 1, 0, " << cuda_register_name(var.type)
                    << reg_map[index] << ";" << std::endl;
                oss << "    st.global.u8" << " [%rd8], %w1;" << std::endl;
            }
        }
    }

    oss << std::endl
        << "    add.u32     %r2, %r2, %r3;" << std::endl
        << "    setp.lt.u32 %p0, %r2, %r1;" << std::endl
        << "    @%p0 bra L1;" << std::endl;

    oss << std::endl
        << "L0:" << std::endl
        << "    @!%p0 st.global.u64 [%rd0], %foo;" << std::endl
        << "    ret;" << std::endl
        << "}" << std::endl;

#if ENOKI_CUDA_DEBUG_TRACE || ENOKI_CUDA_DEBUG_MODERATE
    std::cerr << "cuda_eval(): launching kernel (n=" << size << ", in="
              << n_in << ", out=" << n_out << ", ops=" << n_arith
              << ")" << std::endl;
#endif

    return { oss.str(), ptrs };
}

ENOKI_EXPORT void cuda_jit_run(const std::string &source,
                               const std::vector<void *> &ptrs_,
                               size_t size,
                               bool verbose) {
    if (verbose)
        std::cout << source << std::endl;

    CUjit_option arg[5];
    void *argv[5];
    char error_log[8192], info_log[8192];
    unsigned int logSize = 8192;
    arg[0] = CU_JIT_INFO_LOG_BUFFER;
    argv[0] = (void *) info_log;
    arg[1] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
    argv[1] = (void *) (long) logSize;
    arg[2] = CU_JIT_ERROR_LOG_BUFFER;
    argv[2] = (void *) error_log;
    arg[3] = CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES;
    argv[3] = (void *) (long) logSize;
    arg[4] = CU_JIT_LOG_VERBOSE;
    argv[4] = (void *) 1;

    CUlinkState link_state;
    cuda_check(cuLinkCreate(5, arg, argv, &link_state));

    int rt = cuLinkAddData(link_state, CU_JIT_INPUT_PTX, (void *) source.c_str(),
                           source.length(), nullptr, 0, nullptr, nullptr);
    if (rt != CUDA_SUCCESS) {
        printf("%i\n", rt);
        fprintf(stderr, "PTX Linker Error:\n%s\n", error_log);
        exit(-1);
    }

    void *link_output = nullptr;
    size_t link_output_size = 0;
    cuda_check(cuLinkComplete(link_state, &link_output, &link_output_size));
    if (rt != CUDA_SUCCESS) {
        fprintf(stderr, "PTX Linker Error:\n%s\n", error_log);
        exit(-1);
    }

    if (verbose)
        std::cerr << "CUDA Link completed. Linker output:" << std::endl
                  << info_log << std::endl;

    CUmodule module;
    cuda_check(cuModuleLoadData(&module, link_output));

    // Locate the kernel entry point
    CUfunction kernel;
    cuda_check(cuModuleGetFunction(&kernel, module, "enoki_kernel"));

    // Destroy the linker invocation
    cuda_check(cuLinkDestroy(link_state));

    int device, num_sm;
    cudaGetDevice(&device);
    cudaDeviceGetAttribute(&num_sm,
        cudaDevAttrMultiProcessorCount, device);

    void **ptrs = nullptr;
    cuda_check(cudaMalloc(&ptrs, ptrs_.size() * sizeof(void *)));
    cuda_check(cudaMemcpy(ptrs, ptrs_.data(), ptrs_.size() * sizeof(void *),
                          cudaMemcpyHostToDevice));

    void *args[2] = { &ptrs, &size };

    const unsigned int thread_count = 128;
    const unsigned int block_count = 32;

    cuda_check(cuLaunchKernel(kernel, block_count, 1, 1, thread_count,
                              1, 1, 0, nullptr, args, nullptr));

    cuda_check(cudaFree(ptrs));
    cuda_check(cuModuleUnload(module));
}

static void sweep_recursive(const std::vector<Variable> &tr,
                            std::unordered_set<uint32_t> &visited,
                            std::vector<uint32_t> &sweep,
                            uint32_t idx) {
    if (visited.find(idx) != visited.end())
        return;
    visited.insert(idx);

    std::array<uint32_t, 3> deps = tr[idx].dep;

    auto prio = [&](uint32_t i) -> uint32_t {
        uint32_t k = deps[i];
        if (k >= ENOKI_CUDA_REG_RESERVED)
            return tr[k].subtree_size;
        else
            return 0;
    };

    if (prio(1) < prio(2))
        std::swap(deps[0], deps[1]);
    if (prio(0) < prio(2))
        std::swap(deps[0], deps[2]);
    if (prio(0) < prio(1))
        std::swap(deps[0], deps[1]);

    for (uint32_t k : deps) {
        if (k >= ENOKI_CUDA_REG_RESERVED)
            sweep_recursive(tr, visited, sweep, k);
    }

    sweep.push_back(idx);
}

ENOKI_EXPORT void cuda_eval(bool log_assembly) {
    auto &tr = trace();
    std::unordered_map<size_t, std::pair<std::unordered_set<uint32_t>,
                                         std::vector<uint32_t>>> sweeps;

    for (uint32_t idx : __active) {
        auto &sweep = sweeps[tr[idx].size];
        sweep_recursive(tr, std::get<0>(sweep), std::get<1>(sweep), idx);
    }
    for (uint32_t idx : __dirty)
        tr[idx].dirty = false;

    __active.clear();
    __dirty.clear();

    for (auto const &sweep : sweeps) {
        size_t size = std::get<0>(sweep);
        const std::vector<uint32_t> &schedule = std::get<1>(std::get<1>(sweep));

        auto result = cuda_jit_assemble(size, schedule);
        if (std::get<0>(result).empty())
            continue;

#if ENOKI_CUDA_DEBUG_TRACE
        log_assembly = true;
#endif

        cuda_jit_run(std::get<0>(result),
                     std::get<1>(result),
                     std::get<0>(sweep),
                     log_assembly);

        for (uint32_t idx : schedule) {
            Variable &v = tr[idx];
            if (v.data != nullptr && !v.cmd.empty()) {
                for (int j = 0; j < 3; ++j) {
                    cuda_dec_ref_int(v.dep[j]);
                    v.dep[j] = 0;
                }
            }
        }
    }
}

//! @}
// -----------------------------------------------------------------------

ENOKI_EXPORT void cuda_fetch_element(void *dst, uint32_t src, size_t index, size_t size) {
    auto &tr = trace();
    if (tr[src].data == nullptr || tr[src].dirty)
        cuda_eval();
    assert(!tr[src].dirty);
    cuda_check(cudaMemcpy(dst, (uint8_t *) (tr[src].data) + size * index,
                          size, cudaMemcpyDeviceToHost));
}

ENOKI_EXPORT void* cuda_managed_malloc(size_t size) {
    void *result = nullptr;
    cuda_check(cudaMallocManaged(&result, size));
    return result;
}

ENOKI_EXPORT void cuda_managed_free(void *ptr) {
    cuda_check(cudaFree(ptr));
}

NAMESPACE_END(enoki)
