#include "builtin-effect.h"
#include "common.h"
#include <math.h>

typedef struct gain_s gain_t;

struct gain_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float *gain;
};

static void *instantiate (unsigned long sampleRate)
{
    void *r;

    r = calloc(1, sizeof(gain_t));
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
    gain_t *ins = (gain_t *)instance;

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
    gain_t *ins = (gain_t *)instance;

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
    gain_t *ins = (gain_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!f) {
        xerror();
    }
    //    if (num >= instance->nInputs) {
    //    xerror();
    //}

    ins->gain = (float *)f;
}

static void run (void *instance, unsigned long sampleCount)
{
    gain_t *ins = (gain_t *)instance;
    float gainVal;
    int i;

    if (!ins) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->gain) {
        xerror();
    }

    if (*ins->gain == 0.0f) {
        gainVal = 1.0f;
    }

    gainVal = powf(10, *ins->gain / 20.0f);

    for (i = 0;  i < sampleCount;  i++) {
        ins->output_buffer_left[i] = ins->input_buffer_left[i] * gainVal;
    }

    if (ins->input_buffer_right) {
        if (!ins->output_buffer_right) {
            xerror();
        }
        for (i = 0;  i < sampleCount;  i++) {
            ins->output_buffer_right[i] = ins->input_buffer_right[i] * gainVal;
        }
    }
}

static char *inputNames[] = { "dB" };
static int inputType[] = { INPUT_FLOAT };
static int iHasMin[] = { TRUE };
static float iMin[] = { -3000.0f };
static int iHasMax[] = { TRUE };
static float iMax[] = { 3000.0f };
static float iDefault[] = { 0.0f };
static int iIsLog[] = { TRUE, };
static int iUsesSampleRate[] = { FALSE };

void be_gain_init (builtin_effect_t *be)
{
    be->label = "gain";
    be->description = "simple gain without clipping";
    be->nInputs = 1;
    be->inputNames = inputNames;
    be->inputType = inputType;
    be->iHasMin = iHasMin;
    be->iMin = iMin;
    be->iHasMax = iHasMax;
    be->iMax = iMax;
    be->iDefault = iDefault;
    be->iIsLog = iIsLog;
    be->iUsesSampleRate = iUsesSampleRate;

    be->nOutputs = 0;

    be->nInputBuffers = 2;
    be->nOutputBuffers = 2;

    be->instantiate = instantiate;
    be->cleanup = cleanup;
    be->activate = activate;
    be->deactivate = deactivate;
    be->connect_input_buffer = connect_input_buffer;
    be->connect_output_buffer = connect_output_buffer;
    be->connect_input = connect_input;
    be->run = run;
}
