#include <common/data.h>

namespace sand::grain {

    struct volumereco_cl_manager
    : public ufw::data::base<ufw::data::complex_tag, ufw::data::instanced_tag, ufw::data::global_tag> {
    public:
        explicit volumereco_cl_manager(const ufw::config&);
    };

} // namespace sand::grain

UFW_DECLARE_COMPLEX_DATA(sand::grain::volumereco_cl_manager);
