#include "builtin-effect.h"
#include "common.h"
#include <math.h>

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    float *multiplier;
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
    ins->multiplier = (float *)f;
}

static void doit (float *in, float *out, unsigned long sampleCount, float mult)
{
    int i;

    for (i = 0;  i < sampleCount;  i++) {
        out[i] = (float) (atan(in[i] * mult) / atan(mult));
    }

}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->multiplier) {
        xerror();
    }

    doit(ins->input_buffer_left, ins->output_buffer_left, sampleCount, *ins->multiplier);


    if (ins->input_buffer_right) {
        if (!ins->output_buffer_right) {
            xerror();
        }
        doit(ins->input_buffer_right, ins->output_buffer_right, sampleCount, *ins->multiplier);
    }
}

static char *inputNames[] = { "multiplier" };
static int inputType[] = { INPUT_FLOAT };
static int iHasMin[] = { TRUE };
static float iMin[] = { -600.0f };
static int iHasMax[] = { TRUE };
static float iMax[] = { 600.0f };
static float iDefault[] = { 1.0f };
static int iIsLog[] = { TRUE, };  //FIXME ??
static int iUsesSampleRate[] = { FALSE };

void be_arctan_distortion_init (builtin_effect_t *be)
{
    be->label = "arctan-distortion";
    be->description = "arctan distortion";
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
