#pragma once
#include <ocl/ocl.hpp>
#include <common/sand.h>

/**
 * In the common_structs.cl file we define a set of structs that we want to use both in
 * host and device code. To keep these types in sync, we define them only once and include
 * the same file both in all .cl programs and in here.
 * We redefine the CL_STRUCT macro to account for differences.
 * @note that rect_f is redundant since geoinfo::grain_info::rect_f already exists, and we prefer 
 * the latter as it is more general. These two MUST be manually kept aligned.
 */

#undef CL_STRUCT
#define CL_STRUCT(s) s

using float4 = cl_float4;
using int3   = cl_int3;

namespace sand::grain {
#include "../cl_src/common_structs.cl"

  transform_t to_ocl_xform(const xform_3d& root_xform) {
    double elem[12];
    root_xform.GetComponents(elem);
    transform_t ocl_xform;
    ocl_xform.x = {static_cast<cl_float>(elem[0]), static_cast<cl_float>(elem[4]), static_cast<cl_float>(elem[8]),
                   0.0f};
    ocl_xform.y = {static_cast<cl_float>(elem[1]), static_cast<cl_float>(elem[5]), static_cast<cl_float>(elem[9]),
                   0.0f};
    ocl_xform.z = {static_cast<cl_float>(elem[2]), static_cast<cl_float>(elem[6]), static_cast<cl_float>(elem[10]),
                   0.0f};
    ocl_xform.w = {static_cast<cl_float>(elem[3]), static_cast<cl_float>(elem[7]), static_cast<cl_float>(elem[11]),
                   1.0f};
    return ocl_xform;
  }
} // namespace sand::grain

template <>
struct fmt::formatter<sand::grain::frustum_t> : formatter<string_view> {
  auto format(const sand::grain::frustum_t& f, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(),
                          "  top_o: ({}, {}, {}, {})\n"
                          "  top_n: ({}, {}, {}, {})\n"
                          "  rgt_o: ({}, {}, {}, {})\n"
                          "  rgt_n: ({}, {}, {}, {})\n"
                          "  bot_o: ({}, {}, {}, {})\n"
                          "  bot_n: ({}, {}, {}, {})\n"
                          "  lft_o: ({}, {}, {}, {})\n"
                          "  lft_n: ({}, {}, {}, {})\n"
                          "  idx  : ({}, {}, {})\n",
                          f.top_o.s[0], f.top_o.s[1], f.top_o.s[2], f.top_o.s[3], f.top_n.s[0], f.top_n.s[1],
                          f.top_n.s[2], f.top_n.s[3], f.rgt_o.s[0], f.rgt_o.s[1], f.rgt_o.s[2], f.rgt_o.s[3],
                          f.rgt_n.s[0], f.rgt_n.s[1], f.rgt_n.s[2], f.rgt_n.s[3], f.bot_o.s[0], f.bot_o.s[1],
                          f.bot_o.s[2], f.bot_o.s[3], f.bot_n.s[0], f.bot_n.s[1], f.bot_n.s[2], f.bot_n.s[3],
                          f.lft_o.s[0], f.lft_o.s[1], f.lft_o.s[2], f.lft_o.s[3], f.lft_n.s[0], f.lft_n.s[1],
                          f.lft_n.s[2], f.lft_n.s[3], f.idx.s[0], f.idx.s[1], f.idx.s[2]);
  }
};

template <>
struct fmt::formatter<sand::grain::transform_t> : formatter<string_view> {
  auto format(const sand::grain::transform_t& t, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(),
                          "t.x = ({:.2f}, {:.2f}, {:.2f}, {:.2f})\n"
                          "t.y = ({:.2f}, {:.2f}, {:.2f}, {:.2f})\n"
                          "t.z = ({:.2f}, {:.2f}, {:.2f}, {:.2f})\n"
                          "t.w = ({:.2f}, {:.2f}, {:.2f}, {:.2f})",
                          t.x.s[0], t.x.s[1], t.x.s[2], t.x.s[3], t.y.s[0], t.y.s[1], t.y.s[2], t.y.s[3], t.z.s[0],
                          t.z.s[1], t.z.s[2], t.z.s[3], t.w.s[0], t.w.s[1], t.w.s[2], t.w.s[3]);
  }
};

#undef CL_STRUCT
#define CL_STRUCT(s) CL_STRUCT_BASE(s)