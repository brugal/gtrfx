#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

//#define BUF_SIZE 1024
//#define BUF_SIZE 128
//#define BUF_SIZE 46080

#define PERIODSIZE 8192
#define PERIODS 2
#define BUF_SIZE (PERIODSIZE * PERIODS)

snd_pcm_t *playback_handle;
snd_pcm_t *capture_handle;

int main (int argc, char *argv[])
{
    int err;
    unsigned int rate;
    signed short buf[PERIODSIZE * PERIODS];
    unsigned int resample;
    snd_pcm_hw_params_t *hw_params_capture;
    snd_pcm_hw_params_t *hw_params_playback;
    snd_pcm_uframes_t capBufferSize, playBufferSize;
    int periods = PERIODS;
    snd_pcm_uframes_t periodsize = PERIODSIZE;
    int loops;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <capture device> <playback device>\n", argv[0]);
        exit(1);
    }

    //////////   capture    /////////////

    if ((err = snd_pcm_open(&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio capture device %s (%s)\n",
                argv[1],
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params_capture)) < 0) {
        fprintf(stderr, "cannot allocate capture hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, hw_params_capture)) < 0) {
        fprintf(stderr, "cannot initialize capture hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

#if 0
    resample = 1;
    err = snd_pcm_hw_params_set_rate_resample(capture_handle, hw_params_capture, resample);
    if (err < 0) {
        fprintf(stderr, "resampling setup failed for capture device (%s)\n",
                snd_strerror(err));
        exit(1);
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params_capture, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot set capture access type (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params_capture, SND_PCM_FORMAT_S16_LE)) < 0) {
        fprintf(stderr, "cannot set capture sample format (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    rate = 44100;
    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params_capture, &rate, 0)) < 0) {
        fprintf(stderr, "cannot set capture sample rate (%s)\n",
                snd_strerror(err));
        exit(1);
    }
    printf("capture rate set to %d\n", rate);

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params_capture, 2)) < 0) {
        fprintf(stderr, "cannot set capture channel count (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    /* Set number of periods. Periods used to be called fragments. */
    if (snd_pcm_hw_params_set_periods(capture_handle, hw_params_capture, periods, 0) < 0) {
        fprintf(stderr, "Error setting capture periods.\n");
        exit(1);
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if (snd_pcm_hw_params_set_buffer_size(capture_handle, hw_params_capture, (periodsize * periods)>>2) < 0) {
            fprintf(stderr, "Error setting capture buffersize.\n");
            exit(1);
    }

    if ((err = snd_pcm_hw_params(capture_handle, hw_params_capture)) < 0) {
        fprintf(stderr, "cannot set capture parameters (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_get_buffer_size(hw_params_capture, &capBufferSize);
    printf("capture buffer size %d\n", (int)capBufferSize);
    printf("capture bits %d\n", snd_pcm_hw_params_get_sbits(hw_params_capture));
    if (capBufferSize > BUF_SIZE) {
        fprintf(stderr, "shit capture buffer size > BUF_SIZE  (%d > %d)\n", (int)capBufferSize, BUF_SIZE);
        exit(1);
    }


    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
        fprintf(stderr, "cannot prepare audio capture interface for use (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    ////////////////   playback    ///////////////

    if ((err = snd_pcm_open(&playback_handle, argv[2], SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio playback device %s (%s)\n",
                argv[2],
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params_playback)) < 0) {
        fprintf(stderr, "cannot allocate playback hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(playback_handle, hw_params_playback)) < 0) {
        fprintf(stderr, "cannot initialize playback hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

#if 0
    resample = 1;
    err = snd_pcm_hw_params_set_rate_resample(playback_handle, hw_params_playback, resample);
    if (err < 0) {
        fprintf(stderr, "resampling setup failed for playback device (%s)\n",
                snd_strerror(err));
        exit(1);
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params_playback, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot set playback access type (%s)\n",
                snd_strerror(err));
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params_playback, SND_PCM_FORMAT_S16_LE)) < 0) {
        fprintf (stderr, "cannot set playback sample format (%s)\n",
                 snd_strerror(err));
        exit(1);
    }

    rate = 44100;
    if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params_playback, &rate, 0)) < 0) {
        fprintf(stderr, "cannot set playback sample rate (%s)\n",
                snd_strerror(err));
        exit(1);
    }
    printf("playback rate set to %d\n", rate);

    if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params_playback, 2)) < 0) {
        fprintf(stderr, "cannot set playback channel count (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    /* Set number of periods. Periods used to be called fragments. */
    if (snd_pcm_hw_params_set_periods(playback_handle, hw_params_playback, periods, 0) < 0) {
        fprintf(stderr, "Error setting playback periods.\n");
        exit(1);
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if (snd_pcm_hw_params_set_buffer_size(playback_handle, hw_params_playback, (periodsize * periods)>>2) < 0) {
        fprintf(stderr, "Error setting playback buffersize.\n");
        exit(1);
    }

    if ((err = snd_pcm_hw_params(playback_handle, hw_params_playback)) < 0) {
        fprintf (stderr, "cannot set playback parameters (%s)\n",
                 snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_get_buffer_size(hw_params_playback, &playBufferSize);
    printf("playback buffer size %d\n", (int)playBufferSize);
    printf("playback bits %d\n", snd_pcm_hw_params_get_sbits(hw_params_playback));
    if (playBufferSize > BUF_SIZE) {
        fprintf(stderr, "shit playback buffer size > BUF_SIZE  (%d > %d)\n", (int)playBufferSize, BUF_SIZE);
        exit(1);
    }

    if ((err = snd_pcm_prepare(playback_handle)) < 0) {
        fprintf(stderr, "cannot prepare playback audio interface for use (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    //////////////////////////////

    if (playBufferSize != capBufferSize) {
        fprintf(stderr, "playBufferSize != capBufferSize\n");
        exit(1);
    }

    loops = 0;
    while (1) {
        loops++;

        while ((err = snd_pcm_readi(capture_handle, buf, periodsize >> 2)) < 0) {
            snd_pcm_prepare(capture_handle);
            fprintf(stderr, "%d <<<<<<<<<<<<<<< Buffer Overrun >>>>>>>>>>>>>>>\n", loops);
        }


        while ((err = snd_pcm_writei(playback_handle, buf, periodsize >> 2)) < 0) {
            snd_pcm_prepare(playback_handle);
            fprintf(stderr, "%d playback xrun\n", loops);
        }
    }

    printf("all done\n");

    if (hw_params_capture)
        snd_pcm_hw_params_free(hw_params_capture);
    if (hw_params_playback)
        snd_pcm_hw_params_free(hw_params_playback);
    if (capture_handle)
        snd_pcm_close(capture_handle);
    if (playback_handle)
        snd_pcm_close(playback_handle);

    return 0;
}
