#ifndef SAND_CAF_WRAPPERS_HPP
#define SAND_CAF_WRAPPERS_HPP

#include <data.h>

#include <duneanaobj/StandardRecord/StandardRecord.h>

namespace sand::caf {

  struct standard_record_wrapper
    : public ::caf::StandardRecord
    , public sand::managed_data_base {};

  struct truth_branch_wrapper
    : public ::caf::SRTruthBranch
    , public sand::managed_data_base {};

  struct common_reco_branch_wrapper
    : public ::caf::SRCommonRecoBranch
    , public sand::managed_data_base {};

  struct nd_reco_branch
    : public ::caf::SRNDBranch
    , public sand::managed_data_base {};

} // namespace sand::caf

UFW_DECLARE_MANAGED_DATA(sand::caf::standard_record_wrapper);
UFW_DECLARE_MANAGED_DATA(sand::caf::truth_branch_wrapper);
UFW_DECLARE_MANAGED_DATA(sand::caf::common_reco_branch_wrapper);
UFW_DECLARE_MANAGED_DATA(sand::caf::nd_reco_branch);

#endif // SANDRECO_CAF_WRAPPER_HPP
