#include <TFile.h>

#include <TTreeData.hpp>

namespace sand {

  namespace common {

    using namespace ufw;

    TTreeDataBase::~TTreeDataBase() {
      assert(!m_dirty);
    }

    void TTreeDataBase::configure(const ufw::config& cfg) {
      TObjectWrapper::configure(cfg);
    }

    void TTreeDataBase::flush() {
      if (m_dirty)
        tree()->Fill();
      m_dirty = false;
    }

    const TTree* TTreeDataBase::tree() const {
      return static_cast<const TTree*>(object());
    }

    TTree* TTreeDataBase::tree() {
      return static_cast<TTree*>(object());
    }

    void TTreeDataBase::select(const select_key& k) const {
      auto ncthis = const_cast<TTreeDataBase*>(this); //select is semantically const, but it modifies the TTree state
      ncthis->flush();
      switch (k.index()) {
        case 0: //nullptr
          break;
        case 1: //std::size_t
          ncthis->tree()->GetEntry(std::get<std::size_t>(k));
          break;
        default:
          throw select_error(id(), k);
        };
    }

  }

}
