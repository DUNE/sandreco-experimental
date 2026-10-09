#pragma once

#include <common/data.h>
#include <edep_reader_refactor/EDEPTree.h>

#include <EDepSim/TG4Event.h>

class TFile;
class TTree;

namespace sand {

  /**
   * Drop-in, faster replacement of sand::edep_reader. The tree classes live in sand::edep_refactor.
   * Note that sand::truth_adapter (hit lookup by truth_index) is still provided by the legacy module;
   * use GetHit() on this reader to look hits up here.
   */
  class edep_reader_refactor
    : public edep_refactor::EDEPTree
    , public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::context_tag> {
    TG4Event* m_event{nullptr};

    friend class ufw::data::factory<sand::edep_reader_refactor>;

   public:
    TG4Event const& event() const {
      if (m_event != nullptr) {
        return *m_event;
      }
      UFW_ERROR("Event not initialized");
    }
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::edep_reader_refactor);

template <>
class ufw::data::factory<sand::edep_reader_refactor> {
 public:
  factory(const ufw::config&);
  ~factory();
  sand::edep_reader_refactor& instance(ufw::context_id);

 private:
  sand::edep_reader_refactor reader;
  std::unique_ptr<TFile> input_file;
  TTree* input_tree;
  std::unique_ptr<TG4Event> event_owner;
  TG4Event* event; ///< the branch address: must outlive the SetAddress() call
  ufw::context_id m_id;
};
