#include "builtin-effect.h"
#include "common.h"
#include "common-string.h"
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

static char *inputNames[] = { "filename" };
static int nInputs = 1;
static int nOutputs = 0;
//static char *sinput_names[] = { "tag" };
static int inputType[] = { INPUT_FILE };
static int iHasMin[] = { FALSE };
static int iHasMax[] = { FALSE };
static float iDefault[] = { 1.0f };
static int iIsLog[] = { TRUE, };  //FIXME ??
static int iUsesSampleRate[] = { FALSE };

static int nInputBuffers = 2;
static int nOutputBuffers = 2;

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    char *filename;
    char currentFilename[MAX_CHARS];
    unsigned long sampleRate;

    FILE *f;
};

static void *instantiate (unsigned long sampleRate)
{
    data_t *r;

    r = calloc(1, sizeof(data_t));
    if (!r) {
        xerror();
    }
    r->sampleRate = sampleRate;

    return r;
}

static void cleanup (void *instance)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }

    if (ins->f) {
        fclose(ins->f);
    }
    free(ins);
}

static void activate (void *instance)
{
    data_t *ins = (data_t *)instance;
    char buffer[MAX_CHARS];

    if (!ins) {
        xerror();
    }
    if (!ins->filename) {
        //xerror();
        return;
    }
    if (!*ins->filename) {
        return;
    }

    strncpy(ins->currentFilename, ins->filename, MAX_CHARS);
    ins->f = fopen(ins->filename, "r");
    if (!ins->f) {
        printf("warning couldn't open file '%s'\n", ins->filename);
        return;
    }

    //FIXME read wav header, assuming 16 bit stereo 44100
    fread(buffer, 44, 1, ins->f);
}

static void deactivate (void *instance)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }

    if (ins->f) {
        fclose(ins->f);
        ins->f = NULL;
    }
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

    ins->filename = (char *)input;
}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    short int buffer[MAX_CHARS * 4];  //FIXME
    //unsigned long sz;
    size_t r;

    if (!ins) {
        xerror();
    }
    if (!ins->output_buffer_left) {
        xerror();
    }
    if (!ins->output_buffer_right) {
        xerror();
    }
    if (!ins->input_buffer_left) {
        xerror();
    }
    if (!ins->input_buffer_right) {
        xerror();
    }

    if (!str_matches(ins->currentFilename, ins->filename)) {
        printf("new file: %s\n", ins->filename);
        deactivate(ins);
        activate(ins);
    }

    if (!ins->f) {
        return;
    }

    r = fread(buffer, 1, sampleCount * 4, ins->f);
    if (r < 0) {
        xerror();
    }
    if (r < 4 * (int)sampleCount) {
        printf("looping again %d  %ld\n", r, sampleCount);
        fseek(ins->f, 0, SEEK_SET);
        //FIXME use the data we got
        return;
    }

    for (i = 0;  i < sampleCount;  i++) {
        ins->output_buffer_left[i] = ((float)(buffer[i * 2]) * (1.0f / 32767.5f));
        ins->output_buffer_right[i] = ((float)(buffer[i * 2 + 1]) * (1.0f / 32767.5f));
    }

#if 0
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
#endif
}


void be_wav_file_input_init (builtin_effect_t *be)
{
    be->label = "wavfilein";
    be->description = "get data from wav file";
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
