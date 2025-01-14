#pragma once

#include <TBranch.h>
#include <TTree.h>

#include <config.hpp>
#include <TObjectWrapper.hpp>

namespace sand {

  namespace common {

    class TTreeDataBase : public TObjectWrapper {

    public:
      ~TTreeDataBase() override;

      void configure(const ufw::config&) override;

      std::size_t entries() const { return tree()->GetEntries(); }

      void select(const ufw::select_key&) const override;

      TTree* tree();

      const TTree* tree() const;

    protected:
      void touch() { m_dirty = true; }

      void flush();

    private:
      mutable bool m_dirty = false;

    };


    template <typename DataT>
    class TTreeData : public TTreeDataBase {

    public:
      TTreeData& operator = (const DataT& d) {
        *m_data = d;
        touch();
        return *this;
      }

      void configure(const ufw::config& cfg) override {
        TTreeDataBase::configure(cfg);
        m_branchname = cfg.at("branch");
        m_branch = tree()->Branch(m_branchname.c_str(), &m_data);
        if (!m_branch)
          throw std::runtime_error("Cannot create branch " + m_branchname);
      }

    protected:
      void* get() override { touch(); return m_data; }

      const void* get() const override { return m_data; }

/*    void read() override {
      TTree_product_base::read();
      //tmp m_branch is deleted by the the TTree
      m_branch = get_tree()->FindBranch(m_branchname.c_str());
      if (!m_branch)
        throw std::runtime_error("Cannot find branch " + m_branchname);
      m_branch->SetAddress(&m_data);
    }
*/
    private:
      std::string m_branchname;
      TBranch* m_branch = nullptr;
      DataT* m_data = nullptr;

    };

  }

}
