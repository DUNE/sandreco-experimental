#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>


namespace sand::grain {

class detector_response : public ufw::process {

    public:
    detector_response();
    void configure (const ufw::config& cfg) override;
    // const ufw::var_type_map& products() const override;
    // const ufw::var_type_map& requirements() const override;
    void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;
  
    private:
    double m_sipm_cell = 0.0;
};
    
}

UFW_REGISTER_PROCESS(sand::grain::detector_response)
