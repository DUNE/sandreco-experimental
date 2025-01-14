#include <example.h>
#include <factory.hpp>
#include <TTreeData.hpp>

UFW_REGISTER_DATA(sand::example, sand::common::TTreeData<sand::example>)

UFW_REGISTER_DYNAMIC_DATA_FACTORY(sand::example, sand::common::TTreeData<sand::example>)
