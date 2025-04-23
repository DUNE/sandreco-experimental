#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>
#include <grain/digi.h>
#include <random>

namespace sand::grain {

class detector_response_fast : public ufw::process {

    public:
    detector_response_fast();
    void configure (const ufw::config& cfg) override;
    // const ufw::var_type_map& products() const override;
    // const ufw::var_type_map& requirements() const override;
    void run() override;

    private:
        digi::camera assign_to_pixel(const hits::camera& c);

    private:
        double m_sipm_cell;
        int m_matrix_rows;
        int m_matrix_columns;
        double m_matrix_width;
        double m_matrix_height;
        double m_pde;
        std::default_random_engine m_rng_engine;
        std::uniform_real_distribution<> m_uniform;

};
    
}

UFW_REGISTER_PROCESS(sand::grain::detector_response_fast)
