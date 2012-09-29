#ifndef main_h_included
#define main_h_included

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gtkthread_s gtkthread_t;

struct gtkthread_s {
    int argc;
    char **argv;
};

int programExit;
extern float ldata[];
extern float rdata[];
extern int optionTuner;

    //extern char Title[];

//FIXME
//#define BUFDATA_SIZE (1024 * 8)
#define BUFDATA_SIZE (1024 * 2)

int bufDataPosIn;

extern float bufDataIn[];
int bufDataPosIn;

extern float bufDataOut[];
int bufDataPosOut;

extern int EffectsReady;

void parse_config_file (const char *filename);
void tuner (int sz);
void apply_effects (int sz);

#ifdef __cplusplus
}
#endif
#endif  // main_h_included
