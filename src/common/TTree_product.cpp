#include <TFile.h>
#include <TTree.h>

#include <TTree_product.hpp>

namespace sand {

namespace common {

using namespace ufw;

TTree_product_base::~TTree_product_base() {}

void TTree_product_base::select(const select_key&) {}

product_ptr TTree_product_base::clone() const {}

void TTree_product_base::read() {}

void TTree_product_base::write() const {}

}

}
