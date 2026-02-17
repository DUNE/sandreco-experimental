#pragma once

#include <geoinfo/tracker_info.hpp>
#include <optional>

namespace sand {

  class geoinfo::stt_info : public tracker_info {
   public:
    struct wire : public tracker_info::wire {
      geo_id geo; ///< The unique geometry identifier
    };

    struct station : public tracker_info::station {
      wire_list x_view() const;
      wire_list y_view() const;
      void set_wire_list(std::vector<std::unique_ptr<wire>> &);
    };

   public:
    stt_info(const geoinfo&);

    virtual ~stt_info();

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

    const wire* get_wire_by_id(const geo_id& id) const;

   private:
    void set_wire_adjecency(std::vector<std::unique_ptr<wire>> & w );
  };

} // namespace sand
