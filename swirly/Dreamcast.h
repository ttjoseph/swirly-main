#ifndef _DREAMCAST_H_
#define _DREAMCAST_H_

#include "swirly.h"

class Dreamcast {

 public:
    Dreamcast(Dword setting);
    ~Dreamcast();
    
    void run();
    
 private: 
    Dreamcast();
};

#endif
