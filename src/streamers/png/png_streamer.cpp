#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/streamer.hpp>

#include <grain/image.h>

namespace sand::png {

  class png_streamer : public ufw::streamer {
   public:
    png_streamer();

    ~png_streamer();

    void configure(const ufw::config&, ufw::op_type) override;

    void prepare(const ufw::public_id&, const ufw::type_id&) override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;

   private:
    int m_scale_factor;
    std::vector<uint8_t> m_scaled_row;
  };

} // namespace sand::png

UFW_REGISTER_STREAMER(sand::png::png_streamer)

namespace sand::png {

  png_streamer::png_streamer() : m_scale_factor(1) {}

  png_streamer::~png_streamer() {}

  void png_streamer::configure(const ufw::config& cfg, ufw::op_type op) {
    streamer::configure(cfg, op);
    if (op != ufw::op_type::wo) {
      UFW_ERROR("png_streamer only supports writing.");
    }
    m_scale_factor = cfg.value("scale", m_scale_factor);
    if (m_scale_factor < 1) {
      UFW_ERROR("Invalid scale factor {}: must be integer >= 1", m_scale_factor);
    }
    m_scaled_row.resize(sand::grain::pixel_array<double>::kCols * m_scale_factor);
  }

  void png_streamer::prepare(const ufw::public_id& id, const ufw::type_id& tp) {
    if (tp != ufw::type_of<sand::grain::images>()) {
      UFW_ERROR("png_streamer only supports GRAIN raw camera images.");
    }
    ufw::streamer::prepare(id, tp);
  }

  void png_streamer::read(ufw::context_id) { UFW_FATAL("png_streamer only supports writing."); }

  void png_streamer::write(ufw::context_id ctx) {
    auto folder   = path().parent_path().string();
    auto basename = path().stem().string();
    auto ext      = path().extension().string();
    for (const auto& [id, info] : info_map()) {
      for (const auto& img : static_cast<sand::grain::images*>(info.address)->images) {
        auto filename = folder + '/' + basename + id + '_' + std::to_string(ctx) + '_' + std::to_string(img.camera_id)
                      + "_T" + std::to_string(long(img.time_begin)) + ext;
        FILE* fp = fopen(filename.c_str(), "wb");
        UFW_DEBUG("Opening file for camera {} at {}, named {}", int(img.camera_id), ctx, filename);
        png_structp pngstruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop info        = png_create_info_struct(pngstruct);
        if (setjmp(png_jmpbuf(pngstruct))) {
          png_destroy_write_struct(&pngstruct, &info);
          UFW_ERROR("Error during png creation.");
        }
        png_init_io(pngstruct, fp);
        png_set_IHDR(pngstruct, info, sand::grain::pixel_array<double>::kCols * m_scale_factor,
                     sand::grain::pixel_array<double>::kRows * m_scale_factor, 8, PNG_COLOR_TYPE_GRAY,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_set_swap(pngstruct);
        png_write_info(pngstruct, info);
        auto array = img.amplitude_array<uint8_t>();
        UFW_DEBUG("Writing image data for camera {} at {}, pixel average = {}", int(img.camera_id), ctx,
                  [&array]() { return std::accumulate(array.begin(), array.end(), 0.0); }());
        for (int row = 0; row != sand::grain::pixel_array<double>::kRows; ++row) {
          for (int col = 0; col != sand::grain::pixel_array<double>::kCols; ++col) {
            for (int fillc = 0; fillc != m_scale_factor; ++fillc) {
              // consistent indexing: Row Major
              m_scaled_row[col * m_scale_factor + fillc] = array(row, col);
            }
          }
          for (int fillr = 0; fillr != m_scale_factor; ++fillr) {
            png_write_row(pngstruct, m_scaled_row.data());
          }
        }
        png_write_end(pngstruct, NULL);
        png_destroy_write_struct(&pngstruct, &info);
        fclose(fp);
      }
    }
  }

} // namespace sand::png

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::png::png_streamer)
