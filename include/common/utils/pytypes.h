
#pragma once

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <common/array.h>
#include <common/data.h>
#include <common/digi.h>
#include <common/hit.h>
#include <common/sand.h>

#include <grain/grain.h>
#include <grain/image.h>
#include <grain/voxels.h>

namespace sand::pytypes {

  namespace py = pybind11;

  inline py::array::ShapeContainer size2shape(grain::size_3d sz) {
    return py::array::ShapeContainer({sz.x(), sz.y(), sz.z()});
  }

  inline py::array_t<double, py::array::c_style> xform2ndarray(xform_3d xf) {
    py::array_t<double, py::array::c_style> xform = py::array_t<double>({3, 4});
    auto xform_buf                                = xform.request();
    double* ptr                                   = static_cast<double*>(xform_buf.ptr);
    xf.GetComponents(ptr);
    // TODO this is likely not the correct orientation for this transform, rows and columns may be confused.
    // TODO apply the z transform of SiPMs
    return xform;
  }

  inline py::array_t<double, py::array::c_style>
  pixels2ndarray(const grain::pixel_array<grain::images::pixel>& pixels) {
    py::array_t<double, py::array::c_style> image(
        py::array::ShapeContainer({grain::camera_height, grain::camera_width, 2u}));
    auto buf    = image.request();
    double* ptr = static_cast<double*>(buf.ptr);
    // we only copy amplitude and time, not the truth
    for (auto it = pixels.begin(); it != pixels.end(); ++it) {
      *ptr++ = it->amplitude;
      *ptr++ = it->time_first;
    }
    return image;
  }

  inline grain::voxel_array<float> ndarray2voxels(const py::array_t<float>& array) {
    grain::size_3d sz(array.shape(0), array.shape(1), array.shape(2));
    // TODO this is an expensive copy
    return grain::voxel_array<float>(sz, static_cast<const float*>(array.data()));
  }

} // namespace sand::pytypes
