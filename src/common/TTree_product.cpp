#include <TFile.h>

#include <TTree_product.hpp>

namespace sand {

namespace common {

using namespace ufw;

TTree_product_base::TTree_product_base() :
  m_tree(new TTree("TTree_product", "")) {
    m_tree->SetDirectory(nullptr);
  }

TTree_product_base::~TTree_product_base() {
  delete m_tree;
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

void TTree_product_base::select(const select_key&) {}

product_ptr TTree_product_base::clone() const {
  throw std::logic_error("Can not clone: unsupported operation.");
  //TODO use CloneTree()
}

void TTree_product_base::read() {
  std::unique_ptr<TFile> file(TFile::Open(m_filename.c_str()));
  if (!file || file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  TTree* tmp = file->Get<TTree>(m_treename.c_str());
  if (tmp) {
    std::swap(m_tree, tmp);
    delete tmp;
    m_tree->SetDirectory(nullptr);
  }
}

void TTree_product_base::write() const {
  std::unique_ptr<TFile> file(TFile::Open(m_filename.c_str(), "RECREATE"));
  if (!file || file->IsZombie())
    throw std::runtime_error("Can not open " + m_filename);
  m_tree->SetDirectory(file.get());
  m_tree->Write();
}
  
}

}
