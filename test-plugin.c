#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ladspa.h>
#include <dlfcn.h>
#include "common.h"
#include "common-string.h"

#define BUF_SIZE 1024

float ival[256];
float oval[256];

float linbuffer[BUF_SIZE];
float rinbuffer[BUF_SIZE];

float loutbuffer[BUF_SIZE];
float routbuffer[BUF_SIZE];

float dummydata[BUF_SIZE];
float dummyoutdata[BUF_SIZE];

unsigned int sampleRate = 48000;

/*

Name = 0x80506f8 "White Noise Source", 
  Maker = 0x8050710 "Richard Furse (LADSPA example plugins)", 
  Copyright = 0x8050740 "None", PortCount = 2, PortDescriptors = 0x8050750, 
  PortNames = 0x8050760, PortRangeHints = 0x8050790, ImplementationData = 0x0, 
  instantiate = 0xb7e1c5c0 <instantiateNoiseSource>, 
  connect_port = 0xb7e1c460 <connectPortToNoiseSource>, activate = 0, 
  run = 0xb7e1c9a0 <runNoiseSource>, 
  run_adding = 0xb7e1c930 <runAddingNoiseSource>, 
  set_run_adding_gain = 0xb7e1c490 <setNoiseSourceRunAddingGain>, 
  deactivate = 0, cleanup = 0xb7e1c590 <cleanupNoiseSource>}

*/

int main (int argc, char *argv[])
{
    void *libHandle;
    LADSPA_Descriptor_Function descFunc;
    const LADSPA_Descriptor *pdesc;
    LADSPA_Handle instanceHandle;
    int nInBuffers;
    int nOutBuffers;
    int nInPorts;
    int nOutPorts;
    int i;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <plugin lib> <plugin label> [sample rate optional : default 48000]\n", argv[0]);
        exit(1);
    }
    if (argc > 3) {
        sampleRate = atoi(argv[3]);
    }
    printf("sample rate: %d\n", sampleRate);

    libHandle = dlopen(argv[1], RTLD_NOW);
    if (!libHandle) {
        fprintf(stderr, "couldn't open lib\n");
        exit(1);
    }
    printf("libHandle %p\n", libHandle);

    descFunc = (LADSPA_Descriptor_Function)dlsym(libHandle, "ladspa_descriptor");
    if (!descFunc) {
        fprintf(stderr, "couldn't get desc function\n");
        dlclose(libHandle);
        exit(1);
    }
    printf("descFunc %p\n", descFunc);

    for (i = 0;  ;  i++) {
        pdesc = descFunc(i);
        if (!pdesc) {
            fprintf(stderr, "couldn't find descriptor\n");
            dlclose(libHandle);
            exit(1);
        }
        if (str_begins_with(argv[2], pdesc->Label)) {
            // found it
            printf("%s\n", pdesc->Name);
            break;
        }
    }

    instanceHandle = pdesc->instantiate(pdesc, sampleRate);
    if (!instanceHandle) {
        fprintf(stderr, "wtf instanceHandle == NULL\n");
        exit(1);
    }
    printf("instanceHandle %p\n", instanceHandle);

#if 0
    printf("activate %p\n", pdesc->activate);
    if (pdesc->activate) {
        pdesc->activate(instanceHandle);
        printf("activated\n");
    }
#endif

    // first just count ports and inputs
    nInBuffers = nOutBuffers = nInPorts = nOutPorts = 0;

        for (i = 0;  i < pdesc->PortCount;  i++) {
            LADSPA_PortDescriptor pd;

            pd = pdesc->PortDescriptors[i];
            if (LADSPA_IS_PORT_AUDIO(pd)) {
                if (LADSPA_IS_PORT_INPUT(pd)) {
                    //e->inChannelToPort[nInBuffers] = i;
                    nInBuffers++;
                    if (nInBuffers == 1) {  // left
                        printf("inbuffer 1\n");
                        pdesc->connect_port(instanceHandle, i, linbuffer);
                        printf("connected\n");
                    } else if (nInBuffers == 2) {  // right
                        printf("inbuffer 2\n");
                        pdesc->connect_port(instanceHandle, i, rinbuffer);
                        printf("connected\n");
                    } else {
                        fprintf(stderr, "FIXME warning plugin with %d input buffers\n", nInBuffers);
                        pdesc->connect_port(instanceHandle, i, dummydata);
                        printf("connected\n");
                    }
                } else if (LADSPA_IS_PORT_OUTPUT(pd)) {
                    //e->outChannelToPort[nOutBuffers] = i;
                    nOutBuffers++;
                    if (nOutBuffers == 1) {  // left
                        printf("outbuffer 1\n");
                        pdesc->connect_port(instanceHandle, i, loutbuffer);
                        printf("connected\n");
                    } else if (nOutBuffers == 2) {  // right
                        printf("outbuffer 2\n");
                        pdesc->connect_port(instanceHandle, i, routbuffer);
                        printf("connected\n");
                    } else {
                        fprintf(stderr, "FIXME warning plugin with %d output buffers\n", nOutBuffers);
                        pdesc->connect_port(instanceHandle, i, dummyoutdata);
                        printf("connected\n");
                    }
                }
            } else if (LADSPA_IS_PORT_CONTROL(pd)) {
                if (LADSPA_IS_PORT_INPUT(pd)) {
                    //get_ladspa_default_value_bounds_and_type(pdesc, i, &e->iDefaultVal[nInPorts], &e->imin[nInPorts], &e->imax[nInPorts], &e->itoggled[nInPorts], &e->iIsLog[nInPorts], &e->iIsInt[nInPorts]);
                    //e->ifval[nInPorts] = e->iDefaultVal[nInPorts];
                    //pdesc->connect_port(instanceHandle, i, &e->ifval[nInPorts]);
                    printf("inport %d\n", i);
                    pdesc->connect_port(instanceHandle, i, &ival[nInPorts]);
                    printf("connected\n");
                    //e->inputToPort[nInPorts] = i;
                    nInPorts++;

                } else if (LADSPA_IS_PORT_OUTPUT(pd)) {
                    printf("outport %d\n", i);
                    //pdesc->connect_port(instanceHandle, i, &e->ofval[nOutPorts]);
                    pdesc->connect_port(instanceHandle, i, &oval[nOutPorts]);
                    printf("connected\n");
                    //e->outputToPort[nOutPorts] = i;
                    nOutPorts++;
                }
            }
        }

#if 1
    printf("activate %p\n", pdesc->activate);
    if (pdesc->activate) {
        pdesc->activate(instanceHandle);
        printf("activated\n");
    }
#endif

    printf("all done.\n");

    return 0;
}
