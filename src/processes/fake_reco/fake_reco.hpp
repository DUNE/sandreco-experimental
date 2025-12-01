//
// Created by paolo on 08/05/2025.
//

/**
 * The TTree read from the genie_reader should be a gRooTracker in "t2k_rootracker" format created from a GHEP format
 * root file (which, as far as I understand, should be the standard output format of GENIE.) with the GENIE "gntpc"
 * utility (source code at
 * https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/Apps/gNtpConv.cxx).
 *
 * Since I'll rely on ND_CafMaker code to fill the truth
 * (https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx) but it
 * uses GHEP formatted data, I'll write down some deductions I made in order to help me and future developer
 * understanding this code:
 * - The EventRecord::Summary() result, often called "interaction" (or "in"), is partially encoded as a string via
 *  `string Interaction::AsString(void) const`
 *  (https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/Framework/Interaction/Interaction.cxx#L246)
 *  and stored in the "EvtCode" branch of the gRooTracker. To read it, I (mostly Gemini 3 tbh) wrote a parser:
 *  EvtCode_parser;
 * - In the StdHep fields of the gRooTraker the convention seems to be: the first particle is the nu and the second one
 *  is the target. Some info on the nu and the target must be read from here since they are not present in the EvtCode
 *  string;
 * - I'm not sure about how we should use the data exclusive of the t2k_rootracker format.
 */

#ifndef FAKE_RECO_HPP
#define FAKE_RECO_HPP

#include <edep_reader/edep_reader.hpp>

#include <ufw/process.hpp>

namespace sand::fake_reco {

  class fake_reco : public ufw::process {
   public:
    fake_reco();

    void configure(const ufw::config& cfg) override;
    void run() override;
  };
} // namespace sand::fake_reco

UFW_REGISTER_PROCESS(sand::fake_reco::fake_reco);

#endif // FAKE_RECO_HPP
