#include <common/sand.h>
#include <grain/point_cloud.h>

#include <cmath>
#include <utility>
#include <vector>
#include <array>

#include <TMath.h>

namespace sand::grain {

    std::pair<pos_3d, dir_3d> line_params_to_p_v(const std::array<double,4>& line_params) {
        const double theta = line_params[0];
        const double phi = line_params[1];
        const double x1 = line_params[2];
        const double y1 = line_params[3];

        // Compute the line's direction vector
        dir_3d line_v(std::cos(phi) * std::cos(theta), std::sin(phi) * std::cos(theta), std::sin(theta));
        // Compute a point on the line
        const auto& vx = line_v.x(); 
        const auto& vy = line_v.y();
        const auto& vz = line_v.z();       
        const double px = x1 * (1 - vx * vx/ (1 + vz)) - y1 * (vx * vy / (1 + vz));
        const double py = - x1 * vx * vy / (1 + vz) + y1 * (1 - vy * vy/ (1 + vz));
        const double pz = - x1 * vx - y1 * vy;
        pos_3d line_p(px, py, pz);

        return std::pair<pos_3d, dir_3d>(line_p, line_v);
    }


    std::array<double,4> line_p_v_to_params(const pos_3d& p, const dir_3d& v) {
        // Unit direction
        const dir_3d u = v.Unit();
        const double vx = u.x(), vy = u.y(), vz = u.z();

        const double phi = u.Phi();
        // In ROOT theta is with respect to z axis, we need the elevation from xy plane, so PI/2 - theta_root
        const double theta = M_PI_2 - u.Theta();

        // Assume vz >=0, given the upper emishpere used for the hough3d
        const double invDen = 1.0 / (1.0 + vz);

        const double A = 1.0 - vx*vx * invDen;
        const double E = -vx*vy * invDen;
        const double D = 1.0 - vy*vy * invDen;

        const dir_3d b1(A, E, -vx);
        const dir_3d b2(E, D, -vy);

        const double bx_bx = b1.Mag2();
        const double by_by = b2.Mag2();
        const double bx_by = b1.Dot(b2);

        const double bx_p  = p.Dot(b1);
        const double by_p  = p.Dot(b2);

        const double det = bx_bx*by_by - bx_by*bx_by; // should be >0 unless special degeneracy

        double x1 = 0.0, y1 = 0.0;
        if (std::abs(det) > 1e-12) {
            x1 = (bx_p*by_by - by_p*bx_by) / det;
            y1 = (by_p*bx_bx - bx_p*bx_by) / det;
        }

        return {theta, phi, x1, y1};
    }



    // Class to be used for custom ROOT Fitter
    class WeightedLineFitter {
        public:
            WeightedLineFitter(const std::vector<point_cloud::point>& points) : m_points(points) {} 
            double operator()(const double* fit_parameters);


        private:
            double point_line_distance(const point_cloud::point& point, const pos_3d& line_point, const dir_3d& line_dir);
            
            std::vector<point_cloud::point> m_points;
    };


    double WeightedLineFitter::point_line_distance(const point_cloud::point& point, const pos_3d& line_point, const dir_3d& line_dir) {
        dir_3d delta = point.position - line_point;

        dir_3d cross_prod = delta.Cross(line_dir);
        double cross_mag = cross_prod.R();
        double dir_mag = line_dir.R();

        return (cross_mag / dir_mag) * point.amplitude;
    }


    double WeightedLineFitter::operator()(const double* fit_parameters) {
        const std::array<double, 4> fit_params = {fit_parameters[0], fit_parameters[1], fit_parameters[2], fit_parameters[3]};
        const auto [line_p, line_v] = line_params_to_p_v(fit_params);
        double sum = std::accumulate(m_points.begin(), m_points.end(), 0.0, [&](double acc, const auto& p) {return acc + point_line_distance(p, line_p, line_v);});

        return sum;
    }

}   // namespace sand::grain