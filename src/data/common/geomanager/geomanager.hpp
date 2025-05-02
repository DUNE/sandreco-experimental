#include <ufw/data.hpp>

namespace sand {

  class geomanager : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {

  public:
    explicit geomanager(const ufw::config&);

  };

}

UFW_DECLARE_COMPLEX_DATA(sand::geomanager);
