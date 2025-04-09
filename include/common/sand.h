#pragma once

#include <Math/Vector3D.h>
#include <Math/Vector4D.h>

namespace sand {

  using pos_3d = ROOT::Math::XYZVector;
  using pos_4d = ROOT::Math::XYZTVector;

  using dir_3d = ROOT::Math::DisplacementVector3D<ROOT::Math::XYZVector, ROOT::Math::DefaultCoordinateSystemTag>;
  using dir_4d = ROOT::Math::DisplacementVector3D<ROOT::Math::XYZTVector, ROOT::Math::DefaultCoordinateSystemTag>;

  using mom_3d = ROOT::Math::XYZVector;
  using mom_4d = ROOT::Math::PxPyPzEVector;

}
