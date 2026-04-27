#include "grain/grain.h"
#include <grain/image.h>
#include <grain/voxels.h>

#include <geoinfo/grain_info.hpp>

#include <ufw/factory.hpp>
#include <ufw/pyprocess.hpp>

#include <pybind11/numpy.h>

namespace sand::grain {

  /**
   * \class sand::grain::prior_wrapper
   *
   * \brief C++ wrapper for a python module for prior calculation
   *
   * This process takes images from a spill and attempts to calculate a prior distribution for photon amplitudes
   *
   * \subsection Configuration
   * | Parameter Name    | Type          | Unit   | Required/Default             | Description                        |
   * |-------------------|---------------|--------|------------------------------|------------------------------------|
   * | `voxel_size`      | double        | mm     | Required                     | Voxel pitch (assumed cube)         |
   * 
   * \subsection Python API
   * There is a configure function which returns nothing taking a json formatted string as argument.
   * There is a run function which returns one ndarray of voxels
   */

  class prior_wrapper : public ufw::pyprocess {
   private:
    struct prior_private;

   public:
    prior_wrapper();
    void configure(const ufw::config&) override;
    void run() override;

   private:
    float m_voxel_size;
    std::unique_ptr<prior_private> m_private;

  };

  // pybind11 wants hidden visibility for its types, but I prefer the wrapper itself to have
  // normal visibility for plugin loading so we hide the hidden visibility in this separate struct
  struct __attribute__ ((visibility("hidden"))) prior_wrapper::prior_private {
    py::array::ShapeContainer shape;
    py::array_t<uint8_t> fiducial_mask;
    py::dict transforms;
  };

  prior_wrapper::prior_wrapper() : pyprocess({{"images", "sand::grain::images"}},
                                             {{"prior", "sand::grain::voxels"}}) {
    UFW_INFO("Creating a prior_wrapper process at {}", fmt::ptr(this));
  }

  namespace { //TODO this will probably be useful as a separate header with helper functions

    py::array::ShapeContainer size2shape(size_3d sz) {
      return py::array::ShapeContainer{sz.x(), sz.y(), sz.z()};
    }

    py::array_t<double, py::array::c_style> xform2ndarray(xform_3d xf) {
      py::array_t<double, py::array::c_style> xform = py::array_t<double>({4, 3});
      auto xform_buf = xform.request();
      double* ptr = static_cast<double*>(xform_buf.ptr);
      //TODO apply the z transform of SiPMs
      xf.GetComponents(ptr);
      return xform;
    }

    py::array_t<double, py::array::c_style> pixels2ndarray(const pixel_array<images::pixel>& pixels) {
      py::array_t<double, py::array::c_style> image(py::array::ShapeContainer({camera_height, camera_width, 2u}));
      auto buf = image.request();
      images::pixel* ptr = static_cast<images::pixel*>(buf.ptr);
      std::copy(pixels.begin(), pixels.end(), ptr);
      return image;
    }

    voxel_array<float> ndarray2voxels(const py::array_t<float>& array) {
      size_3d sz(array.shape(0), array.shape(1), array.shape(2));
      //TODO this is an expensive copy
      return voxel_array<float>(sz, static_cast<const float*>(array.data()));
    }

  }

  void prior_wrapper::configure(const ufw::config& cfg) {
    const auto& gi = instance<geoinfo>();
    m_voxel_size = cfg.at("voxel_size");
    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto mask = gi.grain().fiducial_voxels(voxel_sizes);
    //TODO these args could be passed via cfg, but it is inefficient to pass them via json.
    //one can fully reimplement pyprocess::configure to pass different arguments to configure as python types.
    m_private->shape = size2shape(mask.size());
    m_private->fiducial_mask.resize(m_private->shape);
    for (const auto& camera : gi.grain().mask_cameras()) {
      m_private->transforms[camera.name.c_str()] = xform2ndarray(camera.transform);
    }
    pyprocess::configure(cfg);
  }

  void prior_wrapper::run() {
    const auto& gi = instance<geoinfo>();
    const auto& images = get<sand::grain::images>("images");
    auto& prior = set<sand::grain::voxels>("prior");
    // As of today, images is not spill oriented, while voxels are.
    // I will pretend all images are in the first spill, TODO fix this when images becomes a vector of vectors.
    for (int spill = 0; spill != 1; ++spill) {
      py::dict arg_images;
      for (const auto& img : images.images) {
        const auto& camera = gi.grain().at(img.camera_id);
        arg_images[camera.name.c_str()] = pixels2ndarray(img.pixels);
      }
      py::object run_func = py_module().attr("run");
      py::object ret = run_func(m_private->fiducial_mask, m_private->transforms, arg_images);
      prior.voxels.push_back(ndarray2voxels(ret));
    }
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::prior_wrapper)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::prior_wrapper)
