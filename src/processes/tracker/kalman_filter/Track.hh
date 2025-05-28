#ifndef SANDKFTRACK_H
#define SANDKFTRACK_H

#include <common/truth.h>
#include <common/sand.h>

#include "State.hh"
#include "TMatrixD.h"

namespace sand::tracker {

    class TrackStep {

        public:
            enum class TrackStateStage {
            kPrediction,
            kFiltering,
            kSmoothing,
            };

        private:
            State prediction_;
            State filtered_;
            State smoothed_;
            std::vector<double> innovation_;
            double z_;
            double x_;
            double y_;


            // the propagation that bring the vector in this state
            TMatrixD propagator_matrix_; 

            // ID piano di misura;
            geo_id plane_id_;
            channel_id clusterid_; // Not sure if this is correct

        public:
            TrackStep(): propagator_matrix_(5,5) {}; 
            void setPlaneID(const geo_id& plane_id) {plane_id_ = plane_id; };
            const geo_id& getPlaneID() const {return plane_id_; };
            void setClusterIDForThisState(channel_id cluster_id) { clusterid_ = cluster_id; };
            channel_id getClusterIDForThisState() const { return clusterid_; }
            void setStage(TrackStateStage stage, State state);
            const State& getStage(TrackStateStage stage) const;
            void setPropagatorMatrix(TMatrixD propagator_matrix) { propagator_matrix_ = propagator_matrix; };
            const TMatrixD getPropagatorMatrix() { return propagator_matrix_; };
            void setInnovation(std::vector<double> innovation) { innovation_ = innovation; };
            const std::vector<double>& getInnovation() const { return innovation_ ;};
            void setZ(double z){z_ = z;};
            double getZ() const {return z_;};
            void setX(double x){x_ = x;};
            double getX() const {return x_;};
            void setY(double y){y_ = y;};
            double getY() const {return y_;};
    };

}



#endif