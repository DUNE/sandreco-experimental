#include <TFileWrapper.hpp>

#include <TTree_product.hpp>

namespace sand {

namespace common {

using namespace ufw;

TTree_product_base::TTree_product_base() :
  m_tree(new TTree("TTree_product", "")), m_dirty(false) {
    m_tree->SetDirectory(nullptr); //do not attach to a random open TFile...
  }

TTree_product_base::~TTree_product_base() {
  assert(!m_dirty);
  m_tree.reset();
}

void TTree_product_base::configure(const ufw::config& cfg) {
  product::configure(cfg);
  m_filename = cfg.at("file");
  m_treename = cfg.value("tree", std::string(m_tree->GetName()));
  m_tree->SetName(m_treename.c_str());
}

void TTree_product_base::flush() const {
  if (m_dirty)
    m_tree->Fill();
  m_dirty = false;
}

void TTree_product_base::select(const select_key& k) {
  flush();
  switch (k.index()) {
    case 0: //nullptr
      break;
    case 1: //std::size_t
      m_tree->GetEntry(std::get<std::size_t>(k));
      break;
    default:
      throw select_error(id(), k);
    };
}

product_ptr TTree_product_base::clone() const {
  flush();
  throw std::logic_error("Can not clone: unsupported operation.");
  //TODO use CloneTree()
}

void TTree_product_base::read() {
  flush();
  auto file = new utils::TFileWrapper(m_filename.c_str(), "READ");
  if (!file || file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  m_tree.reset();
  m_tree = file->Get<TTree>(m_treename.c_str());
}

void TTree_product_base::write() const {
  flush();
  auto file = new TFile(m_filename.c_str(), "RECREATE");
  if (!file || file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  m_tree->SetDirectory(file);
  m_tree->Write();
  m_tree->SetDirectory(nullptr);
  delete file;
}
  
}

}
