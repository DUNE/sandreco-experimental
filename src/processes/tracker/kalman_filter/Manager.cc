#include "Manager.hh"

namespace sand::tracker {
    mom_3d Manager::getDirectiveCosinesFromStateVector(
    const StateVector& stateVector)
    {
        auto theta = getThetaFromPhi(stateVector);
        if (theta > M_PI_2) theta -= M_PI;
        mom_3d dir(
            stateVector.tanLambda() / sqrt(1 + pow(stateVector.tanLambda(), 2)),
            sin(theta), cos(theta));
        dir *= 1. / dir.R();
        return dir;
    }

    Measurement Manager::getMeasurementFromCluster(
    int clusterID)
    {
        Measurement measurement(2, 1);
        return measurement;
    }

    TMatrixD Manager::getPropagatorMatrix(
        const StateVector& stateVector, double nextPhi, double dZ, double dE, double particle_mass)
    {
        // PRIMO INDICE = RIGA
        TMatrixD propagatorMatrix(5, 5);
        propagatorMatrix[0][0] = dxDx(stateVector, nextPhi, dZ, dE, particle_mass);         
        propagatorMatrix[0][1] = dxDy(stateVector, nextPhi, dZ, dE, particle_mass);         
        propagatorMatrix[0][2] = dxDInvCR(stateVector, nextPhi, dZ, dE, particle_mass);     
        propagatorMatrix[0][3] = dxDTanl(stateVector, nextPhi, dZ, dE, particle_mass);      
        propagatorMatrix[0][4] = dxDPhi(stateVector, nextPhi, dZ, dE, particle_mass);       
        propagatorMatrix[1][0] = dyDx(stateVector, nextPhi, dZ, dE, particle_mass);         
        propagatorMatrix[1][1] = dyDy(stateVector, nextPhi, dZ, dE, particle_mass);         
        propagatorMatrix[1][2] = dyDInvCR(stateVector, nextPhi, dZ, dE, particle_mass);     
        propagatorMatrix[1][3] = dyDTanl(stateVector, nextPhi, dZ, dE, particle_mass);      
        propagatorMatrix[1][4] = dyDPhi(stateVector, nextPhi, dZ, dE, particle_mass);       
        propagatorMatrix[2][0] = dInvCRDx(stateVector, nextPhi, dZ, dE, particle_mass);     
        propagatorMatrix[2][1] = dInvCRDy(stateVector, nextPhi, dZ, dE, particle_mass);     
        propagatorMatrix[2][2] = dInvCRDInvCR(stateVector, nextPhi, dZ, dE, particle_mass); 
        propagatorMatrix[2][3] = dInvCRDTanl(stateVector, nextPhi, dZ, dE, particle_mass);  
        propagatorMatrix[2][4] = dInvCRDPhi(stateVector, nextPhi, dZ, dE, particle_mass);   
        propagatorMatrix[3][0] = dTanlDx(stateVector, nextPhi, dZ, dE, particle_mass);      
        propagatorMatrix[3][1] = dTanlDy(stateVector, nextPhi, dZ, dE, particle_mass);      
        propagatorMatrix[3][2] = dTanlDInvCR(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[3][3] = dTanlDTanl(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[3][4] = dTanlDPhi(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[4][0] = dPhiDx(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[4][1] = dPhiDy(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[4][2] = dPhiDInvCR(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[4][3] = dPhiDTanl(stateVector, nextPhi, dZ, dE, particle_mass);
        propagatorMatrix[4][4] = dPhiDPhi(stateVector, nextPhi, dZ, dE, particle_mass);
        return propagatorMatrix;
    }
}