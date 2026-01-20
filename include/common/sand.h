#pragma once

#include <cstdint>
#include <string>

#include <Math/Rotation3D.h>
#include <Math/SMatrix.h>
#include <Math/SVector.h>
#include <Math/Transform3D.h>
#include <Math/Vector3D.h>
#include <Math/Vector4D.h>

#include <ufw/utils.hpp>

namespace sand {

  /**
   * Position and direction vectors. For 3D space, note the difference between positions and directions.
   */
  using pos_3d = ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<double>>;
  using dir_3d = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<double>>;
  using vec_4d = ROOT::Math::PxPyPzEVector;

  using mom_3d = dir_3d;
  using mom_4d = vec_4d;

  using rot_3d = ROOT::Math::Rotation3D;
  /**
   * xform_3d is the best tool for coordinate system transformation. It is used by multiplying a vector
   * in the original coordinate system to obtain one in the new system: x' = T * x.
   * The inverse transform can be used for the opposite transformation x = T.Inverse() * x'
   * It correctly handles positions and directions, by applying both rotation and translation to the former,
   * and only rotation to ther latter.
   */
  using xform_3d = ROOT::Math::Transform3D;

} // namespace sand

template <>
struct fmt::formatter<sand::pos_3d> : formatter<string_view> {
  auto format(const sand::pos_3d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({:.3f}, {:.3f}, {:.3f})", c.x(), c.y(), c.z());
  }
};

template <>
struct fmt::formatter<sand::dir_3d> : formatter<string_view> {
  auto format(const sand::dir_3d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({:.3f}, {:.3f}, {:.3f})", c.x(), c.y(), c.z());
  }
};

template <>
struct fmt::formatter<sand::vec_4d> : formatter<string_view> {
  auto format(const sand::vec_4d& c, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "({:.3f}, {:.3f}, {:.3f}, {:.3f})", c.x(), c.y(), c.z(), c.t());
  }
};

template <>
struct fmt::formatter<sand::rot_3d> : formatter<string_view> {
  auto format(const sand::rot_3d& c, format_context& ctx) const -> format_context::iterator {
    sand::rot_3d::Scalar r[9];
    c.GetComponents(r);
    return fmt::format_to(ctx.out(), "R(({:.3f}, {:.3f}, {:.3f}), ({:.3f}, {:.3f}, {:.3f}), ({:.3f}, {:.3f}, {:.3f}))",
                          r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
  }
};

template <>
struct fmt::formatter<sand::xform_3d> : formatter<string_view> {
  auto format(const sand::xform_3d& c, format_context& ctx) const -> format_context::iterator {
    sand::xform_3d::Scalar r[12];
    c.GetComponents(r);
    return fmt::format_to(
        ctx.out(),
        "R(({:.3f}, {:.3f}, {:.3f}), ({:.3f}, {:.3f}, {:.3f}), ({:.3f}, {:.3f}, {:.3f})), T({:.3f}, {:.3f}, {:.3f}))",
        r[0], r[1], r[2], r[4], r[5], r[6], r[8], r[9], r[10], r[3], r[7], r[11]);
  }
};

namespace sand {
  /**
   * Class to represet a geometry path, supports safe concatenation via @p operator /.
   */
  class geo_path : public std::string {
   public:
    using std::string::string;
    using std::string::operator=;

    inline geo_path& operator/= (const std::string_view&);

    geo_path operator/ (const std::string_view& rhs) const {
      geo_path p(*this);
      return p /= rhs;
    }

    inline geo_path& operator-= (const geo_path&);

    geo_path operator- (const geo_path& rhs) const {
      geo_path p(*this);
      return p -= rhs;
    }

    inline std::string_view token(std::size_t) const;
  };

  /**
   * Appends @p sv to this with the correct amount of '/' characters.
   */
  inline geo_path& geo_path::operator/= (const std::string_view& sv) {
    if (empty()) {
      assign(sv);
      return *this;
    }
    if (sv.empty()) {
      return *this;
    }
    if (back() == '/') {
      if (sv.front() == '/') {
        append(sv.substr(1));
      } else {
        append(sv);
      }
    } else {
      if (sv.front() == '/') {
        append(sv);
      } else {
        reserve(size() + 1 + sv.size());
        push_back('/');
        append(sv);
      }
    }
    return *this;
  }

  /**
   * Removes a path @e prefix from this path if and only if @p sub matches the start of this path.
   *
   * @returns the remaining path element.
   */
  inline geo_path& geo_path::operator-= (const geo_path& sub) {
    if (std::string_view(sub.data(), sub.size()) == sub) {
      erase(0, sub.size());
    }
    return *this;
  }

  /**
   * @returns the @p i-th element of the path.
   */
  inline std::string_view geo_path::token(std::size_t i) const {
    std::size_t start = 0;
    std::size_t stop  = 0;
    while (i--) {
      start = find('/', start + 1);
    }
    stop = find('/', start + 1);
    if (stop == std::string::npos) {
      stop = size();
    }
    return std::string_view(data() + start + 1, stop - start - 1);
  }

  /**
   * Subdetector type enumeration
   */
  enum subdetector_t : uint8_t { DRIFT = 0, ECAL = 1, GRAIN = 2, STT = 3, TRACKER = STT, MUON = 4, NONE = 255 };

  /**
   * Unique identifier for elements of the detector geometry as known by Geant.
   * There is a 1-1 correspondence between the geo_path of a sensitive detector and a geo_id.
   * Prefer geo_id as a key, as it is substantially faster to compare.
   */
  struct geo_id {
    using supermodule_t = uint8_t;
    using plane_t       = uint8_t;
    using tube_t        = uint8_t;
    // ROOT reports an internal error when generating a dictionary for this anonymous union.
    // Since we don't care too much, we can just pretend it is just an int.
#ifndef __CLING__
    union {
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t padding___[6];
      } /*any*/;
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        supermodule_t supermodule;
        plane_t plane;
        uint8_t padding___1[4];
      } drift;
      struct {
        enum region_t : uint8_t {
          BARREL           = 0,
          ENDCAP_VERTICAL  = 1,
          ENDCAP_CURVE_TOP = 2,
          ENDCAP_CURVE_BOT = 3,
          ENDCAP_HOR_TOP   = 4,
          ENDCAP_HOR_BOT   = 5,
          NONE             = 255
        };
        uint8_t reserved___0;
        subdetector_t subdetector;
        supermodule_t supermodule;
        region_t region;
        plane_t plane;
        uint8_t padding___1[3];
      } ecal;
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t padding___1[6];
      } grain;
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        supermodule_t supermodule;
        plane_t plane;
        tube_t tube;
        uint8_t padding___1[2];
      } stt;
      uint64_t raw = -1;
    };
#else  //__CLING__
    uint64_t raw = -1;
#endif //__CLING__
  };

  // Equality operator
  inline bool operator== (geo_id lhs, geo_id rhs) { return lhs.raw == rhs.raw; }

  // Less-than operator for ordering
  inline bool operator< (geo_id lhs, geo_id rhs) { return lhs.raw < rhs.raw; }

  /**
   * Unique identifier for channels as known by the data acquisition system. There is a many-to-many correspondence
   * between these and geo_ids.
   */
  struct channel_id {
    using link_t    = uint8_t;
    using channel_t = uint32_t;
    // ROOT reports an internal error when generating a dictionary for this anonymous union.
    // Since we don't care too much, we can just pretend it is just an int.
#ifndef __CLING__
    union {
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        link_t link;
        uint8_t padding___1;
        channel_t channel;
      } /*any*/;
      uint64_t raw = -1;
    };
#else  //__CLING__
    uint64_t raw = -1;
#endif //__CLING__
  };

  // Equality operator
  inline bool operator== (channel_id lhs, channel_id rhs) { return lhs.raw == rhs.raw; }

  // Less-than operator for ordering
  inline bool operator< (channel_id lhs, channel_id rhs) { return lhs.raw < rhs.raw; }

} // namespace sand

#ifndef __CLING__

template <>
struct fmt::formatter<sand::subdetector_t> : formatter<string_view> {
  auto format(sand::subdetector_t s, format_context& ctx) const -> format_context::iterator {
    switch (s) {
    case sand::DRIFT:
      return fmt::format_to(ctx.out(), "DRIFT");
    case sand::ECAL:
      return fmt::format_to(ctx.out(), "ECAL");
    case sand::GRAIN:
      return fmt::format_to(ctx.out(), "GRAIN");
    case sand::STT:
      return fmt::format_to(ctx.out(), "STT");
    case sand::MUON:
      return fmt::format_to(ctx.out(), "MUON");
    default:
      return fmt::format_to(ctx.out(), "NONE");
    }
  }
};

template <>
struct fmt::formatter<sand::geo_id> : formatter<string_view> {
  auto format(const sand::geo_id& gid, format_context& ctx) const -> format_context::iterator {
    switch (gid.subdetector) {
    case sand::DRIFT:
      return fmt::format_to(ctx.out(), "[{}: module {}, plane {}]", gid.subdetector, gid.drift.supermodule,
                            gid.drift.plane);
    case sand::ECAL:
      return fmt::format_to(ctx.out(), "[{}: module {}, region {}, plane {}]", gid.subdetector, gid.ecal.supermodule,
                            gid.ecal.region, gid.ecal.plane);
    case sand::GRAIN:
      return fmt::format_to(ctx.out(), "[{}: inner vessel]", gid.subdetector); // grain has no substructure to report
    case sand::STT:
      return fmt::format_to(ctx.out(), "[{}: module {}, plane {}, tube {}]", gid.subdetector, gid.stt.supermodule,
                            gid.stt.plane, gid.stt.tube);
    default:
      return fmt::format_to(ctx.out(), "[{}: not a valid subdetector]", gid.subdetector);
    }
  }
};

template <>
struct fmt::formatter<sand::channel_id> : formatter<string_view> {
  auto format(const sand::channel_id& chid, format_context& ctx) const -> format_context::iterator {
    return fmt::format_to(ctx.out(), "[{}: link {}, channel {}]", chid.subdetector, chid.link, chid.channel);
  }
};

#endif //__CLING__
