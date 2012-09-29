#include "builtin-effect.h"
#include "common.h"
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

static char *label = "clipdetect";
static char *description = "clip detection";

static int nInputs = 2;
static char *inputNames[] = { "threshold", "tag" };
static int inputType[] = { INPUT_FLOAT, INPUT_STRING };
//static char *inputDescriptions[] = { "", "" };
static int iHasMin[] = { TRUE, FALSE };
static float iMin[] = { -1.0f, 0.0f };
static int iHasMax[] = { TRUE, FALSE };
static float iMax[] = { 1.0f, 0.0f };
static float iDefault[] = { 1.0f };
static int iIsLog[] = { FALSE, FALSE };  //FIXME ??
static int iUsesSampleRate[] = { FALSE, FALSE };

static int nOutputs = 0;

static int nInputBuffers = 2;
static int nOutputBuffers = 2;

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float *threshold;
    char *tag;
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

static void connect_input (void *instance, int num, void *input)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!input) {
        xerror();
    }
    if (num >= nInputs) {
        xerror();
    }

    if (num == 0) {
        ins->threshold = (float *)input;
    } else {
        ins->tag = (char *)input;
        //strncpy(ins->tag, (const char *)input, MAX_CHARS);
    }
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    static time_t sec;  //FIXME

    if (!ins) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->threshold) {
        xerror();
    }

    memcpy(ins->output_buffer_left, ins->input_buffer_left, sampleCount * sizeof(float));

    for (i = 0;  i < sampleCount;  i++) {
        if (ins->input_buffer_left[i] > *ins->threshold  ||
            ins->input_buffer_left[i] < -(*ins->threshold)
            ) {
            if (time(NULL) - sec > 1) {
                //FIXME don't print too much
                //printf("[%s] clip %f\n", tag, data[i]);
                printf("[%s] clip %f\n", ins->tag, ins->input_buffer_left[i]);
                sec = time(NULL);
            }
        }
    }

    if (ins->input_buffer_right) {
        if (!ins->output_buffer_right) {
            xerror();
        }
        memcpy(ins->output_buffer_right, ins->input_buffer_right, sampleCount * sizeof(float));
    }
}


void be_clip_detect_init (builtin_effect_t *be)
{
    be->label = label;
    be->description = description;
    be->nInputs = nInputs;
    be->inputNames = inputNames;
    be->inputType = inputType;
    be->iHasMin = iHasMin;
    be->iMin = iMin;
    be->iHasMax = iHasMax;
    be->iMax = iMax;
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
