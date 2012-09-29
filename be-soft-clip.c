#include "builtin-effect.h"
#include "common.h"
#include <math.h>

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float *type;  //FIXME type int?
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

static void connect_input (void *instance, int num, void *f)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!f) {
        xerror();
    }
    ins->type = (float *)f;
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
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
    if (!ins->type) {
        xerror();
    }

#if 1
    for (i = 0;  i < sampleCount;  i++) {
        if (*ins->type >= 1.0f) {
            ins->output_buffer_left[i] = ins->input_buffer_left[i] / (1.0f + fabsf(ins->input_buffer_left[i]));
        } else {
            if (ins->input_buffer_left[i] <= -1.0f) {
                ins->output_buffer_left[i] = -(2.0f / 3.0f);
            } else if (ins->input_buffer_left[i] >= 1.0f) {
                ins->output_buffer_left[i] = (2.0f / 3.0f);
            } else {
                ins->output_buffer_left[i] = ins->input_buffer_left[i] - (ins->input_buffer_left[i] * ins->input_buffer_left[i] * ins->input_buffer_left[i]) / 3.0f;
            }
        }
    }
#endif

    if (ins->input_buffer_right) {
        if (!ins->output_buffer_right) {
            xerror();
        }
#if 1
        for (i = 0;  i < sampleCount;  i++) {
            if (*ins->type >= 1.0f) {
                ins->output_buffer_right[i] = ins->input_buffer_right[i] / (1.0f + fabsf(ins->input_buffer_right[i]));
            } else {
                if (ins->input_buffer_right[i] <= -1.0f) {
                    ins->output_buffer_right[i] = -(2.0f / 3.0f);
                } else if (ins->input_buffer_right[i] >= 1.0f) {
                    ins->output_buffer_right[i] = (2.0f / 3.0f);
                } else {
                    ins->output_buffer_right[i] = ins->input_buffer_right[i] - (ins->input_buffer_right[i] * ins->input_buffer_right[i] * ins->input_buffer_right[i]) / 3.0f;
                }
            }
        }
#endif
    }
}

static char *inputNames[] = { "type" };
static int inputType[] = { INPUT_FLOAT };
static int iHasMin[] = { TRUE };
static float iMin[] = { -600.0f };
static int iHasMax[] = { TRUE };
static float iMax[] = { 600.0f };
static float iDefault[] = { 1.0f };
static int iIsLog[] = { TRUE, };  //FIXME ??
static int iUsesSampleRate[] = { FALSE };

void be_soft_clip_init (builtin_effect_t *be)
{
    be->label = "softclip";
    be->description = "soft clipping";
    be->nInputs = 1;
    be->inputNames = inputNames;
    //be->sinput_names
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
