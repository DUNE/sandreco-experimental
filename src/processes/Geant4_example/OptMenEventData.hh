#ifndef __OptMenEVENTDATA_H__
#define __OptMenEVENTDATA_H__

#include <string>
#include <vector>
using std::string;
using std::vector;

class OptMenEventData
{
  public:
    OptMenEventData();
    ~OptMenEventData();
    
  public:
    void Clear();
    
  public:
  
	 double xVertex, yVertex, zVertex;
	 std::vector<double> x, y, z;
	 std::vector<double> xOrigin, yOrigin, zOrigin;
   std::vector<double> px, py, pz;
	 std::vector<int> pdg, ID;
   std::vector<double> energy, time, scatter;
	 int eventID, innerPhotons;
};

#endif // __OptMenEVENTDATA_H__

