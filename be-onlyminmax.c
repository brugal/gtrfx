#include "builtin-effect.h"
#include "common.h"
#include <math.h>

static char *label = "onlyminmax";
static char *description = "filter out all values that aren't mins or maxes of the sound wave";

static int nInputs = 0;
static int inputType[] = { };
static char *inputNames[] = { };
static int iHasMin[] = { };
static int iHasMax[] = { };
static float iDefault[] = { };
static int iIsLog[] = { };
static int iUsesSampleRate[] = { };

static int nOutputs = 0;
static int outputType[] = { };
static char *outputNames[] = { };


static int nInputBuffers = 2;
static int nOutputBuffers = 2;


typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float llast;
    float rlast;
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
    //int dir;
    float us, prev, next;

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

        us = ins->input_buffer_left[0];
        prev = ins->llast;  //ins->input_buffer_left[i - 1];
        next = ins->input_buffer_left[1];

        if ((us == prev  ||  us == next)  ||
            (us > prev  &&  us > next)  ||
            (us < prev  &&  us < next)  )
        {
            ins->output_buffer_left[0] = us;
        } else {
            ins->output_buffer_left[0] = 0.0f;
        }

    //ins->output_buffer_left[0] = ins->input_buffer_left[0];

    for (i = 1;  i < sampleCount - 1;  i++) {


        us = ins->input_buffer_left[i];
        prev = ins->input_buffer_left[i - 1];
        next = ins->input_buffer_left[i + 1];

        if ((us == prev  ||  us == next)  ||
            (us > prev  &&  us > next)  ||
            (us < prev  &&  us < next)  )
        {
            ins->output_buffer_left[i] = us;
            //ins->output_buffer_left[i - 1] = prev;
            //ins->output_buffer_left[i + 1] = next;
        } else {
            ins->output_buffer_left[i] = 0.0f;
        }

    }

    //ins->output_buffer_left[sampleCount - 1] = ins->input_buffer_left[sampleCount - 1];
    ins->llast = ins->input_buffer_left[sampleCount - 1];

}


void be_onlyminmax_init (builtin_effect_t *be)
{
    be->label = label;
    be->description = description;

    be->nInputs = nInputs;
    be->inputType = inputType;
    be->inputNames = inputNames;
    be->iHasMin = iHasMin;
    be->iHasMax = iHasMax;
    be->iDefault = iDefault;
    be->iIsLog = iIsLog;
    be->iUsesSampleRate = iUsesSampleRate;

    be->nOutputs = nOutputs;
    be->outputType = outputType;
    be->outputNames = outputNames;

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
