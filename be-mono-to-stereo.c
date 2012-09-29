#include "builtin-effect.h"
#include "common.h"
#include <math.h>
#include <string.h>

typedef struct data_s data_t;

struct data_s {
    float *input_buffer;
    float *output_buffer_left;
    float *output_buffer_right;
    //float *gain;
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

    ins->input_buffer = buffer;
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

#if 0
static void connect_input (void *instance, int num, float *f)
{
    //data_t *ins = (data_t *)instance;

#if 0
    if (!ins) {
        xerror();
    }
    if (!f) {
        xerror();
    }
    ins->gain = f;
#endif
}
#endif

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }

    if (!ins->output_buffer_left) {
        xerror();
    }

    if (!ins->output_buffer_right) {
        xerror();
    }

    if (!ins->input_buffer) {
        xerror();
    }

    memcpy(ins->output_buffer_left, ins->input_buffer, sampleCount * sizeof(float));
    memcpy(ins->output_buffer_right, ins->input_buffer, sampleCount * sizeof(float));
}

//static char *inputNames[] =  { "" };  //{ "threshold" };
//static int iHasMin[] = { FALSE };
//static int iHasMax[] = { FALSE };
//static float iDefault[] = { 0.0f };
//static int iIsLog[] = { TRUE, };
//static int iUsesSampleRate[] = { FALSE };

void be_mono_to_stereo_init (builtin_effect_t *be)
{
    be->label = "monotostereo";
    be->description = "convert mono channel (left) to stereo";
    be->nInputs = 0;
    //be->finput_names = finput_names;
#if 0
    be->iHasMin = iHasMin;
    be->iHasMax = iHasMax;
    be->iDefault = iDefault;
    be->iIsLog = iIsLog;
    be->iUsesSampleRate = iUsesSampleRate;
#endif

    be->nOutputs = 0;

    be->nInputBuffers = 1;
    be->nOutputBuffers = 2;

    be->instantiate = instantiate;
    be->cleanup = cleanup;
    be->activate = activate;
    be->deactivate = deactivate;
    be->connect_input_buffer = connect_input_buffer;
    be->connect_output_buffer = connect_output_buffer;
    //be->connect_finput = connect_finput;
    be->run = run;
}
