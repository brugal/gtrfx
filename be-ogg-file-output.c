#include "builtin-effect.h"
#include "common.h"
#include "common-string.h"
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <vorbis/vorbisenc.h>

//FIXME only stereo in/out right now

// from encoder_example.c from oggenc package

static char *inputNames[] = { "quality", "filename" };
static int nInputs = 2;
static int nOutputs = 0;
static int inputType[] = { INPUT_FLOAT, INPUT_FILE };
static int iHasMin[] = { TRUE, FALSE };
static int iHasMax[] = { TRUE, FALSE };
static float iMin[] = { 0.0f, 0.0f };
static float iMax[] = { 1.0f, 0.0f };
static float iDefault[] = { 0.5f, 0.0f };
static int iIsLog[] = { FALSE, FALSE };  //FIXME ??
static int iUsesSampleRate[] = { FALSE, FALSE };

static int nInputBuffers = 2;
static int nOutputBuffers = 2;

typedef struct data_s data_t;

struct data_s {
    float *input_buffer_left;
    float *input_buffer_right;
    float *output_buffer_left;
    float *output_buffer_right;
    char *filename;
    float *quality;

    char currentFilename[MAX_CHARS];
    char parsedFilename[MAX_CHARS];
    unsigned long sampleRate;

    ogg_stream_state os; /* take physical pages, weld into a logical
                            stream of packets */
    ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
    ogg_packet       op; /* one raw packet of data for decode */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the user comments */

    vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

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
        //FIXME all the other shit
        //ogg_write(NULL, 0);
        fclose(ins->f);
    }
    free(ins);
}

static void activate (void *instance)
{
    data_t *ins = (data_t *)instance;
    int ret;
    int eos = 0;
    char dateString[MAX_CHARS];
    char titleString[MAX_CHARS];
    struct timeval tv;
    struct tm *tp;

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
        printf("%s:%s warning couldn't open file '%s'\n", __FILE__, __func__, ins->parsedFilename);
        return;
    }

    // here we go
    vorbis_info_init(&ins->vi);
    ret = vorbis_encode_init_vbr(&ins->vi, 2, ins->sampleRate, *ins->quality);
    if (ret) {
        //FIXME
        xerror();
    }
    vorbis_comment_init(&ins->vc);
    vorbis_comment_add_tag(&ins->vc, "ENCODER", "gtrfx");
    //FIXME way to pass in real name
    vorbis_comment_add_tag(&ins->vc, "ARTIST", getenv("USER"));

    vorbis_comment_add_tag(&ins->vc, "COMMENT", ins->parsedFilename);
    gettimeofday(&tv, NULL);
    tp = localtime(&tv.tv_sec);
    snprintf(dateString, MAX_CHARS, "%04d-%02d-%02d %02d:%02d:%02d", 1900 + tp->tm_year, 1 + tp->tm_mon, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    vorbis_comment_add_tag(&ins->vc, "DATE", dateString);
    snprintf(titleString, MAX_CHARS, "%s gtrfx %04d-%02d-%02d-%02d:%02d:%02d", getenv("USER"), 1900 + tp->tm_year, 1 + tp->tm_mon, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    vorbis_comment_add_tag(&ins->vc, "TITLE", titleString);

    /* set up the analysis state and auxiliary encoding storage */
    vorbis_analysis_init(&ins->vd, &ins->vi);
    vorbis_block_init(&ins->vd, &ins->vb);

    /* set up our packet->stream encoder */
    /* pick a random serial number; that way we can more likely build
       chained streams just by concatenation */
    srand(time(NULL));
    ogg_stream_init(&ins->os, rand());

    /* Vorbis streams begin with three headers; the initial header (with
     most of the codec setup parameters) which is mandated by the Ogg
     bitstream spec.  The second header holds any comment fields.  The
     third header holds the bitstream codebook.  We merely need to
     make the headers, then pass them to libvorbis one at a time;
     libvorbis handles the additional Ogg bitstream constraints */

    {
        ogg_packet header;
        ogg_packet header_comm;
        ogg_packet header_code;

        vorbis_analysis_headerout(&ins->vd, &ins->vc, &header, &header_comm, &header_code);
        ogg_stream_packetin(&ins->os, &header); /* automatically placed in its own page */
        ogg_stream_packetin(&ins->os, &header_comm);
        ogg_stream_packetin(&ins->os, &header_code);

        /* This ensures the actual
         * audio data will start on a new page, as per spec
         */
        while (!eos) {
            int result = ogg_stream_flush(&ins->os,&ins->og);
            if (result == 0)
                break;
            //fwrite(ins->og.header, 1, ins->og.header_len, stdout);
            //fwrite(ins->og.body, 1, ins->og.body_len, stdout);

            fwrite(ins->og.header, 1, ins->og.header_len, ins->f);
            fwrite(ins->og.body, 1, ins->og.body_len, ins->f);
        }
    }
}

static void deactivate (void *instance)
{
    data_t *ins = (data_t *)instance;

    if (!ins) {
        xerror();
    }

    if (ins->f) {
        //FIXME  bunch of shit
        //ogg_write(NULL, 0);
        //vorbis_analysis_wrote(&vd,0);  // signaling all done
        //ogg_stream_clear(&vb);
        //vorbis_clock_clear(&vb);

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

    switch(num) {
    case 0:
        ins->quality = (float *)input;
        break;
    case 1:
        ins->filename = (char *)input;
        break;
    default:
        // shouldn't get here
        break;
    }


}

static void run (void *instance, unsigned long sampleCount)
{
    data_t *ins = (data_t *)instance;
    int i;
    float **buffer;
    int eos = 0;

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

    if (sampleCount == 0) {
        printf("%s:%s  FIXME sampleCount is zero, decoder will shut down\n", __FILE__, __func__);
        xerror();
    }

    if (!str_matches(ins->currentFilename, ins->filename)) {
        printf("oggfileout  new file: %s\n", ins->filename);
        printf("current: %s\n", ins->currentFilename);
        deactivate(ins);
        activate(ins);
    }

    if (!ins->f) {
        return;
    }

    //FIXME zero write indicating no more data

    //buffer = vorbis_analysis_buffer(&ins->vd, READ);
    buffer = vorbis_analysis_buffer(&ins->vd, sampleCount);

    for (i = 0;  i < sampleCount;  i++) {
        buffer[0][i] = ins->input_buffer_left[i];
        buffer[1][i] = ins->input_buffer_right[i];
    }

    /* tell the library how much we actually submitted */
    vorbis_analysis_wrote(&ins->vd, sampleCount);

    /* vorbis does some data preanalysis, then divvies up blocks for
       more involved (potentially parallel) processing.  Get a single
       block for encoding now */
    while (vorbis_analysis_blockout(&ins->vd, &ins->vb) == 1) {
        /* analysis, assume we want to use bitrate management */
        vorbis_analysis(&ins->vb, NULL);
        vorbis_bitrate_addblock(&ins->vb);

        while (vorbis_bitrate_flushpacket(&ins->vd, &ins->op)) {
            /* weld the packet into the bitstream */
            ogg_stream_packetin(&ins->os, &ins->op);
            /* write out pages (if any) */
            while (!eos) {
                int result = ogg_stream_pageout(&ins->os, &ins->og);
                if (result == 0)
                    break;
                fwrite(ins->og.header, 1, ins->og.header_len, ins->f);
                fwrite(ins->og.body, 1, ins->og.body_len, ins->f);

                /* this could be set above, but for illustrative purposes, I do
                   it here (to show that vorbis does know where the stream ends) */
                if (ogg_page_eos(&ins->og))
                    eos = 1;
            }
        }
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


void be_ogg_file_output_init (builtin_effect_t *be)
{
    be->label = "oggfileout";
    be->description = "write data to ogg file";
    be->nInputs = nInputs;
    be->inputNames = inputNames;
    be->inputType = inputType;
    be->iHasMin = iHasMin;
    be->iHasMax = iHasMax;
    be->iMin = iMin;
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
