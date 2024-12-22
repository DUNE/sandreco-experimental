#pragma once

#include <product.hpp>
#include <json_product.hpp>

#include <TBranch.h>
#include <TTree.h>

class TFile;

namespace sand {

namespace common {

  class TTree_product_base : public ufw::product {

  public:
    virtual ~TTree_product_base();

    void configure(const ufw::config&) override;

    void select(const ufw::select_key&) override;

  protected:
    TTree_product_base();

    ufw::product_ptr clone() const override;

    TTree* tree() const { return m_tree; };

    void read() override;

    void write() const override;

  private:
    std::string m_filename;
    std::string m_treename;
    TTree* m_tree;
    mutable TFile* m_file;
    std::vector<std::string> m_dims;

  };

  template <typename DataT>
  class TTree_product : public TTree_product_base {

  public:
    TTree_product& operator = (const DataT& d) {
      *m_data = d;
      m_dirty = true;
      return *this;
    }

    operator DataT& () { return *m_data; }

    operator const DataT& () const { return *m_data; }

    void configure(const ufw::config& cfg) override {
      TTree_product_base::configure(cfg);
      m_branchname = cfg.at("branch");
      m_branch = tree()->Branch(m_branchname.c_str(), &m_data);
      if (!m_branch)
        throw std::runtime_error("Cannot create branch " + m_branchname);
    }

    void select(const ufw::select_key& k) override {
      if (m_dirty)
        tree()->Fill();
      m_dirty = false;
      TTree_product_base::select(k);
    }

    std::size_t entries() const { return tree()->GetEntries(); }

    void read() override {
      TTree_product_base::read();
      //tmp m_branch is deleted by the the TTree
      m_branch = tree()->FindBranch(m_branchname.c_str());
      if (!m_branch)
        throw std::runtime_error("Cannot find branch " + m_branchname);
      m_branch->SetAddress(&m_data);
    }

    void write() const override {
      if (m_dirty)
        tree()->Fill();
      m_dirty = false;
      TTree_product_base::write();
    }

  private:
    std::string m_branchname;
    TBranch* m_branch = nullptr;
    DataT* m_data = nullptr;
    mutable bool m_dirty = false;

  };

}

}
