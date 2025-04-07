#ifndef __CALRORDERFILE_H__
#define __CALRORDERFILE_H__
#include "OptMenReadParameters.hh"

#include <TFile.h>
#include <TROOT.h>
#include <TTree.h>
#include <TKey.h>
#include <TCollection.h>

#include <string>
#include <vector>

#include <iostream>
#include <memory>
#include <fstream>

class OptMenOrderFile {
  public:
    OptMenOrderFile();
    virtual ~OptMenOrderFile();
    
    void inizializeInputTree();

    void reorderFile(G4String tmpName, G4String outName);

    TTree* inizializeOutputSensorsTree();
    TTree* inizializeOutputOpticalTree();
    TTree* inizializeOutputPrimaryTree();
    
    void addFirstElement();
    void eraseFirstElement();
    void insertVectors();
    void clearVectors();
  
  private:
  std::vector<G4int> sensorCollID;
  
  TFile *m_pOutputFile;
  TFile *m_pInputFile;

  TNamed* commit_hash;
  TTree *m_pTreeSensor;
  TTree *m_pTreePrimary;
  TTree *m_pTreeOptical;
  TTree *inputTree;
  TTree *outputTree;
  
  TKey *key;

  // Variables used to set input file branches
  int eventID = 0;
  int innerPhotons = 0;
  double xVertex = 0;
  double yVertex = 0;
  double zVertex = 0;
  std::vector<double>* energy = 0;
  std::vector<double>* time = 0;
  std::vector<double>* x = 0;
  std::vector<double>* y = 0;
  std::vector<double>* z = 0;
  std::vector<double>* xOrigin = 0;
  std::vector<double>* yOrigin = 0;
  std::vector<double>* zOrigin = 0;
  std::vector<double>* px = 0;
  std::vector<double>* py = 0;
  std::vector<double>* pz = 0;
  std::vector<double>* scatter = 0;
  std::vector<int>* pdg = 0;
  

  // Variable used to fill the output files
  int sensorNumber = 0;

  int eventIDOrdered;
  int innerPhotonsOrdered;
  double xVertexOrdered;
  double yVertexOrdered;
  double zVertexOrdered;
  std::vector<double> energyOrdered;
  std::vector<double> timeOrdered;
  std::vector<double> xOrdered;
  std::vector<double> yOrdered;
  std::vector<double> zOrdered;
  std::vector<double> xOriginOrdered;
  std::vector<double> yOriginOrdered;
  std::vector<double> zOriginOrdered;
  std::vector<double> pxOrdered;
  std::vector<double> pyOrdered;
  std::vector<double> pzOrdered;
  std::vector<double> scatterOrdered;
  std::vector<int> pdgOrdered;
};
#endif