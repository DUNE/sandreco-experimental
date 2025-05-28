#pragma once

#include "TMatrixD.h"

namespace sand::tracker {

    using StateCovarianceMatrix = TMatrixD;
    using Measurement = TMatrixD;
    using Prediction  = TMatrixD;

    class StateVector {

        private:
        TMatrixD vector_;
        
        public:
        // default constructors ... probably to be delete in the future
        StateVector(): vector_(5,1) {};

        // constructors
        StateVector(double arg_x, double arg_y, double arg_signed_inv_radius, double arg_tan_lambda, double arg_phi): vector_(5,1) {
            vector_(0,0) = arg_x;
            vector_(1,0) = arg_y;
            vector_(2,0) = arg_signed_inv_radius;
            vector_(3,0) = arg_tan_lambda;
            vector_(4,0) = arg_phi;
        };

        // constructors
        StateVector(TMatrixD vector): vector_(vector) {};
        
        // destructor
        ~StateVector() {};

        // copy assignment
        StateVector& operator=(const TMatrixD& vec) {
            vector_ = vec;
            return *this;
        };

        StateVector& operator=(TMatrixD vec) {
            std::swap(vector_, vec);
            return *this;
        };

        StateVector operator+ (StateVector p2) {
            this->vector_ += p2();
            return *this;
        };

        StateVector operator- (StateVector p2) {
            this->vector_ -= p2();
            return *this;
        };

        StateVector operator* (StateVector p2) {
            this->vector_ = ElementMult(this->vector_, p2());
            return *this;
        };

        // operator()
        const TMatrixD& operator()() const { return vector_; };

        // getters
        inline double x() const { return vector_(0, 0); };
        inline double y() const { return vector_(1, 0); };
        inline double signedInverseRadius() const { return vector_(2, 0); };
        inline double tanLambda() const { return vector_(3, 0); };
        inline double phi() const { return vector_(4, 0); };

        // usefull function
        inline int charge() const { return std::signbit(signedInverseRadius()) == false ? +1 : -1; };
        inline double chargedRadius() const { return 1./signedInverseRadius(); };
        inline double radius() const { return chargedRadius() * charge(); };
    };

    class State {
        StateVector vector_;
        StateCovarianceMatrix cov_matrix_;

        public:
        State(): vector_(), cov_matrix_(5,5) {};
        State(StateVector vector, StateCovarianceMatrix matrix): vector_(vector), cov_matrix_(matrix) {};
        const StateVector& getStateVector() const {return vector_; };
        const StateCovarianceMatrix& getStateCovMatrix() const {return cov_matrix_; };
    };

}

