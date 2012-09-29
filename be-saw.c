#include "builtin-effect.h"
#include "common.h"
#include <math.h>

#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_STRAIGHT 2

static char *label = "saw";
static char *description = "create saw tooth from input mins and maxs";
static char *inputNames[] = { };
static int inputType[] = { };
static int iHasMin[] = { };
static int iHasMax[] = { };
static float iDefault[] = { };
static int iIsLog[] = { };
static int iUsesSampleRate[] = { };
static int nInputs = 0;
static int nOutputs = 0;
static int nInputBuffers = 2;
static int nOutputBuffers = 2;

//FIXME alllow less than for nInputBuffers

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float llast;
    int llastDir;
    float rlast;
    int rlastDir;

    unsigned long int total;
    unsigned long int sampleCount;
    unsigned long int runCount;
};

static void *instantiate (unsigned long sampleRate)
{
    void *r;

    r = calloc(1, sizeof(data_t));
    if (!r) {
        xerror();
    }

    return r;
}

static void cleanup (void *instance)
{
    if (!instance) {
        xerror();
    }

    free(instance);
}

static void activate (void *instance)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }

    ins->llast = 0.0f;
    ins->rlast = 0.0f;
}

static void deactivate (void *instance)
{
}

static void connect_input_buffer (void *instance, int num, float *buffer)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!buffer) {
        xerror();
    }
    if (num > 2) {
        xerror();
    }

    if (num == 0) {
        ins->input_buffer_left = buffer;
    } else {
        ins->input_buffer_right = buffer;
    }
}

static void connect_output_buffer (void *instance, int num, float *buffer)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!buffer) {
        xerror();
    }
    if (num > 2) {
        xerror();
    }

    if (num == 0) {
        ins->output_buffer_left = buffer;
    } else {
        ins->output_buffer_right = buffer;
    }
}

static void connect_input (void *instance, int num, void *f)
{
#if 0
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!f) {
        xerror();
    }
    ins->threshold = (float *)f;
#endif
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    int dir;
    int currMinMax;
    int j;
    float slope;
    float us, prev, next;
    int numMinMax;

    if (!ins) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->input_buffer_right) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->output_buffer_right) {
        xerror();
    }

    ins->sampleCount += sampleCount;

    if (ins->llast == ins->input_buffer_left[0]) {
        dir = DIR_STRAIGHT;
    } else if (ins->llast > ins->input_buffer_left[0]) {
        dir = DIR_DOWN;
    } else {
        dir = DIR_UP;
    }

    currMinMax = 0;
    numMinMax = 1;

    ins->output_buffer_left[0] = ins->input_buffer_left[0];

#if 1
    for (i = 1;  i < sampleCount - 1;  i++) {

        us = ins->input_buffer_left[i];
        prev = ins->input_buffer_left[i - 1];
        next = ins->input_buffer_left[i + 1];

        //if ( (us == prev  ||  us == next)  ||
        if ( (us == next  ||  us == prev)  ||
             (us > prev  &&  us > next)  ||
             (us < prev  &&  us < next)
           )
        {

            if (us == prev) {
                ins->output_buffer_left[i] = ins->input_buffer_left[i];
                continue;
            }

            // min, max
            //FIXME connect line to prev min
            slope = (ins->input_buffer_left[i] - ins->input_buffer_left[currMinMax]) / (float)(i - currMinMax);
            //printf("%f [%d]  -->  %f [%d]  slope: %f\n", ins->input_buffer_left[i], i, ins->input_buffer_left[currMinMax], currMinMax, slope);

#if 0
            for (j = i;  j > currMinMax;  j--) {
                ins->output_buffer_left[j] = ins->input_buffer_left[i] - (slope * (i - j));
                //printf("%f\n", ins->output_buffer_left[j]);
            }
#endif

            for (j = currMinMax;  j < (currMinMax + (i - currMinMax) / 2);  j++) {
                ins->output_buffer_left[j] = ins->input_buffer_left[currMinMax];
            }
            for (j = currMinMax + (i - currMinMax) / 2;  j <= i;  j++) {
                ins->output_buffer_left[j] = ins->input_buffer_left[i];
            }
            //printf("----\n");
            currMinMax = i;
            numMinMax++;
        } else {
            ins->output_buffer_left[i] = 0.0f;
        }
    }
#endif

    //ins->output_buffer_left[sampleCount - 1] = ins->input_buffer_left[sampleCount - 1];
    //FIXME connect to last min
    slope = (ins->input_buffer_left[sampleCount - 1] - ins->input_buffer_left[currMinMax]) / (float)((sampleCount - 1) - currMinMax);
    for (j = sampleCount - 1;  j > currMinMax;  j--) {
        ins->output_buffer_left[j] = ins->input_buffer_left[sampleCount - 1] - (slope * ((sampleCount - 1) - j));
    }
    ins->llast = ins->input_buffer_left[sampleCount - 1];

    numMinMax++;
    ins->total += numMinMax;
    ins->runCount++;
    if (ins->runCount % 500 == 0) {
        printf("%ld  %ld  %f\n", ins->total, ins->sampleCount, (float)ins->total / (float)ins->sampleCount);
    }
}


void be_saw_init (builtin_effect_t *be)
{
    be->label = label;
    be->description = description;
    be->nInputs = nInputs;
    be->inputNames = inputNames;
    be->inputType = inputType;
    be->iHasMin = iHasMin;
    be->iHasMax = iHasMax;
    be->iDefault = iDefault;
    be->iIsLog = iIsLog;
    be->iUsesSampleRate = iUsesSampleRate;

    be->nOutputs = nOutputs;

    be->nInputBuffers = nInputBuffers;
    be->nOutputBuffers = nOutputBuffers;

    be->instantiate = instantiate;
    be->cleanup = cleanup;
    be->activate = activate;
    be->deactivate = deactivate;
    be->connect_input_buffer = connect_input_buffer;
    be->connect_output_buffer = connect_output_buffer;
    be->connect_input = connect_input;
    be->run = run;
}
