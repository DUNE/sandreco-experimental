
#include <factory.hpp>
#include <digitize.hpp>

namespace sand {

  namespace grain {

      void digitize::configure (const ufw::config& cfg) {
      }

      ufw::data_list digitize::products() const {
        return {};
      }

      ufw::data_list digitize::requirements() const {
        return {};
      }

      void digitize::run(const ufw::data_set&, ufw::data_set&) {
      }

  }

}

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::digitize)
