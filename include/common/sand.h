#pragma once

#include <cstdint>
#include <string>

#include <Math/Rotation3D.h>
#include <Math/Transform3D.h>
#include <Math/Vector3D.h>
#include <Math/Vector4D.h>
#include <Math/SVector.h>
#include <Math/SMatrix.h>

namespace sand {

  using pos_3d = ROOT::Math::PositionVector3D<ROOT::Math::Cartesian3D<double>>;
  using dir_3d = ROOT::Math::DisplacementVector3D<ROOT::Math::Cartesian3D<double>>;
  using vec_4d = ROOT::Math::PxPyPzEVector;

  using mom_3d = dir_3d;
  using mom_4d = vec_4d;

  using rot_3d = ROOT::Math::Rotation3D;
  using xform_3d = ROOT::Math::Transform3D;

  /**
   * Class to represet a geometry path, supports safe concatenation via @p operator /.
   */
  class geo_path : public std::string {

  public:
    using std::string::string;
    using std::string::operator=;

    inline geo_path& operator /= (const std::string_view&);

    geo_path operator / (const std::string_view& rhs) const {
      geo_path p(*this);
      return p /= rhs;
    }

    inline geo_path& operator -= (const geo_path&);

    geo_path operator - (const geo_path& rhs) const {
      geo_path p(*this);
      return p -= rhs;
    }
    
    inline std::string_view token(std::size_t) const;

  };

  /**
   * Appends @p sv to this with the correct amount of '/' characters.
   */
  inline geo_path& geo_path::operator /= (const std::string_view& sv) {
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
  inline geo_path& geo_path::operator -= (const geo_path& sub) {
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
    std::size_t stop = 0;
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
  enum subdetector_t : uint8_t {
    DRIFT = 0,
    ECAL = 1,
    GRAIN = 2,
    STT = 3,
    MUON = 4,
    NONE = 255
  };

  /**
   * Unique identifier for elements of the detector geometry as known by Geant.
   * There is a 1-1 correspondence between the geo_path of a sensitive detector and a geo_id.
   * Prefer geo_id as a key, as it is substantially faster to compare.
   */
#ifdef __CLING__
  using geo_id = uint64_t;
#else //__CLING__
  struct geo_id {
    union {
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t padding___[6];
      } /*any*/;
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t supermodule;
        uint8_t plane;
        uint8_t padding___1[4];
      } drift;
      struct {
        enum region_t : uint8_t {
          BARREL = 0,
          ENDCAP_VERTICAL = 1,
          ENDCAP_CURVE_TOP = 2,
          ENDCAP_CURVE_BOT = 3,
          ENDCAP_HOR_TOP = 4,
          ENDCAP_HOR_BOT = 5,
          NONE = 255
        };
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t supermodule;
        region_t region;
        uint8_t plane;
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
        uint8_t supermodule;
        uint8_t plane;
        uint16_t tube;
        uint8_t padding___1[2];
      } stt;
      uint64_t raw = -1;
    };

    // Equality operator
    bool operator==(const geo_id& other) const {
        return raw == other.raw;
    }
    
    // Inequality
    bool operator!=(const geo_id& other) const {
        return !(*this == other);
    }

    // Less-than operator for ordering
    bool operator<(const geo_id& other) const {
        return raw < other.raw;
    }

    // Greater-than operator for ordering
    bool operator>(const geo_id& other) const {
        return raw > other.raw;
    }
};

#endif //__CLING__
  /**
   * Unique identifier for channels as known by the data acquisition system.
   */
#ifdef __CLING__
  using channel_id = uint64_t;
#else //__CLING__
  struct channel_id {
    union {
      struct {
        uint8_t reserved___0;
        subdetector_t subdetector;
        uint8_t link;
        uint8_t padding___1;
        uint32_t channel;
      } /*any*/;
      uint64_t raw = -1;
    };
  };

#endif //__CLING__

}
