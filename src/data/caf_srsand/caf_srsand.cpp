#include <duneanaobj/StandardRecord/SRSAND.h>
#include <factory.hpp>
#include <TTreeData.hpp>

UFW_REGISTER_DATA(caf::SRSANDInt, sand::common::TTreeData<caf::SRSANDInt>)

UFW_REGISTER_DYNAMIC_DATA_FACTORY(caf::SRSANDInt, sand::common::TTreeData<caf::SRSANDInt>)
