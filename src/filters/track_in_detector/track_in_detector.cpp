#include <edep_reader/edep_reader.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/filter.hpp>

namespace sand::filter {

  /**
   * \class sand::filter::track_in_detector
   *
   * This filter accepts events that have a vertex in the required detector(s) and discards those which do not.
   */
  class track_in_detector : public ufw::filter {
   public:
    track_in_detector() : ufw::filter({}) {}

    void configure(const ufw::config& cfg) override {
      for (auto item : cfg.at("require")) {
        auto it = sand::string_to_component.find(item);
        if (it == sand::string_to_component.end()) {
          UFW_ERROR("Detector not found {}.", item);
        }
        m_desired.push_back(it->second);
      }
    }

    ufw::filter::response evaluate() override {
      const auto& tree      = get<sand::edep_reader>();
      const auto& primaries = tree.GetChildrenTrajectories();
      if (primaries.empty()) {
        UFW_WARN("There are no primaries here");
        return block;
      }
      for (const auto& prim : primaries) {
        for (auto det : m_desired) {
          if (prim.HasHitInDetector(det)) {
            for (auto& [name, val] : sand::string_to_component) {
              if (val == det) {
                UFW_INFO("Found a primary with hits in {}.", name);
              }
            }
            return pass;
          }
        }
      }
      UFW_INFO("Found no primaries with hits in the required subdetectors.");
      return block;
    }

   private:
    std::vector<sand::subdetector_t> m_desired;
  };

} // namespace sand::filter

UFW_REGISTER_FILTER(sand::filter::track_in_detector)
UFW_REGISTER_DYNAMIC_FILTER_FACTORY(sand::filter::track_in_detector)
