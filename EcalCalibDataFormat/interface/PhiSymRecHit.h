#ifndef DATAFORMAT_PHISYMRECHIT_H
#define DATAFORMAT_PHISYMRECHIT_H

/** \class PhiSymRecHit
 * 
 * Dataformat dedicated to Phi Symmetry ecal calibration
 *
 */

#include <vector>

#include "DataFormats/DetId/interface/DetId.h"

class PhiSymRecHit
{
public:
    //---ctors---
    PhiSymRecHit();
    PhiSymRecHit(DetId id, float energy=0);
    
    //---dtor---
    ~PhiSymRecHit(){};

    inline void AddEnergy(float energy){e_sum_ += energy;};

private:

    DetId id_;
    float e_sum_;
};

namespace edm
{
    typedef std::vector<PhiSymRecHit> PhiSymRecHitCollection;
}

#endif