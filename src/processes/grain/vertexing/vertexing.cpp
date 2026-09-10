#include <common/sand.h>
#include <grain/point_clusters.h>
#include <grain/vertex.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <vector>

namespace sand::grain {

  /**
   * \class sand::grain::vertexing
   *
   * \brief Estimates the position of the interaction vertex
   *
   * Estimates the position of the interaction vertex given at least two tracks by computing the midpoint between the lines
   *
   * \subsection Configuration
   * | Parameter Name            | Type   | Unit  | Required/Default | Description                                                           |
   * |---------------------------|--------|-------|------------------|-----------------------------------------------------------------------|
   *
   * \subsection Dependencies
   * | Type            | Comment  |
   * |-----------------|----------|
   * | `sand::geoinfo` | Geometry |
   *
   * \subsection Requirements
   * |  Name               | Type                          | Comment                           |
   * |---------------------|-------------------------------|-----------------------------------|
   * | `point_clusters`    | `sand::grain::point_clusters` | Clusters with track               |
   *
   * \subsection Products
   * |  Name                | Type                           | Comment                           |
   * |----------------------|--------------------------------|-----------------------------------|
   * | `vertices`           | `sand::grain::vertex`          | Vertex position                   |
   */

  class vertexing : public ufw::process {
   public:
    vertexing();
    void configure(const ufw::config& cfg) override;
    void run() override;
    pos_3d median_point_between_tracks(const pos_3d& p1, const dir_3d& d1, const pos_3d& p2, const dir_3d& d2);

   private:
  };

  pos_3d vertexing::median_point_between_tracks(const pos_3d& p1, const dir_3d& d1, const pos_3d& p2, const dir_3d& d2) {
    // Vector from line 2's point to line 1's point
    dir_3d w0 = p1 - p2;
    
    // Compute dot products
    double a = d1.Dot(d1);        // |d1|^2
    double b = d1.Dot(d2);        // d1 · d2
    double c = d2.Dot(d2);        // |d2|^2
    double d = d1.Dot(w0);        // d1 · w0
    double e = d2.Dot(w0);        // d2 · w0
    
    // Denominator for solving the linear system
    double denom = a * c - b * b;
    
    // Check if lines are parallel (denom close to 0)
    // If lines are parallel use line 1's closest point to line 2's position
    const double epsilon{1e-10};
    double t = (std::abs(denom) < epsilon) ? d / a : (b * e - c * d) / denom;
    double s = (std::abs(denom) < epsilon) ? 0 : (a * e - b * d) / denom;
    
    // Find closest point on each line
    pos_3d closest1 = p1 + t * d1;
    pos_3d closest2 = p2 + s * d2;
    
    // Return the median (midpoint)
    dir_3d midpoint_vec = ((closest1 - p2) + (closest2 - p2)) * 0.5;
   
    return p2 + midpoint_vec;
  }

  void vertexing::configure(const ufw::config& cfg) {
    process::configure(cfg);
  }

  vertexing::vertexing() : process({{"point_clusters", "sand::grain::point_clusters"}},
                               {{"vertices", "sand::grain::vertex"}}) {
    UFW_DEBUG("Creating a vertexing process at {}.", fmt::ptr(this));
  }

  void vertexing::run() {
    UFW_DEBUG("Running a vertexing process at {}.", fmt::ptr(this));
    const auto& point_clusters_in = get<point_clusters>("point_clusters").clusters;
    auto& vertices_out = set<vertex>("vertices").vertices;
    // Loop on events in a spill
    for (const auto& ev_clusters_in : point_clusters_in) {
      if (ev_clusters_in.size() < 2) {
        UFW_INFO("Skipping event less than 2 tracks");
        continue;
      }
      UFW_DEBUG("Processing {} tracks", ev_clusters_in.size());
      // For the moment, consider only the first 2 tracks, since they should be the most accurate
      pos_3d vertex = median_point_between_tracks(ev_clusters_in[0].centre(), ev_clusters_in[0].axis(), ev_clusters_in[1].centre(), ev_clusters_in[1].axis());
      UFW_DEBUG("Vertex: {}", vertex);
      vertices_out.push_back(vertex);
    }

  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::vertexing)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::vertexing)
