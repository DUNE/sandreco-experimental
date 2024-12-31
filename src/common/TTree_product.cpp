#include <TFile.h>

#include <TTree_product.hpp>

namespace sand {

namespace common {

using namespace ufw;

TTree_product_base::TTree_product_base() :
  m_tree(new TTree("TTree_product", "")), m_file(nullptr) {
    m_tree->SetDirectory(nullptr);
  }

TTree_product_base::~TTree_product_base() {
  delete m_tree;
  delete m_file;
}

void TTree_product_base::configure(const ufw::config& cfg) {
  product::configure(cfg);
  m_filename = cfg.at("file");
  m_treename = cfg.value("tree", std::string(m_tree->GetName()));
  json::array_t dims = cfg.value("dims", json::array_t{});
  for (auto dim: dims)
    m_dims.emplace_back(dim);
  m_tree->SetName(m_treename.c_str());
}

void TTree_product_base::select(const select_key& k) {
  //FIXME temporary
  switch (k.index()) {
    case 0: //nullptr
      break;
    case 1: //std::size_t
      m_tree->GetEntry(std::get<std::size_t>(k));
      break;
    case 2: //double
      throw select_error(id(), k);
    case 3: //std::string
      throw select_error(id(), k);
    };
}

product_ptr TTree_product_base::clone() const {
  throw std::logic_error("Can not clone: unsupported operation.");
  //TODO use CloneTree()
}

void TTree_product_base::read() {
  delete m_tree;
  m_file = TFile::Open(m_filename.c_str());
  if (!m_file || m_file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  m_tree = m_file->Get<TTree>(m_treename.c_str());
  if (!m_tree)
    throw std::runtime_error("Can not read tree " + m_treename + " from " + m_filename);
}

void TTree_product_base::write() const {
  m_file = TFile::Open(m_filename.c_str(), "RECREATE");
  if (!m_file || m_file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  m_tree->SetDirectory(m_file);
  m_tree->Write();
  m_tree->SetDirectory(nullptr);
  delete m_file;
  m_file = nullptr;
}
  
}

}
