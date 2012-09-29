#include "builtin-effect.h"
#include "common.h"
#include <math.h>

/* trying to fix bugs in swh-plugin valve plugin */

static char *label = "valvedistortion";
static char *description = "valve distortion";

static char *inputNames[] = { "level", "character" };
static int inputType[] = { INPUT_FLOAT, INPUT_FLOAT };
static int iHasMin[] = { FALSE, FALSE };  //FIXME
static int iHasMax[] = { FALSE, FALSE };  //FIXME
static float iDefault[] = { 1.0f, 1.0f };
static int iIsLog[] = { FALSE, FALSE };
static int iUsesSampleRate[] = { FALSE, FALSE };
static int nInputs = 2;

static int nOutputs = 0;

static int nInputBuffers = 1;
static int nOutputBuffers = 1;

//FIXME alllow less than for nInputBuffers

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *output_buffer_left;
    float *level;
    float *character;

    float otm1;
    float itm1;
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

    ins->itm1 = 1.0f;
    ins->otm1 = 1.0f;
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
    if (num >= nInputBuffers) {
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
    if (num >= nOutputBuffers) {
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
    if (num >= nInputs) {
        xerror();
    }

    if (num == 0) {
        ins->level = f;
    } else {
        ins->character = f;
    }
}

static inline void round_to_zero(volatile float *f)
{
    *f += 1e-18;
    *f -= 1e-18;
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    float q, dist, fx;
    float input;

    if (!ins) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->level) {
        xerror();
    }
    if (!ins->character) {
        xerror();
    }

    q = *ins->level - 0.999f;
    dist = *ins->character * 40.0f + 0.1f;

    if (q == 0.0f) {
        for (i = 0;  i < sampleCount;  i++) {
            //int positive = FALSE;

            input = ins->input_buffer_left[i];
#if 0
            if (input > 0.0f) {
                positive = TRUE;
                input *= -1.0f;
            }
#endif
            if (input == q) {
                fx = 1.0f / dist;
            } else {
                fx = input / (1.0f - expf(-dist * input));
            }
            ins->otm1 = 0.999f * ins->otm1 + fx - ins->itm1;
            round_to_zero(&ins->otm1);
            ins->itm1 = fx;
            ins->output_buffer_left[i] = ins->otm1;
#if 0
            if (positive) {
                ins->output_buffer_left[i] *= -1.0f;
            }
#endif
        }
    } else {
        for (i = 0;  i < sampleCount;  i++) {
            //int positive = FALSE;

            input = ins->input_buffer_left[i];
#if 0
            if (input > 0.0f) {
                positive = TRUE;
                input *= -1.0f;
            }
#endif

            if (input == q) {
                fx = 1.0f / dist + q / (1.0f - expf(dist * q));
            } else {
                fx = (input - q) / (1.0f - expf(-dist * (input - q))) + q / (1.0f - expf(dist * q));
            }
            ins->otm1 = 0.999f * ins->otm1 + fx - ins->itm1;
            round_to_zero(&ins->otm1);
            ins->itm1 = fx;
            ins->output_buffer_left[i] = ins->otm1;
#if 0
            if (positive) {
                ins->output_buffer_left[i] *= -1.0f;
            }
#endif
        }
    }

    // plugin dies, doing some checks
#define CLIP_VAL 1.0f
    if (ins->itm1 > CLIP_VAL) {
        //printf("itm1 > CLIP_VAL\n");
        ins->itm1 = CLIP_VAL;
    }
    if (ins->itm1 < -CLIP_VAL) {
        //printf("itm1 < CLIP_VAL\n");
        ins->itm1 = -CLIP_VAL;
    }

    if (ins->otm1 > CLIP_VAL) {
        //printf("otm1 > CLIP_VAL\n");
        ins->otm1 = CLIP_VAL;
    }
    if (ins->otm1 < -CLIP_VAL) {
        //printf("otm1 < CLIP_VAL\n");
        ins->otm1 = -CLIP_VAL;
    }

}

void be_valve_distortion_init (builtin_effect_t *be)
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
