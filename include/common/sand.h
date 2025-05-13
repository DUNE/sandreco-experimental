#pragma once

#include <cstdint>
#include <string>

#include <Math/Vector3D.h>
#include <Math/Vector4D.h>
#include <Math/SVector.h>
#include <Math/SMatrix.h>

namespace sand {

  using pos_3d = ROOT::Math::XYZVector;
  using pos_4d = ROOT::Math::XYZTVector;

  using dir_3d = ROOT::Math::DisplacementVector3D<ROOT::Math::XYZVector, ROOT::Math::DefaultCoordinateSystemTag>;
  using dir_4d = ROOT::Math::DisplacementVector3D<ROOT::Math::XYZTVector, ROOT::Math::DefaultCoordinateSystemTag>;

  using mom_3d = ROOT::Math::XYZVector;
  using mom_4d = ROOT::Math::PxPyPzEVector;

  class geo_path : public std::string {

  public:
    using std::string::string;
    using std::string::operator=;

    inline geo_path& operator /= (const std::string_view&);

    geo_path operator / (const std::string_view& rhs) const {
      geo_path p(*this);
      return p /= rhs;
    }

    inline std::string_view token(std::size_t) const;

  };

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

  struct geo_id {
    enum subdetector_t : uint8_t {
      DRIFT = 0,
      ECAL = 1,
      GRAIN = 2,
      STT = 3,
      MUON = 4,
      NONE = 255
    };
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
          ENDCAP_HOR_BOT = 5
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
  };

  struct channel_id {
    union {
      struct {
        uint8_t reserved___0;
        geo_id::subdetector_t subdetector;
        uint8_t link;
        uint8_t padding___1;
        uint32_t channel;
      } /*any*/;
      uint64_t raw = -1;
    };
  };

}
