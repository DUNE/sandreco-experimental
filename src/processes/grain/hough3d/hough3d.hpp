#pragma once

#include <cmath>
#include <vector>

#include <common/sand.h>
#include <ocl/ocl.hpp>

namespace sand::grain {

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

    cl_float3 get_line_point(const cl_float4& versor, float x_prime, float y_prime) {
        const float inv = 1.0f / (1.0f + versor.s[2]);
        return cl_float3{x_prime * (1.0f - versor.s[0] * versor.s[0] * inv) - y_prime * versor.s[0] * versor.s[1] * inv,
                        - x_prime * versor.s[0] * versor.s[1] * inv + y_prime * (1.0f - versor.s[1] * versor.s[1] * inv),
                        - x_prime * versor.s[0] - y_prime * versor.s[1]};
    }

} // namespace sand::grain