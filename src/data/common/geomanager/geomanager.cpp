#include <ufw/data.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <common/sand.h>

#include <geomanager.hpp>
#include <ecal_manager.hpp>
#include <grain_manager.hpp>
#include <tracker_manager.hpp>
#include <drift_manager.hpp>
#include <stt_manager.hpp>

namespace sand {

  geomanager::geomanager(const ufw::config&) {}

  geomanager::~geomanager() = default;

}
