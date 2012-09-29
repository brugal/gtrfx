#include "builtin-effect.h"
#include "common.h"
#include <math.h>

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float *threshold;
    float *slope;
    float *top;

    float count;  // num of samples since clipping began
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

    ins->count = 0;
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

    if (num >= 3) {
        xerror();
    }

    if (num == 0) {
        ins->threshold = (float *)f;
    } else if (num == 1) {
        ins->slope = (float *)f;
    } else {
        ins->top = (float *)f;
    }
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    float input;
    float x;

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
    if (!ins->top) {
        xerror();
    }

    for (i = 0;  i < sampleCount;  i++) {
        input = ins->input_buffer_left[i];
        if (*ins->top  &&  input <= 0.0f) {
            ins->output_buffer_left[i] = input;
            continue;
        } else if (!*ins->top  &&  input >= 0.0f) {
            ins->output_buffer_left[i] = input;
            continue;
        }

        if (input < *ins->threshold  &&  input > -(*ins->threshold)) {
            // not clipping anymore
            ins->output_buffer_left[i] = input;
            ins->count = 0;
            continue;
        }

        // we are clipping
        if (input >= 0.0f) {
            x = *ins->threshold;
        } else {
            x = -(*ins->threshold);
        }

        //FIXME saw wave would break this -- alwasy at a max/min
        // do it
        if (input >= 0.0f) {
            x -= ins->count * *ins->slope;
            if (x <= 0.0f)
                x = 0.0f;
        } else {
            x += ins->count * *ins->slope;
            if (x >= 0.0f)
                x = 0.0f;
        }

        ins->output_buffer_left[i] = x;
        ins->count++;
    }
}

static char *inputNames[] = { "threshold", "slope", "top" };
static int inputType[] = { INPUT_FLOAT, INPUT_FLOAT, INPUT_BOOL };
static int iHasMin[] = { TRUE, FALSE, TRUE };
static float iMin[] = { -600.0f, -500.0f, 0.0f };
static int iHasMax[] = { TRUE, FALSE, TRUE };
static float iMax[] = { 600.0f, 500.0f, 1.0f };
static float iDefault[] = { 0.7f, 0.0004f, 1.0f };
static int iIsLog[] = { TRUE, FALSE, FALSE };  //FIXME ??
static int iUsesSampleRate[] = { FALSE, FALSE, FALSE };

void be_falling_clip_init (builtin_effect_t *be)
{
    be->label = "fallingclip";
    be->description = "hard clipping that decays with a slope, similar to valve distortion, clips only the top or bottom";
    be->nInputs = 3;
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

    be->nInputBuffers = 1;
    be->nOutputBuffers = 1;

    be->instantiate = instantiate;
    be->cleanup = cleanup;
    be->activate = activate;
    be->deactivate = deactivate;
    be->connect_input_buffer = connect_input_buffer;
    be->connect_output_buffer = connect_output_buffer;
    be->connect_input = connect_input;
    be->run = run;
}
