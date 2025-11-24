#pragma once

#include <common/truth.h>
#include <edep_reader/EDEPTree.h>

#include <ufw/data.hpp>

#include <EDepSim/TG4Event.h>

class TG4HitSegment;
class TFile;
class TTree;

namespace sand {

  class edep_reader
    : public EDEPTree
    , public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::context_tag> {
    TG4Event* m_event{nullptr};

    using EDEPTree::EDEPTree;
    friend class ufw::data::factory<sand::edep_reader>;

   public:
    TG4Event const& event() const {
      if (m_event != nullptr) {
        return *m_event;
      }
      UFW_ERROR("Event not initialized");
    }
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::edep_reader);

template <>
class ufw::data::factory<sand::edep_reader> {
 public:
  factory(const ufw::config&);
  ~factory();
  sand::edep_reader& instance(ufw::context_id);

 private:
  sand::edep_reader reader;
  std::unique_ptr<TFile> input_file;
  TTree* input_tree;
  TG4Event* event;
  ufw::context_id m_id;
};
