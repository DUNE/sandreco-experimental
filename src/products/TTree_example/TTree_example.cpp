#include <example.h>
#include <factory.hpp>
#include <TTree_product.hpp>

using TTree_example = sand::common::TTree_product<sand::example>;

UFW_REGISTER_OBJECT_TYPE(TTree_example)
UFW_REGISTER_DYNAMIC_FACTORY(TTree_example)
