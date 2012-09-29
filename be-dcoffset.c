#include "builtin-effect.h"
#include "common.h"
#include <math.h>

#define DIR_STRAIGHT 0
#define DIR_UP 1
#define DIR_DOWN 2

static char *label = "dcoffset";
static char *description = "dc offset correction using average of last couple of samples";

static char *inputNames[] = { "fixed offset" };
static int inputType[] = { INPUT_FLOAT };
static int iHasMin[] = { FALSE };
static int iHasMax[] = { FALSE };
static float iDefault[] = { 0.0f };
static int iIsLog[] = { FALSE };
static int iUsesSampleRate[] = { FALSE };
static int nInputs = 1;

static int nOutputs = 0;

static int nInputBuffers = 1;
static int nOutputBuffers = 1;

//FIXME alllow less than for nInputBuffers

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *output_buffer_left;
    float *fixedOffset;
    float total;
    float avg;
    unsigned int samples;

    // testing
    float ftotal;
    float favg;
    unsigned int fsamples;
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
    ins->total = 0.0f;
    ins->samples = 0;
    ins->avg = 0.0f;
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
    if (num > 1) {
        xerror();
    }

    if (num == 0) {
        ins->input_buffer_left = buffer;
    } else {
        //ins->input_buffer_right = buffer;
        xerror();
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
    if (num > 1) {
        xerror();
    }

    if (num == 0) {
        ins->output_buffer_left = buffer;
    } else {
        //ins->output_buffer_right = buffer;
        xerror();
    }
}

static void connect_input (void *instance, int num, void *f)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!f) {
        xerror();
    }
    if (num > nInputs - 1) {
        xerror();
    }

    ins->fixedOffset = f;
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;

    if (!ins) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->fixedOffset) {
        xerror();
    }

    for (i = 0;  i < sampleCount;  i++) {
        //FIXME input var
        if (ins->samples < 100000) {
            ins->total += ins->input_buffer_left[i];
            ins->samples++;
            ins->avg = ins->total / (float)ins->samples;
            if (ins->samples % 5000 == 0) {
                //printf("dc offset %f\n", ins->avg);
            }
        } else {
            ins->total = 0.0f;
            ins->samples = 0;
        }

        ins->output_buffer_left[i] = ins->input_buffer_left[i];

        if (*ins->fixedOffset != 0) {
            ins->output_buffer_left[i] += *ins->fixedOffset;
        } else {
            ins->output_buffer_left[i] -= ins->avg;
        }

        if (ins->fsamples < 10000) {
            ins->ftotal += ins->output_buffer_left[i];
            ins->fsamples++;
            ins->favg = ins->ftotal / (float)ins->fsamples;
            if (ins->fsamples % 5000 == 0) {
                //printf("fixed == %f\n", ins->favg);
            }
        } else {
            ins->ftotal = 0.0f;
            ins->fsamples = 0;
        }
    }

    //memcpy(ins->output_buffer_left, ins->input_buffer_left, sampleCount * sizeof(float));
}


void be_dcoffset_init (builtin_effect_t *be)
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
