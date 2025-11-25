#pragma once

#include <geoinfo/geoinfo.hpp>

namespace sand {

  class geoinfo::subdetector_info {
   public:
    subdetector_info(const geoinfo&, const geo_path&);
    subdetector_info(const subdetector_info&)             = delete;
    subdetector_info& operator= (const subdetector_info&) = delete;

    virtual ~subdetector_info();

    virtual geo_id id(const geo_path&) const = 0;

    virtual geo_path path(geo_id) const = 0;

    geo_path path() const { return m_path; }

    geo_path partial_path(const geo_path& full_path, const geoinfo& gi) const {
      auto partial_path = full_path;
      if (full_path.find(gi.root_path()) != std::string::npos) {
        partial_path = partial_path - gi.root_path();
      }
      if (partial_path.find(path()) != std::string::npos) {
        partial_path = partial_path - path();
      }
      return partial_path;
    }

    /**
     * @returns A transformation matrix that takes a vector from local to global coordinates.
     */
    const xform_3d& transform() const { return m_transform; }

   protected:
    const geoinfo& info() { return r_info; }

   private:
    const geoinfo& r_info;
    geo_path m_path;
    xform_3d m_transform;
  };

} // namespace sand
