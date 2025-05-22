#include <ufw/config.hpp>
#include <ufw/factory.hpp>

#include <geant_gdml_parser.hpp>

namespace sand::grain {

  geant_gdml_parser::geant_gdml_parser(const ufw::config& cfg) {
    //this little manouver is needed because of improper handling of relative paths elsewhere...
    auto path = cfg.path_at("path");
    auto starting_path = std::filesystem::current_path();
    UFW_DEBUG("Starting path {}", std::filesystem::current_path().string());
    UFW_DEBUG("Setting path {}", path.parent_path().string());
    UFW_DEBUG("Reading geometry file: {}", path.filename().string());
    std::filesystem::current_path(path.parent_path());
    SetOverlapCheck(true);
    SetStripFlag(false);
    Read(path.filename().c_str(), false);
    std::filesystem::current_path(starting_path);
    UFW_DEBUG("Back to {}", std::filesystem::current_path().string());
  }

}
