#include "OptMenEventData.hh"

OptMenEventData::OptMenEventData()
{

}

OptMenEventData::~OptMenEventData()
{
	
}

void OptMenEventData::Clear()
{
  std::vector<int>().swap(ID);
  std::vector<int>().swap(pdg);
  std::vector<double>().swap(energy);
  std::vector<double>().swap(time);
  std::vector<double>().swap(scatter);
  std::vector<double>().swap(x);
  std::vector<double>().swap(y);
  std::vector<double>().swap(z);
  std::vector<double>().swap(xOrigin);
  std::vector<double>().swap(yOrigin);
  std::vector<double>().swap(zOrigin);
  std::vector<double>().swap(px);
  std::vector<double>().swap(py);
  std::vector<double>().swap(pz);
}

