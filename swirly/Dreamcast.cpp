#include "Dreamcast.h"

#include "SHCpu.h"
#include "SHMmu.h"
#include "SHTmu.h"
#include "SHBsc.h"
#include "SHSci.h"
#include "SHUbc.h"
#include "SHDmac.h"
#include "SHIntc.h"
#include "Gpu.h"
#include "Modem.h"
#include "Spu.h"
#include "Gdrom.h"
#include "Maple.h"
#include "Overlord.h"
#include "Debugger.h"

SHCpu *cpu; 
SHMmu *mmu;
SHTmu *tmu;
SHBsc *bsc;
SHUbc *ubc;
SHSci *sci;
SHDmac *dmac;
SHIntc *intc;

Overlord *overlord;
Modem *modem;
Gdrom *gdrom;
Maple *maple;
Gpu *gpu;
Spu *spu;

Dreamcast::Dreamcast(Dword setting) {

    overlord = new Overlord();

    cpu = new SHCpu();
    modem = new Modem();
    dmac = new SHDmac();
    intc = new SHIntc();
    mmu = new SHMmu();
    tmu = new SHTmu();
    bsc = new SHBsc();
    sci = new SHSci();
    ubc = new SHUbc();
    gpu = new Gpu();
    spu = new Spu();
    gdrom = new Gdrom(0); // 13986
    maple = new Maple(setting);
}

Dreamcast::~Dreamcast() {
    delete cpu;
    delete overlord;
    delete modem;
    delete maple;
    delete gdrom;
    delete dmac;
    delete intc;
    delete mmu;
    delete gpu;
    delete tmu;
    delete bsc;
    delete sci;
    delete ubc;
    delete spu;
}

void Dreamcast::run() {
    cpu->go();
}

