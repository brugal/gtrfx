#include "builtin-effect.h"
#include "common.h"
#include "common-string.h"
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "wav.h"

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
    char parsedFilename[MAX_CHARS];
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
    //char buffer[MAX_CHARS];
    unsigned char buf[WAV_HEADER_LEN];
    int size = 0x7fffffff; /* Use a bogus size initially */


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
    parse_filename(ins->filename, ins->parsedFilename, MAX_CHARS);
    printf("parsed filename: %s\n", ins->parsedFilename);
    ins->f = fopen(ins->parsedFilename, "w");
    if (!ins->f) {
        printf("warning couldn't open file '%s'\n", ins->parsedFilename);
        return;
    }

    //FIXME write wav header
    /* Store information */
    //internal->wave.common.wChannels = format->channels;
    //internal->wave.common.wBitsPerSample = format->bits;
    //internal->wave.common.dwSamplesPerSec = format->rate;

    memset(buf, 0, WAV_HEADER_LEN);

    /* Fill out our wav-header with some information. */
#if 0
    strncpy(internal->wave.riff.id, "RIFF",4);
    internal->wave.riff.len = size - 8;
    strncpy(internal->wave.riff.wave_id, "WAVE",4);

    strncpy(internal->wave.format.id, "fmt ",4);
    internal->wave.format.len = 16;

    internal->wave.common.wFormatTag = WAVE_FORMAT_PCM;
    internal->wave.common.dwAvgBytesPerSec = 
        internal->wave.common.wChannels * 
        internal->wave.common.dwSamplesPerSec *
        (internal->wave.common.wBitsPerSample >> 3);

    internal->wave.common.wBlockAlign = 
        internal->wave.common.wChannels * 
        (internal->wave.common.wBitsPerSample >> 3);

    strncpy(internal->wave.data.id, "data",4);

    internal->wave.data.len = size - 44;
#endif

    strncpy((char *)buf, "RIFF", 4);
    WRITE_U32(buf + 4, (size - 8));
    strncpy((char *)(buf + 8), "WAVE", 4);
    strncpy((char *)(buf + 12), "fmt ", 4);
    WRITE_U32(buf+16, 16);  // format len
    WRITE_U16(buf+20, WAVE_FORMAT_PCM);
    WRITE_U16(buf+22, 2);  // wChannels
    WRITE_U32(buf+24, ins->sampleRate);
    WRITE_U32(buf+28, (2 * ins->sampleRate * (16 >> 3)));  //internal->wave.common.dwAvgBytesPerSec);
    WRITE_U16(buf+32, (2 * (16 >> 3)));  //internal->wave.common.wBlockAlign);
    WRITE_U16(buf+34, 16);  // bits per sample
    strncpy((char *)(buf + 36), "data", 4);  //internal->wave.data.id, 4);
    WRITE_U32(buf+40, (size - 44));  //internal->wave.data.len);

#if 0
    strncpy(buf, internal->wave.riff.id, 4);
    WRITE_U32(buf+4, internal->wave.riff.len);
    strncpy(buf+8, internal->wave.riff.wave_id, 4);
    strncpy(buf+12, internal->wave.format.id,4);
    WRITE_U32(buf+16, internal->wave.format.len);
    WRITE_U16(buf+20, internal->wave.common.wFormatTag);
    WRITE_U16(buf+22, internal->wave.common.wChannels);
    WRITE_U32(buf+24, internal->wave.common.dwSamplesPerSec);
    WRITE_U32(buf+28, internal->wave.common.dwAvgBytesPerSec);
    WRITE_U16(buf+32, internal->wave.common.wBlockAlign);
    WRITE_U16(buf+34, internal->wave.common.wBitsPerSample);
    strncpy(buf+36, internal->wave.data.id, 4);
    WRITE_U32(buf+40, internal->wave.data.len);
#endif

    if (fwrite(buf, sizeof(char), WAV_HEADER_LEN, ins->f) != WAV_HEADER_LEN) {
        xerror();
    }


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
    //short int buffer[MAX_CHARS * 4];  //FIXME
    //unsigned long sz;
    //size_t r;

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
        printf("wavfileout  new file: %s\n", ins->filename);
        deactivate(ins);
        activate(ins);
    }

    if (!ins->f) {
        return;
    }

#if 0
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

#endif

    for (i = 0;  i < sampleCount;  i++) {
        signed short int data;

        data = (signed short int)(ins->input_buffer_left[i] * 32767.5f);
        fwrite(&data, sizeof(signed short int), 1, ins->f);
        data = (signed short int)(ins->input_buffer_left[i] * 32767.5f);
        fwrite(&data, sizeof(signed short int), 1, ins->f);
    }

    if (ins->input_buffer_left) {
        if (!ins->output_buffer_left) {
            xerror();
        }
        memcpy(ins->output_buffer_left, ins->input_buffer_left, sampleCount * sizeof(float));
    }

    if (ins->input_buffer_right) {
        if (!ins->output_buffer_right) {
            xerror();
        }
        memcpy(ins->output_buffer_right, ins->input_buffer_right, sampleCount * sizeof(float));
    }

}


void be_wav_file_output_init (builtin_effect_t *be)
{
    be->label = "wavfileout";
    be->description = "write data to wav file";
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
