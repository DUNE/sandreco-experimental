#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#include <ufw/streamer.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/config.hpp>

#include <grain/image.h>

namespace sand::png {

  class png_streamer : public ufw::streamer {

  public:
    png_streamer();

    ~png_streamer();

    void configure(const ufw::config&, const ufw::type_id&, ufw::op_type) override;

    void attach(ufw::data::data_base&) override;

    void read(ufw::context_id) override;

    void write(ufw::context_id) override;

    private:
    sand::grain::images* m_images;

  };

}

UFW_REGISTER_STREAMER(sand::png::png_streamer)

namespace sand::png {

  png_streamer::png_streamer() {
  }

  png_streamer::~png_streamer() {
  }

  void png_streamer::configure(const ufw::config& cfg, const ufw::type_id& tp, ufw::op_type op) {
    streamer::configure(cfg, tp, op);
    if (op != ufw::op_type::wo) {
      UFW_ERROR("png_streamer only supports writing.");
    }
    if (tp != ufw::type_of<sand::grain::images>()) {
      UFW_ERROR("png_streamer only supports GRAIN raw camera images.");
    }
  }

  void png_streamer::attach(ufw::data::data_base& d) {
    m_images = static_cast<sand::grain::images*>(&d);
    streamer::attach(d);
  }

  void png_streamer::read(ufw::context_id id) {
    UFW_FATAL("png_streamer only supports writing.");
  }

  void png_streamer::write(ufw::context_id id) {
    auto folder = path().parent_path().string();
    auto basename = path().stem().string();
    auto ext = path().extension().string();
    for (const auto& img : m_images->images) {
      auto filename = folder + '/' + basename + '_' + img.camera_name + '_' + std::to_string(id) + ext;
      FILE* fp = fopen(filename.c_str(), "wb");
      UFW_DEBUG("Opening file for {} at {}, named {}", img.camera_name, id, filename);
      png_structp pngstruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      png_infop info = png_create_info_struct(pngstruct);
      if (setjmp(png_jmpbuf(pngstruct))) {
        png_destroy_write_struct(&pngstruct, &info);
        UFW_ERROR("Error during png creation.");
      }
      png_init_io(pngstruct, fp);
      png_set_IHDR(pngstruct, info, sand::grain::pixel_array<double>::kCols, sand::grain::pixel_array<double>::kRows,
                   8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
      png_set_swap(pngstruct);
      png_write_info(pngstruct, info);
      auto array = img.amplitude_array<uint8_t>();
      UFW_DEBUG("Writing image data for {} at {}, pixel average = {}", img.camera_name, id, [&array](){ return std::accumulate(array.begin(), array.end(), 0.0); }());
      for (int i = 0; i != sand::grain::pixel_array<double>::kRows; ++i) {
        auto vec = array.Row(i);
        png_bytep row_ptr = reinterpret_cast<png_bytep>(vec.Array());
        png_write_row(pngstruct, row_ptr);
      }
      png_write_end(pngstruct, NULL);
      png_destroy_write_struct(&pngstruct, &info);
      fclose(fp);
    }
  }

}

UFW_REGISTER_DYNAMIC_STREAMER_FACTORY(sand::png::png_streamer)
