#include <example.h>
#include <factory.hpp>
#include <TTree_product.hpp>

UFW_DECLARE_DATA_TYPE(sand::example)

using TTree_example = sand::common::TTree_product<sand::example>;

UFW_REGISTER_DATA_PRODUCT(sand::example, TTree_example)
UFW_REGISTER_DYNAMIC_DATA_PRODUCT_FACTORY(sand::example, TTree_example)
