#pragma once

#include <cmath>
#include <vector>

#include <common/sand.h>
#include <ocl/ocl.hpp>

namespace sand::grain {

    // Evenly distribute point across sphere surface using golden spiral method (Fibonacci's sphere)
    // Using float4 in to be consistent with points
    std::vector<cl_float4> fibonacci_sphere_versors(size_t n_versors) {
        std::vector<cl_float4> versors;
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

            versors.push_back({r * std::cos(theta), r * std::sin(theta), z, 0.0});
        }

        return versors;
    }

    // Remove redundant symmetries selecting only upper emishpere
    std::vector<cl_float4> select_unique_versors(const std::vector<cl_float4>& versors) {
        std::vector<cl_float4> unique_versors;
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

    // Convert cl_float4 to 3d point cloud for writing ouput clusters
    std::vector<point_cloud::point> point_float4_to_cloud(const std::vector<cl_float4>& points) {
        std::vector<point_cloud::point> converted_points;
        converted_points.reserve(points.size());
        for (const auto& p : points) {
            converted_points.push_back({pos_3d{p.s[0], p.s[1], p.s[2]}, p.s[3]});
        }
        return converted_points;
    }

    // Remove and return neighbouring points
    // NOTE: it modifies the input points vector
    std::vector<cl_float4> filter_neighbouring_points(std::vector<cl_float4>& points, const std::vector<float>& distances, float max_clustering_distance) {
        // Safety check: ensure sizes match
        if (points.size() != distances.size()) {
            UFW_ERROR("Points size ({}) different from distances size ({})", points.size(), distances.size());
        }

        std::vector<cl_float4> clustered_points;
        clustered_points.reserve(points.size());

        size_t write_index = 0;
        for (size_t read_index = 0; read_index < distances.size(); ++read_index) {
            
            if (distances[read_index] <= max_clustering_distance) {
                clustered_points.push_back(points[read_index]); 
            }
            else {
                points[write_index] = std::move(points[read_index]);
                write_index++;
            }
        }

        // After the loop, truncate the vector points to its new filtered size.
        if (write_index < points.size()) {
            points.resize(write_index);
        }

        return clustered_points;
    }

} // namespace sand::grain