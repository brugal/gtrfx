#include "builtin-effect.h"
#include "common.h"
#include <math.h>

static char *label = "hardgate";
static char *description = "ignore sound below threshold";
static char *inputNames[] = { "threshold", "delay time (ms)", "open time (ms)" };
static int inputType[] = { INPUT_FLOAT, INPUT_FLOAT, INPUT_FLOAT };
static int iHasMin[] = { TRUE, TRUE, TRUE };
static float iMin[] = { -600.0f, 0.0f, 0.0f };
static int iHasMax[] = { TRUE, FALSE, FALSE };
static float iMax[] = { 600.0f, 0.0f, 0.0f };
static float iDefault[] = { 0.0f, 0.0f, 0.0f };
static int iIsLog[] = { FALSE, FALSE, FALSE };
static int iUsesSampleRate[] = { FALSE, FALSE, FALSE };
static int nInputs = 3;
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
    float *threshold;
    float *delayTime;
    float *openTime;

    long int openCount;
    long int delayCount;
    unsigned long rate;
};

static void *instantiate (unsigned long sampleRate)
{
    void *r;

    r = calloc(1, sizeof(data_t));
    if (!r) {
        xerror();
    }

    ((data_t *)r)->rate = sampleRate;

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
        ins->threshold = (float *)f;
    } else if (num == 1) {
        ins->delayTime = (float *)f;
    } else if (num == 2) {
        ins->openTime = (float *)f;
    }
}

static unsigned long ftime_to_sample_count (data_t *ins, float f)
{
    unsigned long r;

    r = (unsigned long)(f / (1000.0 / (float)ins->rate));

    //printf("f  %f   %ld\n", f, r);

    return r;
}

static int too_weak (data_t *ins, float val)
{
    //    ib[i] < *ins->threshold  &&  ib[i] > -(*ins->threshold)
    if (val < *ins->threshold  &&  val > -(*ins->threshold))
        return TRUE;

    return FALSE;
}

static void reset_open_count (data_t *ins)
{
    ins->openCount = ftime_to_sample_count(ins, *ins->openTime);
}


static void reset_delay_count (data_t *ins)
{
    ins->delayCount = ftime_to_sample_count(ins, *ins->delayTime);
}

static void doit (data_t *ins, int bufNum, float *ib, float *ob, unsigned long sampleCount)
{
    int i;

    for (i = 0;  i < sampleCount;  i++) {
        if (ins->openCount >= 0) {
            ob[i] = ib[i];
            if (!too_weak(ins, ib[i])) {
                reset_open_count(ins);
                continue;
            }

            ins->openCount--;
            continue;
        }

#if 1
        if (ins->delayCount >= 0) {
            ob[i] = 0.0f;
            ins->delayCount--;

            if (ins->delayCount < 0) {
                // time to open the gate
                ob[i] = ib[i];
                reset_open_count(ins);
                //printf("opening gate...  %d\n", ins->startCount);
                continue;
            }

            continue;
        }
#endif

        if (!too_weak(ins, ib[i])) {
            //ob[i] = ib[i];
            ob[i] = 0.0f;
            //reset_start_count(ins);
            //ins->startCount--;
            reset_delay_count(ins);
            //printf("reseting open count %d\n", ins->openCount);
            continue;
        } else {
            ob[i] = 0.0f;
        }
    }

}

#if 0
static void doitx (data_t *ins, int bufNum, float *ib, float *ob, unsigned long sampleCount)
{
    int i;
    int pos;

    if (ins->startCount > 0) {
        // keep it open
        for (i = 0;  i < sampleCount;  i++) {
            ob[i] = ib[i];
            ins->startCount--;

            if (!(ib[i] < *ins->threshold  &&  ib[i] > -(*ins->threshold))) {
                // outside of threshold again, reset startCount
                ins->startCount = ftime_to_sample_count(ins, *ins->closeTime);
            }

            if (ins->startCount <= 0)
                break;
            //printf("gate open!  %ld\n", ins->startCount);
        }
        pos = i;
    }

    for (i = pos;  i < sampleCount;  i++) {
        if (ib[i] < *ins->threshold  &&  ib[i] > -(*ins->threshold)) {
            ob[i] = 0.0f;
        } else {
            ob[i] = ib[i];

            // gate now open
            ins->startCount = ftime_to_sample_count(ins, *ins->closeTime);
        }
    }

}
#endif

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;

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
    if (!ins->threshold) {
        xerror();
    }
    if (!ins->delayTime) {
        xerror();
    }
    if (!ins->openTime) {
        xerror();
    }

    doit(ins, 0, ins->input_buffer_left, ins->output_buffer_left, sampleCount);
    //doit(ins, 1, ins->input_buffer_right, ins->output_buffer_right, sampleCount);
}


void be_hard_gate_init (builtin_effect_t *be)
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
