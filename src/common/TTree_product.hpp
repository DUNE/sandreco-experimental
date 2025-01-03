#pragma once

#include <config.hpp>
#include <product.hpp>

#include <TBranch.h>
#include <TTree.h>

class TFile;

namespace sand {

namespace common {

  class TTree_product_base : public ufw::product {

  public:
    virtual ~TTree_product_base();

    void configure(const ufw::config&) override;

    ufw::product_ptr clone() const override;

    std::size_t entries() const { return m_tree->GetEntries(); }

    void select(const ufw::select_key&) override;

    void read() override;

    TTree* tree() { touch(); return m_tree; }

    const TTree* tree() const { return m_tree; }

    void write() const override;

  protected:
    TTree_product_base();

    void touch() { m_dirty = true; }

    TTree* get_tree() { return m_tree; };

    void flush() const;

  private:
    std::string m_filename;
    std::string m_treename;
    mutable TTree* m_tree;
    mutable TFile* m_file;
    mutable bool m_dirty;

  };

  template <typename DataT>
  class TTree_product : public TTree_product_base {

  public:
    TTree_product& operator = (const DataT& d) {
      *m_data = d;
      touch();
      return *this;
    }

    void* data() override { touch(); return m_data; }

    const void* data() const override { return m_data; }

    void configure(const ufw::config& cfg) override {
      TTree_product_base::configure(cfg);
      m_branchname = cfg.at("branch");
      m_branch = get_tree()->Branch(m_branchname.c_str(), &m_data);
      if (!m_branch)
        throw std::runtime_error("Cannot create branch " + m_branchname);
    }

    void read() override {
      TTree_product_base::read();
      //tmp m_branch is deleted by the the TTree
      m_branch = get_tree()->FindBranch(m_branchname.c_str());
      if (!m_branch)
        throw std::runtime_error("Cannot find branch " + m_branchname);
      m_branch->SetAddress(&m_data);
    }

  private:
    std::string m_branchname;
    TBranch* m_branch = nullptr;
    DataT* m_data = nullptr;

  };

}

}
