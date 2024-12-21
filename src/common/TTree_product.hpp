#pragma once

#include <product.hpp>

class TFile;
class TTree;

namespace sand {

namespace common {

  class TTree_product_base : public ufw::product {

  public:
    virtual ~TTree_product_base();

    void select(const ufw::select_key&) override;

  protected:
    ufw::product_ptr clone() const override;

    void read() override;

    void write() const override;

  private:
    TFile* m_file;
    TTree* m_tree;

  };

  template <typename DataT>
  class TTree_product : public DataT, public TTree_product_base {

  public:
    virtual ~TTree_product();

    void select(const ufw::select_key&) override;

  protected:
    ufw::product_ptr clone() const override;

    void read() override;

    void write() const override;

  };

}

}
