#pragma once

#include "Track.hh"

#include "tracker/tracklet_map.h"
#include "tracker/s_particle_infos.h"
#include "common/sand.h"

#include "TMath.h"

namespace sand::tracker {
    class Manager {

        private:
            Track this_track_;
            TrackStep::TrackStateStage current_stage_; // forward or backward
            int current_step_; // index of the TrackStep in Track
            double current_z_; 
            tracklet_map* z_to_tracklets_;
            s_particle_infos::s_particle_info particleInfo_;
        
          public:
            enum class Orientation {
            kVertical,
            kHorizontal
            };
            Orientation getOrientation() {return current_orientation_;};
            mom_3d getDirectiveCosinesFromStateVector(const StateVector& state_vector);
            double getPhiFromTheta(double theta, int charge) { return theta - charge * 0.5*TMath::Pi(); };
            double getThetaFromPhi(double phi, int charge) { return phi + charge * 0.5*TMath::Pi(); };
            double getThetaFromPhi(const StateVector& state_vector) { return getThetaFromPhi(state_vector.phi(), state_vector.charge()); };
            Measurement getMeasurementFromCluster(int cluster_id);

            // TMatrixD getInitialCovMatrix(const StateVector& state_vector, const Orientation& orientation);
            TMatrixD getPropagatorMatrix(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass);  // Propagate and Smooth
            TMatrixD getProcessNoiseMatrix(const StateVector& state_vector, double next_phi, double dz, double de, double z, double particle_mass);          // Propagate
            TMatrixD getMeasurementNoiseMatrix();               // Filter
            TMatrixD getProjectionMatrix(Orientation orientation, const StateVector& state_vector);                     // Filter
            TMatrixD getKalmanGainMatrix(const TMatrixD& covariance_matrix,
                                        const TMatrixD& projection_matrix,
                                        const TMatrixD& measurement_noise_matrix);                     // Filter
            TMatrixD getAMatrix(const TMatrixD& covariance_matrix_filtered,
                                const TMatrixD& covariance_matrix_next_predicted,
                                const TMatrixD& propagator_matrix);                     // Smooth

            Orientation current_orientation_ = Orientation::kVertical;
            Measurement getMeasurementFromTracklet(const tracklet_map::tracklet& tracklet);
            double deltaRadius(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const;
            inline double dEDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { auto tan = state_vector.tanLambda(); return de * tan / (1 + tan*tan); };
            inline double dEDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { 
            if (de == 0) {
                return 0.;
            } else {
                return -de / tan(state_vector.phi());
            }
            };
            inline double dDeltaInvRDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; };
            inline double dDeltaInvRDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; };
            inline double dDeltaInvRDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { 
            double tan = state_vector.tanLambda();
            double constant_tan = 0.299792458 * 0.6 / sqrt(1 + pow(tan, 2));
            double x = 1./state_vector.radius();
            double squared_root = sqrt(pow(constant_tan, 2) / pow(x, 2) + pow(particle_mass, 2));
            double new_derivatives_r = state_vector.charge() * (-de * (2 * pow(constant_tan, 2) + 3 * pow(particle_mass, 2) * pow(x, 2)) / pow(constant_tan, 2) / squared_root);
            return new_derivatives_r;
            };

            inline double dDeltaInvRDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { 
            auto tan = state_vector.tanLambda(); 
            if (de != 0) {
                double old_derivatives = deltaRadius(state_vector, next_phi, dz, de, particle_mass) * (1./de * dEDTanl(state_vector, next_phi, dz, de, particle_mass) + tan / (1 + tan*tan));
                double c = -(pow(1./state_vector.radius(), 3)) / pow(0.299792458 * 0.6, 2);
                double f = tan*tan + 1;
                double g = sqrt(pow(0.299792458 * 0.6, 2) /
                                pow(1./state_vector.radius(), 2) / (1 + tan*tan) + pow(particle_mass, 2));
                double h = de;
                double constant_r = 0.299792458 * 0.6 / (1. / state_vector.radius());
                double f_derivatives = 2 * tan;
                double g_derivatives = -pow(constant_r, 2) * tan / pow(tan*tan + 1, 2) / sqrt(pow(constant_r, 2) / (tan*tan + 1) + pow(particle_mass, 2));
                double h_derivatives = de * tan / (1 + tan*tan);
                double new_derivatives_tan = c * (f_derivatives*g*h + f*g_derivatives*h + f*g*h_derivatives);
                return new_derivatives_tan;
            } else {
                return deltaRadius(state_vector, next_phi, dz, de, particle_mass) * ( tan / (1 + tan*tan)); 
            }
            };
            
            inline double dDeltaInvRDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { 
            if (de == 0) {
                return 0.;
            } else {
                return deltaRadius(state_vector, next_phi, dz, de, particle_mass) / de * dEDPhi(state_vector, next_phi, dz, de, particle_mass); 
            }
            };

            inline double dPhiDCosPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { 
            return state_vector.charge()/sqrt(1 - pow(cos(state_vector.phi()) + dz/state_vector.radius(),2));
            };

            inline double dxDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 1; }
            inline double dyDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
            inline double dInvCRDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return state_vector.charge() * dDeltaInvRDx(state_vector, next_phi, dz, de, particle_mass); }
            inline double dTanlDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
            inline double dPhiDx(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }

            inline double dxDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
            inline double dyDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 1.; }
            inline double dInvCRDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return state_vector.charge() * dDeltaInvRDy(state_vector, next_phi, dz, de, particle_mass); }
            inline double dTanlDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
            inline double dPhiDy(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }

            inline double dPhiDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const 
            {
            return state_vector.charge() * dz * dPhiDCosPhi(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dxDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return -pow(state_vector.chargedRadius(),2) * state_vector.tanLambda() * (next_phi - state_vector.phi()) + state_vector.chargedRadius() * state_vector.tanLambda() * dPhiDInvCR(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dyDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return -pow(state_vector.radius(), 2) * state_vector.charge() * (sin(next_phi) - sin(state_vector.phi())) 
                    + state_vector.radius() * cos(next_phi) * dPhiDInvCR(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dInvCRDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return 1. + state_vector.charge() * dDeltaInvRDInvCR(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dTanlDInvCR(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }

            inline double dxDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return state_vector.chargedRadius() * (next_phi - state_vector.phi());
            }
            inline double dyDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
            inline double dInvCRDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return state_vector.charge() * dDeltaInvRDTanl(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dTanlDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 1.; }
            inline double dPhiDTanl(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }

            inline double dPhiDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            int dPhiDPhi_sign = ((next_phi - state_vector.phi()) * state_vector.charge()) > 0 ? 1 : -1;
            return -sin(state_vector.phi()) * dPhiDCosPhi(state_vector, next_phi, dz, de, particle_mass);
            }
            inline double dxDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return state_vector.chargedRadius() * state_vector.tanLambda() * (dPhiDPhi(state_vector, next_phi, dz, de, particle_mass) - 1.);
            }
            inline double dyDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const
            {
            return state_vector.radius() * (cos(next_phi) * dPhiDPhi(state_vector, next_phi, dz, de, particle_mass) - cos(state_vector.phi()));
            }
            inline double dInvCRDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return state_vector.charge() * dDeltaInvRDPhi(state_vector, next_phi, dz, de, particle_mass); }
            inline double dTanlDPhi(const StateVector& state_vector, double next_phi, double dz, double de, double particle_mass) const { return 0.; }
        
            StateVector propagateState(const StateVector& state_vector, double dz, double de, double particle_mass);
            StateCovarianceMatrix propagateCovMatrix(const TMatrixD& covariance_matrix,
                                                        const TMatrixD& propagatorMatrix,
                                                        const TMatrixD& processNoiseMatrix);

            StateVector filterState(const StateVector& state_vector, 
                                        const TMatrixD& kalmanGainMatrix,
                                        const Measurement& observed, 
                                        const Measurement& predicted);

            StateCovarianceMatrix filterCovMatrix(const TMatrixD& covariance_matrix,
                                                    const TMatrixD& projection_matrix,
                                                    const TMatrixD& measurement_noise_matrix);

            StateVector smoothState(const StateVector& stateVectorFiltered, 
                                        const StateVector& stateVectorPreviousSmoothed,
                                        const StateVector& stateVectorPreviousPredicted, 
                                        const TMatrixD& theAMatrix);

            StateCovarianceMatrix smoothCovMatrix(const TMatrixD& covariance_matrix_filtered,
                                                    const TMatrixD& covarianceMatrixPreviousSmoothed,
                                                    const TMatrixD& covarianceMatrixPreviousPredicted,
                                                    const TMatrixD& theAMatrix);
            
            Prediction getPrediction(Orientation orientation, const StateVector& state_vector);

            void propagate(double& de, double& dz, double& beta);
            double evalChi2(const Measurement& measurement, const Measurement& prediction, const TMatrixD& measurement_noise_matrix);
            int findBestMatch(double& nextZ, const Measurement& prediction, const TMatrixD& measurement_noise_matrix);
            void setNextOrientation();
            void filter(const Measurement& measurement, const Measurement& prediction);
            void smooth();
            void initFromMC(tracklet_map* z_to_tracklets, const s_particle_infos::s_particle_info& particloInfo);
            void initFromSeed(tracklet_map* three_tracklets, tracklet_map* z_to_tracklets, const s_particle_infos::s_particle_info& particloInfo, double sx = 0.004, double sy = 0.004);
            tracklet_map FindSeedPoints_MCstart(tracklet_map* z_to_tracklets, const s_particle_infos::s_particle_info& particloInfo, int maxSteps=10);
            double findClosestNonEmptyKey(const tracklet_map& myMap, double target);
            void run();
            const Track& getTrack() {return this_track_; };
            void EvaluateInnovation(const Measurement& measurement, const Prediction& prediction, const TMatrixD& Sk);            
    };

}