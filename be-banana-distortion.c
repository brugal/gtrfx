#include "builtin-effect.h"
#include "common.h"
#include <math.h>

#define DIR_STRAIGHT 0
#define DIR_UP 1
#define DIR_DOWN 2

static char *label = "banana-distortion";
static char *description = "distortion using an exponential function when rising and logarithmic when amplitude is falling";

static char *inputNames[] = { "rising base", "falling base" };
static int inputType[] = { INPUT_FLOAT, INPUT_FLOAT };
static int iHasMin[] = { FALSE, FALSE };
static int iHasMax[] = { FALSE, FALSE };
static float iDefault[] = { 2.14f, 2.14f };
static int iIsLog[] = { FALSE, FALSE };  //FIXME true?
static int iUsesSampleRate[] = { FALSE, FALSE };
static int nInputs = 2;

static int nOutputs = 0;

static int nInputBuffers = 1;
static int nOutputBuffers = 1;

//FIXME alllow less than for nInputBuffers

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    //float *input_buffer_right;
    float *output_buffer_left;
    //float *output_buffer_right;
    float llast;
    //int llastDir;
    int last_dir;
    //float rlast;
    //int rlastDir;
    float *rising_base;
    float *falling_base;

    //unsigned long int total;
    //unsigned long int sampleCount;
    //unsigned long int runCount;
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
    ins->last_dir = DIR_STRAIGHT;
    //ins->rlast = 0.0f;
    //ins->rising_base = iDefault[0];
    //ins->falling_base = iDefault[1];
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

    if (num == 0) {
        ins->rising_base = (float *)f;
    } else {
        ins->falling_base = (float *)f;
    }
}

static float rising_func (float base, float f)
{
    float r;

    //r = powf(base, f);
    //FIXME error return values
    //r = sqrtf(1.0 + f);
    //r -= 1.0f;

    r = powf(f + 1.0, base);
    r -= 1.0f;
    //r = logf(1.0f + f);
    //r -= 1.0f;

    //r = f;

    return r;
}

static float falling_func (float base, float f)
{
    float r;

    //r = powf(base, f);
    //FIXME error return values

    //if 

    //r = f;
#if 0
    r = powf(f, base);
    if (f < 0.0f) {
        r = -fabsf(r);
    } else {
        r = fabsf(r);
    }
#endif
    r = powf(f + 1.0, base);
    r -= 1.0f;

    //r = f;

    return r;
}

#if 0
static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    //int dir;
    //int currMinMax;
    //int j;
    //float slope;
    float us, prev;  // next;
    //int numMinMax;

    if (!ins) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    //if (!ins->input_buffer_right) {
    //    xerror();
    //}
    if (!ins->output_buffer_left) {
        xerror();
    }
    //if (!ins->output_buffer_right) {
    //    xerror();
    //}
    if (!ins->rising_base) {
        xerror();
    }
    if (!ins->falling_base) {
        xerror();
    }


    for (i = 0;  i < sampleCount;  i++) {
        us = ins->input_buffer_left[i];
        if (i == 0) {
            prev = ins->llast;
        } else {
            prev = ins->input_buffer_left[i - 1];
        }

        //FIXME not sure whether to use current function (us == prev)
        if (us > prev) {
            ins->output_buffer_left[i] = rising_func(*ins->rising_base, ins->input_buffer_left[i]);
            ins->last_dir = DIR_UP;
        } else if (us < prev) {
            ins->output_buffer_left[i] = rising_func(*ins->rising_base, ins->input_buffer_left[i]);
            ins->output_buffer_left[i] = falling_func(*ins->falling_base, ins->output_buffer_left[i]);
            ins->last_dir = DIR_DOWN;
        } else {
            if (ins->last_dir == DIR_UP) {
                ins->output_buffer_left[i] = rising_func(*ins->rising_base, ins->input_buffer_left[i]);
            } else {
                ins->output_buffer_left[i] = rising_func(*ins->rising_base, ins->input_buffer_left[i]);
                ins->output_buffer_left[i] = falling_func(*ins->falling_base, ins->output_buffer_left[i]);
            }
        }
    }

    ins->llast = ins->input_buffer_left[sampleCount - 1];
}
#endif

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    //int dir;
    //int currMinMax;
    //int j;
    //float slope;
    float us, prev;  // next;
    //int numMinMax;

    if (!ins) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    //if (!ins->input_buffer_right) {
    //    xerror();
    //}
    if (!ins->output_buffer_left) {
        xerror();
    }
    //if (!ins->output_buffer_right) {
    //    xerror();
    //}
    if (!ins->rising_base) {
        xerror();
    }
    if (!ins->falling_base) {
        xerror();
    }


    for (i = 0;  i < sampleCount;  i++) {
        us = ins->input_buffer_left[i];
        if (i == 0) {
            prev = ins->llast;
        } else {
            prev = ins->input_buffer_left[i - 1];
        }

        if (us > prev) {
            ins->last_dir = DIR_UP;
            if (us < -0.5) {
                us = -0.5;
            } else if (us < -0.25) {
                //us = -0.25;
            }
        } else if (us < prev) {
            ins->last_dir = DIR_DOWN;
        } else {

        }

        ins->output_buffer_left[i] = us;
    }
}


void be_banana_distortion_init (builtin_effect_t *be)
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
