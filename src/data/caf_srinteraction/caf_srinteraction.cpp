#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <factory.hpp>
#include <TTreeData.hpp>

UFW_REGISTER_DATA(caf::SRInteraction, sand::common::TTreeData<caf::SRInteraction>)

UFW_REGISTER_DYNAMIC_DATA_FACTORY(caf::SRInteraction, sand::common::TTreeData<caf::SRInteraction>)
