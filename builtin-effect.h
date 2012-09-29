#ifndef builtin_effect_h_included
#define builtin_effect_h_included

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_FLOAT 0
#define INPUT_STRING 1
#define INPUT_BOOL 2
#define INPUT_FILE 3
#define INPUT_LIST 4

typedef struct builtin_effect_s builtin_effect_t;

struct builtin_effect_s {
    int id;
    void (*init)(builtin_effect_t *be);

    ///////////////////////////
    char *label;
    char *description;
    //int finputs;  // number of floating point inputs
    //int sinputs;  // number of string inputs
    //int foutputs;
    int nInputs;
    int *inputType;
    char **inputNames;

    int nOutputs;
    int *outputType;
    char **outputNames;

    //float **input;
    int *iHasMin;
    int *iHasMax;
    float *iMin;
    float *iMax;
    float *iDefault;
    int *iIsLog;
    int *iUsesSampleRate;  // *sampleRate
    //float **output;

    int nInputBuffers;
    int nOutputBuffers;
    //float **inputBuffers;
    //float **outputBuffers;

    void *(*instantiate)(unsigned long SampleRate);
    void (*cleanup)(void *instance);
    void (*activate)(void *instance);
    void (*deactivate)(void *instance);
    void (*connect_input_buffer)(void *instance, int num, float *buffer);
    void (*connect_output_buffer)(void *instance, int num, float *buffer);
    void (*connect_input)(void *instance, int num, void *input);
    void (*connect_output)(void *instance, int num, void *output);

    void (*run)(void *instance, unsigned long sampleCount);
};

enum {
FX_GAIN,
FX_LINEAR_GAIN,
FX_HARD_CLIP,
FX_SOFT_CLIP,
FX_HARD_GATE,
FX_CLIP_DETECT,
FX_MONO_TO_STEREO,
FX_WAV_FILE_INPUT,
FX_WAV_FILE_OUTPUT,
FX_OGG_FILE_OUTPUT,
FX_SOUND_PIPE_INPUT,
FX_TRIANGLE,
FX_SAW,
FX_ONLYMINMAX,
FX_ARCTAN_DISTORTION,
FX_BANANA_DISTORTION,
FX_DCOFFSET,
FX_VALVE_DISTORTION,
FX_FALLING_CLIP,
FX_CONV,
};


void be_gain_init (builtin_effect_t *be);
void be_linear_gain_init (builtin_effect_t *be);
void be_hard_clip_init (builtin_effect_t *be);
void be_soft_clip_init (builtin_effect_t *be);
void be_hard_gate_init (builtin_effect_t *be);
void be_clip_detect_init (builtin_effect_t *be);
void be_mono_to_stereo_init (builtin_effect_t *be);
void be_wav_file_input_init (builtin_effect_t *be);
void be_wav_file_output_init (builtin_effect_t *be);
void be_ogg_file_output_init (builtin_effect_t *be);
void be_sound_pipe_input_init (builtin_effect_t *be);
void be_triangle_init (builtin_effect_t *be);
void be_saw_init (builtin_effect_t *be);
void be_onlyminmax_init (builtin_effect_t *be);
void be_arctan_distortion_init (builtin_effect_t *be);
void be_banana_distortion_init (builtin_effect_t *be);
void be_dcoffset_init (builtin_effect_t *be);
void be_valve_distortion_init (builtin_effect_t *be);
void be_falling_clip_init (builtin_effect_t *be);
void be_conv_init (builtin_effect_t *be);

#ifdef __cplusplus
}
#endif
#endif  // builtin_effect_h_included
