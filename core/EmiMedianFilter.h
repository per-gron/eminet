//
//  EmiMedianFilter.h
//  rock
//
//  Created by Per Eckerdal on 2012-05-22.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiMedianFilter_h
#define rock_EmiMedianFilter_h

#include <algorithm>
#include <cmath>

// This class implements a simple algorithm for a non-linear
// median filter, as used for calculating packet arrival rate
// and link capacity in the UDT congestion control algorithm.
template<typename Element, int BUFFER_SIZE = 64, int TOLERANCE = 8>
class EmiMedianFilter {
    
    Element _elms[BUFFER_SIZE];
    size_t  _frontIdx; // This is the index to the value just ahead of the newest element
    
public:
    explicit EmiMedianFilter(Element defaultValue) :
    _frontIdx(0) {
        std::fill(_elms, _elms+BUFFER_SIZE, defaultValue);
    }
    
    virtual ~EmiMedianFilter() {}
    
    inline void pushValue(Element value) {
        _elms[_frontIdx] = value;
        _frontIdx = (_frontIdx+1) % BUFFER_SIZE;
    }
    
    Element calculate() const {
        Element sortedElms[BUFFER_SIZE];
        std::copy(_elms, _elms+BUFFER_SIZE, sortedElms);
        std::sort(sortedElms, sortedElms+BUFFER_SIZE);
        
        Element median = sortedElms[BUFFER_SIZE/2];
        
        Element sum = 0;
        int count = 0;
        
        for (int i=0; i<BUFFER_SIZE; i++) {
            Element elm = sortedElms[i];
            
            if (TOLERANCE < ((elm > median) ? elm/median : median/elm)) {
                continue;
            }
            
            sum += elm;
            count++;
        }
        
        return sum/count;
    }
};

#endif
