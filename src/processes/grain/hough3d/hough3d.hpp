#pragma once

#include <cmath>
#include <vector>

#include <common/sand.h>
#include <ocl/ocl.hpp>

namespace sand::grain {

    // Evenly distribute point across sphere surface using golden spiral method (Fibonacci's sphere)
    std::vector<cl_float3> fibonacci_sphere_versors(size_t n_versors) {
        std::vector<cl_float3> versors;
        versors.reserve(n_versors);

        if (n_versors == 0) {
            UFW_ERROR("n_versors must be greater than 0");
            return versors;
        } 

        const float golden_angle = M_PI * (3.0 - std::sqrt(5.0));

        for (size_t i = 0; i < n_versors; ++i)
        {
            const float z = 1.0 - 2.0 * (i + 0.5) / n_versors;
            const float r = std::sqrt(1.0 - z * z);
            const float theta = golden_angle * i;

            versors.push_back({r * std::cos(theta), r * std::sin(theta), z});
        }

        return versors;
    }

    // Remove redundant symmetries
    std::vector<cl_float3> select_unique_versors(const std::vector<cl_float3>& versors) {
        std::vector<cl_float3> unique_versors;
        unique_versors.reserve(versors.size());

        constexpr double eps = 1e-12;

        for (const auto& v : versors) {
            if (v.s[2] > 0 || (std::abs(v.s[2]) < eps && v.s[1] > 0) || (std::abs(v.s[2]) < eps && std::abs(v.s[1]) < eps && v.s[0] > 0)) {
                unique_versors.push_back(v);
            }
        }

        return unique_versors;
    }

    // Convert 3d point cloud to cl_float4 for GPU processing
    std::vector<cl_float4> point_cloud_to_float4(const std::vector<point_cloud::point>& points) {
        std::vector<cl_float4> converted_points;
        converted_points.reserve(points.size());
        for (const auto& p : points) {
            converted_points.push_back({static_cast<float>(p.position.x()), static_cast<float>(p.position.y()), static_cast<float>(p.position.z()), static_cast<float>(p.amplitude)});
        }
        return converted_points;
    }


} // namespace sand::grain