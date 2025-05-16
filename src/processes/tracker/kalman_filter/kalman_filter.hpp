#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>
#include <grain/digi.h>
#include <random>

namespace sand::grain {

class kalman_filter : public ufw::process {

    public:
    kalman_filter();
    void configure (const ufw::config& cfg) override;
    // const ufw::var_type_map& products() const override;
    // const ufw::var_type_map& requirements() const override;
    void run() override;

    private:
        double m_trackletinfo_placeholder;

};
    
}

UFW_REGISTER_PROCESS(sand::grain::kalman_filter)
