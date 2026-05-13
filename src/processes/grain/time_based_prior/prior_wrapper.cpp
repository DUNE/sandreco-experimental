#include <common/utils/pytypes.h>
#include <grain/image.h>
#include <grain/voxels.h>

#include <geoinfo/grain_info.hpp>

#include <ufw/factory.hpp>
#include <ufw/pyprocess.hpp>

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
  struct __attribute__((visibility("hidden"))) prior_wrapper::prior_private {
    py::array::ShapeContainer shape;
    py::array_t<uint8_t> fiducial_mask;
    py::dict transforms;
  };

  prior_wrapper::prior_wrapper() : pyprocess({{"images", "sand::grain::images"}}, {{"prior", "sand::grain::voxels"}}) {
    UFW_INFO("Creating a prior_wrapper process at {}", fmt::ptr(this));
  }

  void prior_wrapper::configure(const ufw::config& cfg) {
    pyprocess::configure(cfg); // this is initializing the python interpreter and needs to be first.
    const auto& gi = instance<geoinfo>();
    m_voxel_size   = cfg.at("voxel_size");
    dir_3d voxel_sizes(m_voxel_size, m_voxel_size, m_voxel_size);
    auto mask = gi.grain().fiducial_voxels(voxel_sizes);
    // TODO these args could be passed via cfg, but it is inefficient to pass them via json.
    // one can fully reimplement pyprocess::configure to pass different arguments to configure as python types.
    m_private.reset(new prior_wrapper::prior_private{pytypes::size2shape(mask.size()), {}, {}});
    m_private->fiducial_mask.resize(m_private->shape);
    for (const auto& camera : gi.grain().mask_cameras()) {
      UFW_INFO("Transform for camera {}", camera.name);
      m_private->transforms[camera.name.c_str()] = pytypes::xform2ndarray(camera.transform);
    }
  }

  void prior_wrapper::run() {
    const auto& gi     = instance<geoinfo>();
    const auto& images = get<sand::grain::images>("images");
    auto& prior        = set<sand::grain::voxels>("prior");
    for (auto& this_spill : images.images) {
      py::dict arg_images;
      for (const auto& img : this_spill) {
        const auto& camera              = gi.grain().at(img.camera_id);
        arg_images[camera.name.c_str()] = pytypes::pixels2ndarray(img.pixels);
      }
      py::object run_func = py_module().attr("run");
      py::object ret      = run_func(m_private->fiducial_mask, m_private->transforms, arg_images);
      prior.voxels.push_back(pytypes::ndarray2voxels(ret));
    }
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::prior_wrapper)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::prior_wrapper)
