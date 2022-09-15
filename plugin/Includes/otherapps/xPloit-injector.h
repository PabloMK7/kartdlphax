#include "xPloit_injector/O3DS/AMERICA/o3ds_america.h"
#include "xPloit_injector/O3DS/EUROPE/o3ds_europe.h"
#include "xPloit_injector/O3DS/JAPAN/o3ds_japan.h"
#include "xPloit_injector/N3DS/AMERICA/n3ds_america.h"
#include "xPloit_injector/N3DS/EUROPE/n3ds_europe.h"
#include "xPloit_injector/N3DS/JAPAN/n3ds_japan.h"

const unsigned char* xploit_injector_bins[2][3] = {
    {
        o3ds_europe_xploit_injector,
        o3ds_america_xploit_injector,
        o3ds_japan_xploit_injector,
    },
    {
        n3ds_europe_xploit_injector,
        n3ds_america_xploit_injector,
        n3ds_japan_xploit_injector,
    }
};

const long int xploit_injector_bins_sizes[2][3] = {
    {
        o3ds_europe_xploit_injector_size,
        o3ds_america_xploit_injector_size,
        o3ds_japan_xploit_injector_size,
    },
    {
        n3ds_europe_xploit_injector_size,
        n3ds_america_xploit_injector_size,
        n3ds_japan_xploit_injector_size,
    }
};