#include <stdio.h>
#include "sound.h"
#include "main.h"
#include "tuneit.h"
#include "common.h"
#include <string.h>
#include <libgen.h>
#include <jack/jack.h>

static jack_port_t *input_port;
static jack_port_t *output_port_left;
static jack_port_t *output_port_right;

static int dataSize;

static int process (jack_nframes_t nframes, void *arg)
{
    int i;
    static unsigned int count = 0;

    //count++;

    jack_default_audio_sample_t *out_left =  (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_left, nframes);
    jack_default_audio_sample_t *out_right =  (jack_default_audio_sample_t *) jack_port_get_buffer(output_port_right, nframes);
    jack_default_audio_sample_t *in = (jack_default_audio_sample_t *) jack_port_get_buffer(input_port, nframes);

    memcpy(ldata, in, sizeof(jack_default_audio_sample_t) * nframes);
    memset(rdata, 0, nframes);

    dataSize = nframes;

    //printf("dataSize %d\n", dataSize);

    for (i = 0;  i < dataSize;  i++) {
        if (bufDataPosIn >= BUFDATA_SIZE) {
            bufDataPosIn = 0;
        }
        bufDataIn[bufDataPosIn] = ldata[i];
        bufDataPosIn++;
    }

    if (optionTuner) {
        tuneit_runf(nframes, ldata);
        tuner(nframes);
    }

    if (EffectsReady) {
        if (count < 1) {
            printf("wating ... %d\n", count);
            count++;
        } else {
            apply_effects(nframes);
        }
    } else {
        printf("effects not ready, not processing yet\n");
    }

    for (i = 0;  i < dataSize;  i++) {
        if (bufDataPosOut >= BUFDATA_SIZE) {
            bufDataPosOut = 0;
        }
        bufDataOut[bufDataPosOut] = ldata[i];
        bufDataPosOut++;
    }

    memcpy(out_left, ldata, sizeof(jack_default_audio_sample_t) * nframes);
    memcpy(out_right, rdata, sizeof(jack_default_audio_sample_t) * nframes);

    return 0;
}

static void jack_shutdown (void *arg)
{
    printf("gtrfx jack shut us down\n");
}

static void jack_error (const char *desc)
{
    fprintf(stderr, "gtrfx jack error: %s\n", desc);
}

#if 0
static int jack_xrun_callback (void *arg)
{
    printf("xrun\n");
}
#endif

void *gtrfx_main_thread (void *data)
{
    jack_client_t *client;
    const char **ports;
    unsigned int rate;
    gtkthread_t *args = data;
    //FIXME
    time_t sec;
    jack_status_t jstatus;
    char st[MAX_CHARS];

    sec = time(NULL);

    if (args->argc > 1) {
        snprintf(st, MAX_CHARS, "gtrfx %s", basename(args->argv[1]));
    } else {
        strncpy(st, "gtrfx", MAX_CHARS);
    }

    client = jack_client_open(st, 0, &jstatus);
    if (client == NULL) {
        fprintf(stderr, "couldn't open jack client: %d\n", jstatus);
        exit(1);
    }

    jack_set_error_function(jack_error);

    jack_set_process_callback(client, process, 0);
    jack_on_shutdown(client, jack_shutdown, 0);
    SampleRate = jack_get_sample_rate(client);
    printf("engine sample rate: %d\n", SampleRate);

    if (args->argc < 2)
        parse_config_file(NULL);
    else
        parse_config_file(args->argv[1]);

    input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    output_port_left = jack_port_register (client, "output0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    output_port_right = jack_port_register (client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate(client)) {
        fprintf(stderr, "can't activate client\n");
        exit(1);
    }


    if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput)) == NULL) {
        fprintf(stderr, "Cannot find any physical capture ports\n");
        exit(1);
    }
    if (jack_connect (client, ports[0], jack_port_name (input_port))) {
        fprintf (stderr, "cannot connect input ports\n");
    }
    free(ports);
    if ((ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput)) == NULL) {
        fprintf(stderr, "Cannot find any physical playback ports\n");
        exit(1);
    }
    if (jack_connect (client, jack_port_name (output_port_left), ports[0])) {
        fprintf (stderr, "cannot connect output ports\n");
    }
    if (jack_connect (client, jack_port_name (output_port_right), ports[1])) {
        fprintf (stderr, "cannot connect output ports\n");
    }
    free(ports);

    rate = SampleRate;

    tuneit_init(SampleRate);

    printf("all done\n");

    //jack_set_process_callback(client, process, 0);

    EffectsReady = TRUE;


    return 0;
}
