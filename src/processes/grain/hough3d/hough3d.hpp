#pragma once

#include <cmath>
#include <vector>

#include <common/sand.h>

namespace sand::grain {

    // Evenly distribute point across sphere surface using golden spiral method (Fibonacci's sphere)
    std::vector<dir_3d> fibonacci_sphere_versors(size_t n_versors) {
        std::vector<dir_3d> versors;
        versors.reserve(n_versors);

        if (n_versors == 0) {
            UFW_ERROR("n_versors must be greater than 0");
            return versors;
        } 

        const double golden_angle = M_PI * (3.0 - std::sqrt(5.0));

        for (size_t i = 0; i < n_versors; ++i)
        {
            const double z = 1.0 - 2.0 * (i + 0.5) / n_versors;
            const double r = std::sqrt(1.0 - z * z);
            const double theta = golden_angle * i;

            versors.push_back({r * std::cos(theta), r * std::sin(theta), z});
        }

        return versors;
    }

    // Remove redundant symmetries
    std::vector<dir_3d> select_unique_versors(const std::vector<dir_3d>& versors) {
        std::vector<dir_3d> unique_versors;
        unique_versors.reserve(versors.size());

        constexpr double eps = 1e-12;

        for (const auto& v : versors) {
            if (v.z() > 0 || (std::abs(v.z()) < eps && v.y() > 0) || (std::abs(v.z()) < eps && std::abs(v.y()) < eps && v.x() > 0)) {
                unique_versors.push_back(v);
            }
        }

        return unique_versors;
    }


} // namespace sand::grain