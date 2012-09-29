//FIXME /usr/lib/ladspa/fm_osc_1415.so fmOsc crashes -- maybe need to check that input buffer has ok values???  stupid shit -- it should be the one checking these

//FIXME maybe add effects turned off (some ladspa are buggy with defaults)
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <alsa/asoundlib.h>
#include <ladspa.h>

#include <pthread.h>
#include <gtk/gtk.h>

#include <time.h>

#include <sys/types.h>
#include <dirent.h>

#include <ctype.h>
#include <libgen.h>

#include "common.h"
#include "common-string.h"
#include "builtin-effect.h"
#include "tuneit.h"
#include "main.h"
#include "sound.h"

#define LADSPA_PLUGIN_DIR "/usr/lib/ladspa"
#define MAX_LINE 1024
#define DEFAULT_CONFIG ".gtrfx"

#define MAX_EFFECT_BANKS 256

#define EFFECT_BUILTIN 0
#define EFFECT_LADSPA 1

#define MIX_REPLACE 0
#define MIX_ADD 1

/////////////////////////////////////////

#define OSCIL_WIDTH 300
#define OSCIL_HEIGHT 200

static builtin_effect_t builtin_effects[] = {
    { FX_GAIN, be_gain_init },
    { FX_LINEAR_GAIN, be_linear_gain_init },
    { FX_HARD_CLIP, be_hard_clip_init },
    { FX_SOFT_CLIP, be_soft_clip_init },
    { FX_HARD_GATE, be_hard_gate_init },
    { FX_CLIP_DETECT, be_clip_detect_init },
    { FX_MONO_TO_STEREO, be_mono_to_stereo_init },
    { FX_WAV_FILE_INPUT, be_wav_file_input_init },
    { FX_WAV_FILE_OUTPUT, be_wav_file_output_init },
    { FX_OGG_FILE_OUTPUT, be_ogg_file_output_init },
    { FX_SOUND_PIPE_INPUT, be_sound_pipe_input_init },
    { FX_TRIANGLE, be_triangle_init },
    { FX_SAW, be_saw_init },
    { FX_ONLYMINMAX, be_onlyminmax_init },
    { FX_ARCTAN_DISTORTION, be_arctan_distortion_init },
    { FX_BANANA_DISTORTION, be_banana_distortion_init },
    { FX_DCOFFSET, be_dcoffset_init },
    { FX_VALVE_DISTORTION, be_valve_distortion_init },
    { FX_FALLING_CLIP, be_falling_clip_init },
    { FX_CONV, be_conv_init },
};

static int numBuiltinEffects;


typedef struct gui_s gui_t;
struct gui_s
{
    GtkWidget *window;
    GtkWidget *swindow;
    GtkWidget *effectVbox;
    //GtkWidget *darea;
    GtkWidget *addEffectButton;
    GtkWidget *writeConfigButton;
    GtkWidget *clampLadspaInputButton;
    GtkWidget *tunerButton;
    GtkWidget *muteButton;
    GtkWidget *effectListWindow;
};
static gui_t gui;

typedef struct effect_s effect_t;

struct effect_s
{
    int valid;
    int on;  // on or off
    int type;  // builtin, ladspa, etc..
    int mixType;  // add or replace
    float mixvaldb;  // if mixType == MIX_ADD
    int id;  // for builtin effects

    int nInPorts;  // control ports doesn't include audio ports,
    int nOutPorts;
    int inChannels;
    int outChannels;

    float ifval[256];  //FIXME dynamic?
    //void **ival;
    //int **itype;
    float iDefaultVal[256];
    int ihasmin[256];
    int ihasmax[256];
    float imin[256];
    float imax[256];
    float itoggled[256];
    float iIsLog[256];
    float iIsInt[256];
    float ofval[256];
    int inputToPort[256];
    int outputToPort[256];
    int inChannelToPort[256];
    int outChannelToPort[256];

    int inputBufferIsBounded[256];
    float inputBufferMax[256];
    float inputBufferMin[256];

    //FIXME
    char strings[MAX_CHARS][16];

    const LADSPA_Descriptor *pdesc;
    LADSPA_Handle instanceHandle;
    void *libHandle;
    char libFname[MAX_CHARS];

    char pluginName[MAX_CHARS];
    char label[MAX_CHARS];
    char name[MAX_CHARS];

    effect_t *prev;
    effect_t *next;

    void *data;

    void (*init)(void);
    void (*cleanup)(void);
};

static effect_t *effects[MAX_EFFECT_BANKS];
//FIXME gui notebook form for diff banks
//FIXME mutex lock for currentEffectBank?
static int currentEffectBank = 0;

static GMutex *effects_lock;

typedef struct gui_effect_s gui_effect_t;

struct gui_effect_s {
    effect_t *effect;
    GtkWidget *label;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *hsep;
    GtkWidget *closeButton;
    GtkWidget *onOffButton;
    GtkWidget *moveUpButton;
    GtkWidget *moveDownButton;
    GtkWidget *mixTypeButton;
    GtkWidget *mixEntry;
};

typedef struct gui_entry_float_s gui_entry_float_t;
struct gui_entry_float_s {
    GtkWidget *nameLabel;
    GtkWidget *valueLabel;
    GtkWidget *entry;
    GtkWidget *hbox;
    GtkWidget *hscale;
    float *val;
};

typedef struct gui_entry_file_s gui_entry_file_t;
struct gui_entry_file_s {
    GtkWidget *entry;
    GtkWidget *file_select;
    char *s;
};

typedef struct plugin_list_s plugin_list_t;

struct plugin_list_s {
    int type;
    int id;
    char libname[MAX_CHARS];
    char name[MAX_CHARS];
    char description[MAX_CHARS];

    plugin_list_t *prev;
    plugin_list_t *next;
};

static plugin_list_t *plugins = NULL;
static plugin_list_t *plugin_last = NULL;

typedef struct oscil_s oscil_t;
struct oscil_s {
    GtkWidget *darea;
    guchar *rgb_buffer;
    int width;
    int height;
};

typedef struct visuals_s visuals_t;
struct visuals_s {
    oscil_t oscil_in;
    //FIXME tuner
    oscil_t oscil_out;
};

//FIXME don't use PERIODSIZE and PERIODS
// the final audio
float ldata[PERIODSIZE * PERIODS];
float rdata[PERIODSIZE * PERIODS];



float bufDataIn[BUFDATA_SIZE];
int bufDataPosIn;

float bufDataOut[BUFDATA_SIZE];
int bufDataPosOut;

// this is a dummy channel for plugins that have more than left right and we
// want to ignore
static float dummydata[PERIODSIZE * PERIODS];

// these are the tmp buffers for filters which then are mixed
static float lindata[PERIODSIZE * PERIODS];
static float rindata[PERIODSIZE * PERIODS];
static float loutdata[PERIODSIZE * PERIODS];
static float routdata[PERIODSIZE * PERIODS];

//FIXME take out
unsigned int SampleRate = 48000;
//unsigned int SampleRate = 44100;

#define FREQ_TABLE_MAX 96

static float freq_table[FREQ_TABLE_MAX];

int programExit = FALSE;

static int optionClampLadspaInput = FALSE;
int optionTuner = FALSE;
static int optionMute = FALSE;

int EffectsReady = FALSE;
char title[MAX_CHARS];

///////////////////////////////////////

static void get_ladspa_default_input_value_bounds_and_type (const LADSPA_Descriptor *desc, int inPort, effect_t *eff);
static effect_t *ladspa_effect_new (const char *libname, const char *label);
static void gui_add_ladspa_effect (effect_t *e, int pos);
static void gui_add_builtin_effect (effect_t *e, int pos);
static effect_t *builtin_effect_new (int id);

// returns TRUE even for stupid errors -- fuck that plugin
static int ladspa_plugin_bad (const LADSPA_Descriptor *pdesc)
{
    int i;

    if (!pdesc->Name) {
        fprintf(stderr, "warning no name given for ladspa plugin\n");
        return TRUE;
    } else {
        printf("Name: %s\n", pdesc->Name);
    }

    if (!pdesc->Maker) {
        fprintf(stderr, "warning maker not given for ladspa plugin\n");
        return TRUE;
    } else {
        printf("Maker: %s\n", pdesc->Maker);
    }

    if (!pdesc->Copyright) {
        fprintf(stderr, "warning no copyright for ladspa plugin\n");
        return TRUE;
    } else {
        printf("Copyright: %s\n", pdesc->Copyright);
    }

    if (pdesc->PortCount <= 0) {
        fprintf(stderr, "error, PortCount <= 0   %ld\n", pdesc->PortCount);
        return TRUE;
    } else {
        printf("PortCount: %ld\n", pdesc->PortCount);
    }

    if (!pdesc->PortDescriptors) {
        fprintf(stderr, "error no port descriptors\n");
        return TRUE;
    } else {
        for (i = 0;  i < pdesc->PortCount;  i++) {
            printf("port descriptor %d  %d\n", i, pdesc->PortDescriptors[i]);
        }
    }

    if (!pdesc->PortNames) {
        fprintf(stderr, "warning no port names\n");
        return TRUE;
    } else {
        for (i = 0;  i < pdesc->PortCount;  i++) {
            if (!pdesc->PortNames[i]) {
                fprintf(stderr, "warning port %d has no name\n", i);
                return TRUE;
            } else {
                printf("port %d:  %s\n", i, pdesc->PortNames[i]);
            }
        }
    }

    if (!pdesc->PortRangeHints) {
        fprintf(stderr, "error no port range hints\n");
        return TRUE;
    } else {
        for (i = 0;  i < pdesc->PortCount;  i++) {
            printf("port %d  range hint (%d  %f  ->  %f)\n", i, pdesc->PortRangeHints[i].HintDescriptor, pdesc->PortRangeHints[i].LowerBound, pdesc->PortRangeHints[i].UpperBound);
        }
    }

    if (pdesc->ImplementationData) {
        //printf("ImplementationData  %p  don't know what the fuck it's for -- ignoring plugin\n", pdesc->ImplementationData);
        //return TRUE;
        printf("ImplementationData  %p\n", pdesc->ImplementationData);
    } else {
        printf("ImplementationData  %p\n", pdesc->ImplementationData);
    }

    if (!pdesc->instantiate) {
        fprintf(stderr, "error no instantiate()\n");
        return TRUE;
    } else {
        printf("instantiate() %p\n", pdesc->instantiate);
    }

    if (!pdesc->connect_port) {
        fprintf(stderr, "error no connect_port()\n");
        return TRUE;
    } else {
        printf("connect_port() %p\n", pdesc->connect_port);
    }

    printf("activate()  %p\n", pdesc->activate);

    if (!pdesc->run) {
        fprintf(stderr, "no run()\n");
        return TRUE;
    } else {
        printf("run()  %p\n", pdesc->run);
    }

    printf("run_adding()  %p\n", pdesc->run_adding);
    printf("set_run_adding_gain()  %p\n", pdesc->set_run_adding_gain);

    if (pdesc->run_adding  &&  !pdesc->set_run_adding_gain) {
        fprintf(stderr, "run_adding() without set_run_adding_gain()\n");
        // even though we don't use this, bail anyway
        return TRUE;
    }

    printf("deactivate()  %p\n", pdesc->deactivate);
    printf("cleanup()  %p\n", pdesc->cleanup);

    return FALSE;
}

//FIXME maybe pass in the type
#if 1
static effect_t *effect_new (void)
{
    effect_t *e;

    e = calloc(1, sizeof(effect_t));
    if (!e) {
        xerror("couldn't allocate effect");
    }

    return e;
}
#endif

static void effect_prepend_to_bank (effect_t *eff, int bank)
{
    effect_t *elist;
    effect_t *old;

    if (eff == NULL) {
        xerror();
    }
    if (bank >= MAX_EFFECT_BANKS) {
        xerror("bank:%d >= MAX_EFFECT_BANKS", bank);
    }

    g_mutex_lock(effects_lock);

    elist = effects[bank];
    if (elist == NULL) {
        // this will be the first effect
        effects[bank] = eff;
        eff->prev = NULL;
        eff->next = NULL;
        g_mutex_unlock(effects_lock);
        return;
    }

    //for (;  elist->next != NULL;  elist = elist->next) {
    //    ;
    //}
    old = elist;

    effects[bank] = eff;
    eff->prev = NULL;
    eff->next = old;

    old->prev = eff;

    g_mutex_unlock(effects_lock);
}

static void effect_append_to_bank (effect_t *eff, int bank)
{
    effect_t *elist;

    if (eff == NULL) {
        xerror();
    }
    if (bank >= MAX_EFFECT_BANKS) {
        xerror("bank:%d >= MAX_EFFECT_BANKS", bank);
    }

    g_mutex_lock(effects_lock);

    elist = effects[bank];
    if (elist == NULL) {
        // this will be the first effect
        effects[bank] = eff;
        eff->prev = NULL;
        eff->next = NULL;
        g_mutex_unlock(effects_lock);
        return;
    }

    for (;  elist->next != NULL;  elist = elist->next) {
        ;
    }
    elist->next = eff;
    eff->prev = elist;
    eff->next = NULL;  // should already be
    g_mutex_unlock(effects_lock);
}

static void effect_remove (effect_t *eff)
{
    effect_t *prev, *next;
    int i;

    if (eff == NULL) {
        xerror();
    }

    g_mutex_lock(effects_lock);

    prev = eff->prev;
    next = eff->next;

    if (prev == NULL) {
        for (i = 0;  i < MAX_EFFECT_BANKS;  i++) {
            if (effects[i] == eff) {
                printf("reseting bank base %p < %p\n", eff, eff->next);
                effects[i] = eff->next;
                break;
            }
        }
        if (i >= MAX_EFFECT_BANKS) {
            xerror();
        }
    } else {
        prev->next = next;
    }

    if (next) {
        next->prev = prev;
    }

    g_mutex_unlock(effects_lock);
}

static void effect_free (effect_t *eff)
{
    //FIXME free other shit
    //printf("FIXME -- free other shit -- pdesc, effect data, etc.\n");
    if (eff->type == EFFECT_LADSPA) {
        if (eff->pdesc->deactivate) {
            eff->pdesc->deactivate(eff->instanceHandle);
        }
        if (eff->pdesc->cleanup) {
            eff->pdesc->cleanup(eff->instanceHandle);
        } else {
            //printf("no cleanup??  %s\n", eff->name);
        }
    } else if (eff->type == EFFECT_BUILTIN) {
        builtin_effects[eff->id].deactivate(eff->instanceHandle);
        builtin_effects[eff->id].cleanup(eff->instanceHandle);
    }

    if (eff->cleanup) {
        eff->cleanup();
    }

    if (eff->type == EFFECT_LADSPA) {
        dlclose(eff->libHandle);
    }

    if (eff->data) {
        free(eff->data);
    }

    free(eff);
}

static void effect_move_back (effect_t *eff, int bank)
{
    effect_t *zero, *one, *two, *three;

    if (!eff) {
        xerror();
    }

    g_mutex_lock(effects_lock);

    printf("moving back %s\n", eff->name);

    if (eff->prev == NULL) {  // already at beginning
        printf("effect_move_back: at beg, skipping\n");
        g_mutex_unlock(effects_lock);
        return;
    }

    if (eff->prev->prev == NULL) {  // we will now be the beginning of the list
        printf("setting to main head\n");
        effects[bank] = eff;
    }

    // 0     3
    //   1 2
    zero = one = two = three = NULL;

    two = eff;  // us
    one = eff->prev;
    three = eff->next;
    zero = one->prev;  // one checked for NULL at beginning of function

    // 0     3
    //   2 1
    if (zero) {
        zero->next = two;
    }
    one->prev = two;
    one->next = three;
    two->prev = zero;
    two->next = one;
    if (three) {
        three->prev = one;
    }

    g_mutex_unlock(effects_lock);
}

static void effect_move_forward (effect_t *eff, int bank)
{
    effect_t *zero, *one, *two, *three;

    if (!eff) {
        xerror();
    }

    g_mutex_lock(effects_lock);

    printf("moving forward %s\n", eff->name);

    if (eff->next == NULL) {  // already at beginning
        printf("effect_move_forward: at end, skipping\n");
        g_mutex_unlock(effects_lock);
        return;
    }

    if (eff->prev == NULL) {  // we will now be the beginning of the list
        printf("setting next to main head\n");
        effects[bank] = eff->next;
    }

    // 0     3
    //   1 2
    zero = one = two = three = NULL;

    one = eff;  // us
    zero = one->prev;
    two = one->next;
    three = two->next;  // two checked for NULL at beginning of function

    // 0     3
    //   2 1

    if (zero) {
        zero->next = two;
    }
    one->prev = two;
    one->next = three;
    two->prev = zero;
    two->next = one;
    if (three) {
        three->prev = one;
    }

    g_mutex_unlock(effects_lock);
}

// for ladspa name is shortened 'pdesc->Label', and
// description is 'pdesc->Name'
static void plugin_append (const char *libname, const char *name, const char *description, int type, int id)
{
    plugin_list_t *p;

    p = calloc(1, sizeof(plugin_list_t));
    if (!p) {
        xerror("couldn't allocate plugin");
    }
    strncpy(p->libname, libname, MAX_CHARS);
    strncpy(p->name, name, MAX_CHARS);
    strncpy(p->description, description, MAX_CHARS);
    p->type = type;
    p->id = id;
    if (plugins == NULL) {
        p->prev = NULL;
        p->next = NULL;
        plugins = p;
        plugin_last = p;
    } else {
    plugin_last->next = p;
    p->next = NULL;
    p->prev = plugin_last;
    plugin_last = p;
    }
}

static float dB (float d)
{
    return(powf(10, d / 20.0f));
}

static void entry_float_activate_cb (GtkWidget *widget, gpointer data)
{
    const gchar *etext;
    float f;
    gui_entry_float_t *entryData;

    entryData = (gui_entry_float_t *)data;

    etext = gtk_entry_get_text(GTK_ENTRY(widget));
    printf("new text: %s\n", etext);
    f = (float)strtod(etext, NULL);
    //*((float *)data) = f;
    *entryData->val = f;
    //FIXME the other stuff
    gtk_label_set_text(GTK_LABEL(entryData->valueLabel), etext);
    //gtk_entry_set_text(GTK_ENTRY(widget), "");
    gtk_range_set_value(GTK_RANGE(entryData->hscale), (gdouble)f);
}

static void entry_string_activate_cb (GtkWidget *widget, gpointer data)
{
    const gchar *etext;
    //float f;

    etext = gtk_entry_get_text(GTK_ENTRY(widget));
    printf("new text: %s\n", etext);
    //f = (float)strtod(etext, NULL);
    //*((float *)data) = f;
    strncpy((char *)data, etext, MAX_CHARS);
}


static void file_select_ok_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_entry_file_t *entryData = (gui_entry_file_t *)data;
    const gchar *newText;

    if (!entryData) {
        xerror();
    }

    newText = gtk_file_selection_get_filename(GTK_FILE_SELECTION(entryData->file_select));

    strncpy(entryData->s, newText, MAX_CHARS);
    gtk_entry_set_text(GTK_ENTRY(entryData->entry), newText);
}

static void entry_file_select_button_clicked_cb (GtkWidget *widget, gpointer data)
{
    gui_entry_file_t *entryData = (gui_entry_file_t *)data;
    GtkWidget *fs;

    if (!entryData) {
        xerror();
    }

     fs = gtk_file_selection_new("Please select a file");
    entryData->file_select = fs;

    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), gtk_entry_get_text(GTK_ENTRY(entryData->entry)));

    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked", GTK_SIGNAL_FUNC(file_select_ok_button_clicked_cb), data);

    // ensure that the dialog box is destroyed when the user clicks a button
    g_signal_connect_swapped (GTK_FILE_SELECTION (fs)->ok_button,
                              "clicked",
                              G_CALLBACK (gtk_widget_destroy),
                              fs);
    g_signal_connect_swapped (GTK_FILE_SELECTION (fs)->cancel_button,
                              "clicked",
                              G_CALLBACK (gtk_widget_destroy),
                              fs);

    gtk_widget_show(fs);
}

static void close_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;

    effect_remove(ge->effect);
    effect_free(ge->effect);

    gtk_widget_destroy(GTK_WIDGET(ge->vbox));
    free(ge);
}

static void on_off_toggle_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        ge->effect->on = TRUE;
        gtk_button_set_label(GTK_BUTTON(button), " on");
    } else {
        ge->effect->on = FALSE;
        gtk_button_set_label(GTK_BUTTON(button), "off");
    }
}

static void move_up_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;
    GList *g;
    int i;
    int found = FALSE;

    effect_move_back(ge->effect, currentEffectBank);

    for (i = 0, g = g_list_first(GTK_BOX(gui.effectVbox)->children);  g != NULL;  g = g_list_next(g), i++) {
        printf("%p\n", g->data);
        if (((struct _GtkBoxChild *)(g->data))->widget == ge->vbox) {
            printf("found at %d\n", i);
            found = TRUE;
            break;
        }
    }

    if (!found) {
        xerror();
    }

    if (i == 0) {  // already at beginning
        return;
    }

    gtk_box_reorder_child(GTK_BOX(gui.effectVbox), ge->vbox, i - 1);
}

static void move_down_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;
    GList *g;
    int i;
    int found = FALSE;

    effect_move_forward(ge->effect, currentEffectBank);

    g = g_list_last(GTK_BOX(gui.effectVbox)->children);
    if (!g) {
        xerror();
    }
    if (((struct _GtkBoxChild *)(g->data))->widget == ge->vbox) {
        // already at end
        printf("gui already at end of list\n");
        return;
    }

    for (i = 0, g = g_list_first(GTK_BOX(gui.effectVbox)->children);  g != NULL;  g = g_list_next(g), i++) {
        printf("%p\n", g->data);
        if (((struct _GtkBoxChild *)(g->data))->widget == ge->vbox) {
            printf("found at %d\n", i);
            found = TRUE;
            break;
        }
    }

    if (!found) {
        xerror();
    }

    gtk_box_reorder_child(GTK_BOX(gui.effectVbox), ge->vbox, i + 1);
}

static void mix_type_button_clicked_cb (GtkWidget *button, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        ge->effect->mixType = MIX_ADD;
        gtk_button_set_label(GTK_BUTTON(button), "add");
        gtk_widget_show(ge->mixEntry);
    } else {
        ge->effect->mixType = MIX_REPLACE;
        gtk_button_set_label(GTK_BUTTON(button), "rep");
        gtk_widget_hide(ge->mixEntry);
    }
}

static void mix_entry_activate_cb (GtkWidget *entry, gpointer data)
{
    gui_effect_t *ge = (gui_effect_t *)data;
    const gchar *etext;
    float f;

    etext = gtk_entry_get_text(GTK_ENTRY(entry));
    printf("new text for mix entry: %s\n", etext);
    f = (float)strtod(etext, NULL);
    ge->effect->mixvaldb = f;
}

static gui_effect_t *gui_effect_new (effect_t *eff, int pos)
{
    gui_effect_t *ge;
    GtkWidget *vsep;
    char text[MAX_CHARS];

    if (!eff) {
        xerror();
    }

    ge = calloc(1, sizeof(gui_effect_t));
    if (!ge) {
        xerror("couldn't allocate gui effect");
    }

    ge->effect = eff;
    ge->vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gui.effectVbox), ge->vbox, FALSE, FALSE, 0);

    if (pos == -1) {  // append
    } else if (pos == 0) {  // prepend
        gtk_box_reorder_child(GTK_BOX(gui.effectVbox), ge->vbox, 0);
    } else {
        printf("%s:%s  FIXME\n", __FILE__, __func__);
    }

    ge->hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), ge->hbox, FALSE, FALSE, 0);

    ge->closeButton = gtk_button_new_with_label("X");
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->closeButton, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(ge->closeButton), "clicked", GTK_SIGNAL_FUNC(close_button_clicked_cb), ge);
    vsep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(ge->hbox), vsep, FALSE, TRUE, 10);

    //FIXME, pixmaps or something
    ge->onOffButton = gtk_toggle_button_new_with_label(" on");
    //ge->onOffButton = gtk_check_button_new_with_label(" on");
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->onOffButton, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ge->onOffButton), ge->effect->on);
    gtk_signal_connect(GTK_OBJECT(ge->onOffButton), "clicked", GTK_SIGNAL_FUNC(on_off_toggle_button_clicked_cb), ge);
    if (!ge->effect->on) {
        gtk_button_set_label(GTK_BUTTON(ge->onOffButton), "off");
    }
    vsep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(ge->hbox), vsep, FALSE, TRUE, 10);
    ge->moveUpButton = gtk_button_new_with_label("^");
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->moveUpButton, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(ge->moveUpButton), "clicked", GTK_SIGNAL_FUNC(move_up_button_clicked_cb), ge);

    vsep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(ge->hbox), vsep, FALSE, TRUE, 10);
    ge->moveDownButton = gtk_button_new_with_label("\\/");
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->moveDownButton, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(ge->moveDownButton), "clicked", GTK_SIGNAL_FUNC(move_down_button_clicked_cb), ge);

    vsep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(ge->hbox), vsep, FALSE, TRUE, 10);
    if (ge->effect->mixType == MIX_REPLACE) {
        ge->mixTypeButton = gtk_toggle_button_new_with_label("rep");
    } else {
        ge->mixTypeButton = gtk_toggle_button_new_with_label("add");
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ge->mixTypeButton), ge->effect->mixType == MIX_ADD);
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->mixTypeButton, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(ge->mixTypeButton), "clicked", GTK_SIGNAL_FUNC(mix_type_button_clicked_cb), ge);
    ge->mixEntry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(ge->hbox), ge->mixEntry, FALSE, FALSE, 0);
    snprintf(text, MAX_CHARS, "%f", ge->effect->mixvaldb);
    gtk_entry_set_text(GTK_ENTRY(ge->mixEntry), text);
    gtk_signal_connect(GTK_OBJECT(ge->mixEntry), "activate", GTK_SIGNAL_FUNC(mix_entry_activate_cb), ge);

    ge->hsep = gtk_hseparator_new();
    gtk_box_pack_end(GTK_BOX(ge->vbox), ge->hsep, FALSE, TRUE, 10);
    gtk_widget_show_all(ge->hsep);

    gtk_widget_show_all(ge->vbox);

    //FIXME hack
    if (ge->effect->mixType == MIX_REPLACE) {
        gtk_widget_hide(ge->mixEntry);
    }

    return ge;
}

static void gui_add_label (gui_effect_t *ge, const char *name)
{
    if (!ge) {
        xerror();
    }

    strncpy(ge->effect->name, name, MAX_CHARS);

    ge->label = gtk_label_new(name);
    gtk_box_pack_start(GTK_BOX(ge->vbox), ge->label, TRUE, FALSE, 0);
    //gtk_box_pack_start(GTK_BOX(ge->hbox), ge->label, TRUE, FALSE, 0);
    gtk_widget_show_all(ge->label);
}

static gint hscale_float_value_changed_cb (GtkWidget *widget, gpointer data)
{
    gui_entry_float_t *entryData;
    float oldVal;
    float newVal;
    //float adjVal;

    entryData = (gui_entry_float_t *)data;

    oldVal = *entryData->val;
    newVal = (float)gtk_range_get_value(GTK_RANGE(entryData->hscale));

    printf("hscale value changed: %f (from %f)\n", newVal, oldVal);

    return TRUE;
}

static gint entry_float_destroy_cb (GtkWidget *widget, gpointer data)
{
    printf("float entry destroy cb  %p  %p\n", widget, data);
    g_free(data);

    return FALSE;
}

static gint entry_file_select_button_destroy_cb (GtkWidget *widget, gpointer data)
{
    //printf("entry file select button destroy cb\n");
    g_free(data);

    return FALSE;
}

static void gui_add_entry_float (gui_effect_t *ge, const char *name, float *val, int portNum)
{
    //GtkWidget *label;
    //GtkWidget *entry;
    //GtkWidget *hbox;
    //double min, max;
    char text[MAX_CHARS];
    gui_entry_float_t *entryData = NULL;

    if (!ge) {
        xerror();
    }

    entryData = g_malloc(sizeof(gui_entry_float_t));
    if (!entryData) {
        printf("couldn't allocate entryData\n");
        exit(1);
    }
    entryData->val = val;

    snprintf(text, MAX_CHARS, "%s:", name);

    entryData->hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), entryData->hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(entryData->hbox);

    entryData->nameLabel = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(entryData->hbox), entryData->nameLabel, FALSE, FALSE, 0);
    gtk_widget_show_all(entryData->nameLabel);

    entryData->entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(entryData->hbox), entryData->entry, FALSE, FALSE, 0);
    //gtk_entry_set_width_chars(GTK_ENTRY(entryData->entry), 10);
    gtk_widget_show_all(entryData->entry);

    snprintf(text, MAX_CHARS, "%f", *val);
    gtk_entry_set_text(GTK_ENTRY(entryData->entry), text);
    gtk_signal_connect(GTK_OBJECT(entryData->entry), "activate", GTK_SIGNAL_FUNC(entry_float_activate_cb), (gpointer)entryData);
    gtk_signal_connect(GTK_OBJECT(entryData->entry), "destroy", GTK_SIGNAL_FUNC(entry_float_destroy_cb), (gpointer)entryData);

    entryData->valueLabel = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(entryData->hbox), entryData->valueLabel, FALSE, FALSE, 10);
    //gtk_widget_show_all(entryData->valueLabel);

    //FIXME ranges
    //entryData->hscale = gtk_hscale_new_with_range(-666.0f, 666.0f, 1.0f);
    //entryData->hscale = gtk_hscale_new_with_range(-666666.0f, 666666.0f, 1.0f);
    //if (ge->effect->ihasmin
    //entryData->hscale = gtk_hscale_new_with_range((gdouble)min, (gdouble)max, 1.0f);
    //FIXME what if no min or max
    entryData->hscale = gtk_hscale_new_with_range(ge->effect->imin[portNum], ge->effect->imax[portNum], 100.0f);
    gtk_signal_connect(GTK_OBJECT(entryData->hscale), "value_changed", GTK_SIGNAL_FUNC(hscale_float_value_changed_cb), (gpointer)entryData);
    gtk_box_pack_end(GTK_BOX(entryData->hbox), entryData->hscale, FALSE, FALSE, 0);
    gtk_scale_set_digits(GTK_SCALE(entryData->hscale), 6);
    gtk_widget_set_usize(GTK_WIDGET(entryData->hscale), 250, -1);
    gtk_range_set_value(GTK_RANGE(entryData->hscale), (gdouble)*val);
    gtk_widget_show_all(entryData->hscale);

}

static void bool_toggle_clicked_cb (GtkWidget *checkButton, gpointer data)
{
    float *f = (float *)data;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkButton))) {
        *f = 1.0f;
    } else {
        *f = 0.0f;
    }
}

static void gui_add_bool_toggle (gui_effect_t *ge, const char *name, float *val, int portNum)
{
    GtkWidget *checkButton;
    GtkWidget *hbox;

    if (!ge) {
        xerror();
    }
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);

    checkButton = gtk_check_button_new_with_label(name);
    gtk_box_pack_start(GTK_BOX(hbox), checkButton, FALSE, FALSE, 0);
    gtk_widget_show_all(checkButton);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkButton), (gboolean)*val);
    gtk_signal_connect(GTK_OBJECT(checkButton), "clicked", GTK_SIGNAL_FUNC(bool_toggle_clicked_cb), val);
}

static void gui_add_entry_string (gui_effect_t *ge, const char *name, char *st)
{
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *hbox;
    char text[MAX_CHARS];

    if (!ge) {
        xerror();
    }

    snprintf(text, MAX_CHARS, "%s:", name);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
    label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show_all(label);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_widget_show_all(entry);
    //snprintf(text, MAX_CHARS, "%f", *val);
    //gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_entry_set_text(GTK_ENTRY(entry), st);
    gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(entry_string_activate_cb), st);

    //label = gtk_label_new(text);
    //gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    //gtk_widget_show_all(label);
}

static void gui_add_entry_file (gui_effect_t *ge, const char *name, char *st)
{
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *hbox;
    GtkWidget *button;
    char text[MAX_CHARS];
    gui_entry_file_t *entryData = NULL;

    if (!ge) {
        xerror();
    }

    entryData = g_malloc(sizeof(gui_entry_file_t));
    if (!entryData) {
        printf("couldn't allocate entryData\n");
        xerror();
    }
    entryData->s = st;

    snprintf(text, MAX_CHARS, "%s:", name);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
    label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show_all(label);

    entry = gtk_entry_new();
    entryData->entry = entry;
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_widget_show_all(entry);
    //snprintf(text, MAX_CHARS, "%f", *val);
    //gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_entry_set_text(GTK_ENTRY(entry), st);
    gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(entry_string_activate_cb), st);

    //label = gtk_label_new(text);
    //gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    //gtk_widget_show_all(label);

    button = gtk_button_new_with_label("select");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(entry_file_select_button_clicked_cb), (gpointer)entryData);
    gtk_signal_connect(GTK_OBJECT(button), "destroy", GTK_SIGNAL_FUNC(entry_file_select_button_destroy_cb), (gpointer)entryData);
    gtk_widget_show_all(button);

}

static void gui_add_output (gui_effect_t *ge, const char *name, float *val)
{
    GtkWidget *label;
    GtkWidget *hbox;
    char text[MAX_CHARS];

    if (!ge) {
        xerror();
    }

    snprintf(text, MAX_CHARS, "%s:", name);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ge->vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
    label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show_all(label);

    snprintf(text, MAX_CHARS, "[ %f ]", *val);
    label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show_all(label);
    //FIXME update val
}

#define SKIP_WORD() do { while (*sp != '\0'  &&  *sp != ' '  &&  *sp != '\t') { sp++; }} while (0);
#define SKIP_WHITESPACE() do { while(*sp == ' ' ||  *sp == '\t') { sp++; }} while(0);

void parse_config_file (const char *filename)
{
    FILE *f;
    char s[MAX_LINE];
    char *homeEnv;
    char *sp;
    char *sptmp;
    char defaultFilename[MAX_LINE];
    unsigned int lineNum;
    effect_t *e;
    int i;
    int numStrings;
    char st[MAX_CHARS];

    if (!filename) {
        title[0] = '\0';
        homeEnv = getenv("HOME");
        snprintf(defaultFilename, MAX_LINE, "%s/%s", homeEnv, DEFAULT_CONFIG);
        f = fopen(defaultFilename, "r");
        if (!f) {
            fprintf(stderr, "couldn't open default config: %s\n", DEFAULT_CONFIG);
            return;
        }
    } else {
        snprintf(title, MAX_CHARS, "%s", basename((char *)filename));
        f = fopen(filename, "r");
        if (!f) {
            fprintf(stderr, "couldn't open file %s\n", filename);
            return;
        }
    }

    printf("title: %s\n", title);

    gdk_threads_enter();

    snprintf(st, MAX_CHARS, "gtrfx %s", title);
    gtk_window_set_title(GTK_WINDOW(gui.window), st);

    lineNum = 0;
 getNewLine:
    while (1) {
        fgets(s, MAX_LINE, f);
        if (feof(f))
            break;
        lineNum++;
        if (strlen(s) >= MAX_LINE - 1) {
            fprintf(stderr, "warning parsing config line %d, length >= MAX_LINE - 1, skipping this line\n", lineNum);
            // just skip, depend on '\0'
            goto getNewLine;
        }

        // strip trailing whitespace
        sp = s + strlen(s) - 1;
        while (*sp == ' '  ||  *sp == '\t'  ||  *sp == '\n') {
            sp--;
        }
        sp[1] = '\0';

        // strip leading whitespace
        sp = s;
        while (*sp == ' ' || *sp == '\t') {
            sp++;
        }

        if (sp[0] == '\0')
            continue;

        if (sp[0] == '#')
            continue;

        if (str_begins_with(sp, "end"))
            break;

        if (str_begins_with(sp, "option")) {
            SKIP_WORD();
            SKIP_WHITESPACE();

            printf("found option line: ");

            if (str_begins_with(sp, "clampLadspaInput")) {
                printf("clamp ladspa input");
                SKIP_WORD();
                SKIP_WHITESPACE();
                if (str_begins_with(sp, "1")) {
                    optionClampLadspaInput = TRUE;
                } else if (str_begins_with(sp, "true")) {
                    optionClampLadspaInput = TRUE;
                } else {
                    optionClampLadspaInput = FALSE;
                }
                printf(" %d\n", optionClampLadspaInput);
                continue;
            } else if (str_begins_with(sp, "mute")) {
                printf("mute");
                if (str_begins_with(sp, "1")) {
                    optionMute = TRUE;
                } else if (str_begins_with(sp, "true")) {
                    optionMute = TRUE;
                } else {
                    optionMute = FALSE;
                }
                printf(" %d\n", optionMute);
                continue;
            } else {
                printf("unknown option '%s'\n", sp);
            }

            continue;
        }

        if (str_begins_with(sp, "fx")) {
            int mixType;
            float mixvaldb = 0.0f;
            int on;  // on or off

            // now mix type

            SKIP_WORD();
            SKIP_WHITESPACE();
            if (str_begins_with(sp, "on")) {
                on = TRUE;
            } else if (str_begins_with(sp, "off")) {
                on = FALSE;
            } else {
                printf("invalid value after 'fx': %s\n  should be 'on' or 'off'\n", sp);
                exit(1);
            }

            SKIP_WORD();
            SKIP_WHITESPACE();
            if (str_begins_with(sp, "add")) {
                mixType = MIX_ADD;
                mixvaldb = 0.0f;
                SKIP_WORD();
            } else if (str_begins_with(sp, "replace")) {
                printf("mixtype replace\n");
                mixType = MIX_REPLACE;
                SKIP_WORD();
            } else {
                //fprintf(stderr, "warning config line %d: no mix type\n", lineNum);
                // just assume MIX_REPLACE
                printf("using mix replace '%s'\n", sp);
                mixType = MIX_REPLACE;
                //break;
                //goto getNewLine;
            }

            SKIP_WHITESPACE();

            if (mixType == MIX_ADD) {
                if (sp[0] == '-'  ||  isdigit(sp[0])) {
                    // use mixvaldb
                    mixvaldb = (float)strtod(sp, NULL);
                    printf("using mixvaldb %f\n", mixvaldb);
                    SKIP_WORD();
                    SKIP_WHITESPACE();
                }
            }

            if (str_begins_with(sp, "builtin")) {
                printf("builtin\n");
                //sp += strlen("builtin");
                SKIP_WORD();
                SKIP_WHITESPACE();
                if (strlen(sp) == 0) {
                    fprintf(stderr, "warning config line %d: no builtin effect specified\n", lineNum);
                    continue;
                }
                //sp++;  // should be on a non whitespace char

                for (i = 0;  i < numBuiltinEffects;  i++) {
                    if (str_begins_with(sp, builtin_effects[i].label)) {
                        printf("found builtin effect\n");
                        break;
                    }
                }
                if (i >= numBuiltinEffects) {
                    fprintf(stderr, "warning config line %d:  unknown builtin effect\n", lineNum);
                    goto getNewLine;
                }

                e = builtin_effect_new(i);
                if (!e) {
                    xerror();
                }

                SKIP_WORD();

                sptmp = sp;
                numStrings = 0;
                for (i = 0;  i < e->nInPorts;  i++) {
                    SKIP_WHITESPACE();
                    if (*sp == '\0') {
                        //fprintf(stderr, "warning config line %d:  not enough input values for plugin\n", lineNum);
                        // keep default values for the rest
                        break;
                    }
                    switch (builtin_effects[e->id].inputType[i]) {
                    case INPUT_BOOL:
                    case INPUT_FLOAT: {
                        sscanf(sp, "%f", &e->ifval[i]);
                        break;
                    }
                    case INPUT_STRING: {
                        //FIXME more than one string
                        //FIXME this is buggy if a string comes before a float,
                        // ex:
                        // fx replace builtin oggfileout 0.6 /tmp/%d-just-guitar.ogg       works
                        // fx replace builtin oggfileout /tmp/%d-just-guitar.ogg 0.6    doesn't work
                        sscanf(sp, "%s", e->strings[numStrings]);
                        //printf("sldkfj %s\n", e->strings[numStrings]);
                        numStrings++;
                        break;
                    }
                    case INPUT_FILE: {
                        //FIXME more than one string
                        //FIXME this is buggy if a string comes before a float,
                        // ex:
                        // fx replace builtin oggfileout 0.6 /tmp/%d-just-guitar.ogg       works
                        // fx replace builtin oggfileout /tmp/%d-just-guitar.ogg 0.6    doesn't work
                        sscanf(sp, "%s", e->strings[numStrings]);
                        //printf("sldkfj %s\n", e->strings[numStrings]);
                        numStrings++;
                        break;
                    }
                    default:
                        xerror();
                    }

                    //printf("%f\t\t%s\n", e->ifval[i], e->pdesc->PortNames[e->inputToPort[i]]);
                    printf("%f\t\t%s\n", e->ifval[i], builtin_effects[e->id].inputNames[i]);
                    SKIP_WORD();
                }

                //FIXME string input -- tag in clip detect
                e->mixType = mixType;
                e->mixvaldb = mixvaldb;
                e->on = on;
                //strncpy(e->pluginName, builtin_effects[e->id].label, MAX_CHARS);
                effect_append_to_bank(e, currentEffectBank);
                gui_add_builtin_effect(e, -1);

                goto getNewLine;
            }
            if (str_begins_with(sp, "ladspa")) {
                char libFname[MAX_LINE];
                char pluginName[MAX_LINE];
                int ret;
                effect_t *e;

                sp += strlen("ladspa");
                if (strlen(sp) == 0) {
                    fprintf(stderr, "warning config line %d: no ladspa plugin listed\n", lineNum);
                    continue;
                }
                sp++;

                if (sp[0] == '"') {  // filename enclosed in quotes -- maybe filename has a space in it
                    int i;
                    int gotEnd;

                    for (i = 1, gotEnd = FALSE;  i < strlen(sp);  i++) {
                        if (sp[i] == '"') {
                            gotEnd = TRUE;
                            break;
                        }
                    }
                    if (!gotEnd) {
                        fprintf(stderr, "warning config line %d:  couldn't find ending quote for lib filename\n", lineNum);
                        continue;
                    }
                    memcpy(libFname, sp + 1, i - 1);
                    libFname[i] = '\0';
                    sp += i;  // at last quote
                    sp++;
                } else {
                    // just strcpy name
                    ret = sscanf(sp, "%s", libFname);
                    if (ret == EOF) {
                        fprintf(stderr, "warning config line %d: couldn't get ladspa library name\n", lineNum);
                        continue;
                    }
                    sp += strlen(libFname);
                }
                // skip white space
                while (*sp == ' ' || *sp == '\t') {
                    sp++;
                }
                if (*sp == '\0') {
                    fprintf(stderr, "warning config line %d: no ladspa plugin name found\n", lineNum);
                    continue;
                }

                ret = sscanf(sp, "%s", pluginName);
                if (ret == EOF) {
                    fprintf(stderr, "warning config line %d: couldn't get get plugin name\n", lineNum);
                    continue;
                }
                sp += strlen(pluginName);

                e = ladspa_effect_new(libFname, pluginName);
                if (!e) {
                    fprintf(stderr, "couldn't load ladspa %s\n", pluginName);
                    goto getNewLine;
                }
                // check if enough args for plugin where given in config file
                sptmp = sp;
                for (i = 0;  i < e->nInPorts;  i++) {
                    SKIP_WHITESPACE();
                    if (*sp == '\0') {
                        //fprintf(stderr, "warning config line %d:  not enough input values for plugin\n", lineNum);
                        // keep default values for the rest
                        break;
                    }
                    sscanf(sp, "%f", &e->ifval[i]);
                    printf("%f\t\t%s\n", e->ifval[i], e->pdesc->PortNames[e->inputToPort[i]]);
                    SKIP_WORD();
                }

                e->mixType = mixType;
                e->mixvaldb = mixvaldb;
                e->on = on;
                //strncpy(e->libFname, libFname, MAX_CHARS);
                //strncpy(e->pluginName, pluginName, MAX_CHARS);
                effect_append_to_bank(e, currentEffectBank);
                gui_add_ladspa_effect(e, -1);

                goto getNewLine;
            }
        }

        printf("warning unknown config line %d: '%s'\n", lineNum, s);
    }

    //FIXME update options in gui (ex mute, clampLadspaInput)

    gdk_flush();
    gdk_threads_leave();
}
#undef SKIP_WORD
#undef SKIP_WHITESPACE

static inline float clamp (float f, float cmin, float cmax)
{
    if (f < cmin)
        return cmin;
    else if (f > cmax)
        return cmax;

    return f;
}

void apply_effects (int sz)
{
    int j;
    effect_t *e;
    int i;

    g_mutex_lock(effects_lock);

    for (e = effects[currentEffectBank];  e != NULL;  e = e->next) {
        if (!e->valid) {
            printf("?? effect not valid FIXME\n");
            continue;
        }
        if (!e->on) {
            //FIXME maybe it still needs to be fed data, just not copy the results to the stream
            continue;
        }

        //FIXME mixing
        memcpy(lindata, ldata, sz * sizeof(float));
        memcpy(rindata, rdata, sz * sizeof(float));
        memset(loutdata, 0, sz * sizeof(float));
        memset(routdata, 0, sz * sizeof(float));
        memset(dummydata, 0, sz * sizeof(float));

        if (e->type == EFFECT_BUILTIN) {
            builtin_effects[e->id].run(e->instanceHandle, (unsigned long)sz);
        } else if (e->type == EFFECT_LADSPA) {
            if (!e->pdesc) {
                xerror("error trying to run ladspa effect, !e->pdesc");
            }

            //FIXME check if inputs have ranges -- but what then?  do we truncate?  stupid
            if (optionClampLadspaInput) {
                for (i = 0;  i < sz;  i++) {
                    lindata[i] = clamp(lindata[i], -1.0f, 1.0f);
                    rindata[i] = clamp(rindata[i], -1.0f, 1.0f);
                }
            }
            e->pdesc->run(e->instanceHandle, (unsigned long)sz);
        } else {
            xerror();
        }

        if (e->mixType == MIX_REPLACE) {
            memcpy(ldata, loutdata, sz * sizeof(float));
            memcpy(rdata, routdata, sz * sizeof(float));
        } else if (e->mixType == MIX_ADD) {
            if (e->mixvaldb == 0.0f) {
                //printf("no mixval\n");
                //FIXME is this right?  isn't it at half volume
                for (j = 0;  j < sz;  j++) {
                    ldata[j] = (ldata[j] + loutdata[j]) / 2.0f;
                    rdata[j] = (rdata[j] + routdata[j]) / 2.0f;
                }
            } else {
                //printf("using mixvaldb  %f\n", e->mixvaldb);
                for (j = 0;  j < sz;  j++) {
                    ldata[j] = (ldata[j] + (dB(e->mixvaldb) * loutdata[j])) / 2.0f;
                    rdata[j] = (rdata[j] + (dB(e->mixvaldb) * routdata[j])) / 2.0f;
                }
            }
        } else {
            xerror("unknown mix type");
        }
    }

    if (optionMute) {
        memset(ldata, 0, sz * sizeof(float));
        memset(rdata, 0, sz * sizeof(float));
    }

    g_mutex_unlock(effects_lock);
}

//FIXME this as a plugin
//static guchar *rgb_buffer_in = NULL;
//static int rgb_width_in = 300;
//static int rgb_height_in = 200;

static gint draw_oscil (GtkWidget *darea, guchar *rgb_buffer, int rgb_width, int rgb_height, float *bufData, int bufDataPos)
{
    //GtkWidget *darea = GTK_WIDGET(data);
    int i;
    int j;
    gboolean distort = FALSE;
    float maxHeight;
    int prevVal;

    // darea->allocation.width, darea->allocation.height

    if (rgb_width < 1) {
        fprintf(stderr, "%s: rgb_width < 1  (%d)\n", __FUNCTION__, rgb_width);
        return TRUE;
    }

    if (rgb_width > BUFDATA_SIZE) {
        fprintf(stderr, "%s: rgb_width %d > BUFDATA_SIZE %d\n", __FUNCTION__, rgb_width, BUFDATA_SIZE);
        return TRUE;
    }

    maxHeight = floorf(((float)rgb_height / 2.0f));

    prevVal = 0;
    //bzero(rgb_buffer, 3 * rgb_width * rgb_height);
    memset(rgb_buffer, 0, 3 * rgb_width * rgb_height);
    for (i = 0;  i < rgb_width;  i++) {
        int pos;
        int val;
        int index;
        int red = 0;

        pos = bufDataPos - 1;
        pos -= BUFDATA_SIZE;

        pos += (BUFDATA_SIZE / rgb_width) * i;

        if (pos < 0) {
            pos += BUFDATA_SIZE;
        }
        if (pos >= BUFDATA_SIZE) {
            printf("wtf %d\n", pos);
            pos -= BUFDATA_SIZE;
        }

        val = (int)(bufData[pos] * maxHeight);
        if (val >= (int)maxHeight) {
            val = (int)maxHeight;
            red = 255;
            distort = TRUE;
        }
        if (val <= -(int)maxHeight) {
            val = -(int)maxHeight;
            red = 255;
            distort = TRUE;
        }

        if (val > prevVal) {
            for (j = prevVal;  j <= val;  j++) {
                index = (rgb_height / 2) - j;
                if (index < 0) {
                    index = 0;
                }
                if (index > (rgb_height - 1)) {
                    index = rgb_height - 1;
                }
                index = index * (rgb_width * 3);
                index += (3 * i);

                if (red) {
                    rgb_buffer[index + 0] = red;
                } else {
                    rgb_buffer[index + 1] = 105;
                }
            }
        } else if (val < prevVal) {
            for (j = val;  j <= prevVal;  j++) {
                index = (rgb_height / 2) - j;
                if (index < 0) {
                    index = 0;
                }
                if (index > (rgb_height - 1)) {
                    index = rgb_height - 1;
                }
                index = index * (rgb_width * 3);
                index += (3 * i);

                if (red) {
                    rgb_buffer[index + 0] = red;
                } else {
                    rgb_buffer[index + 1] = 105;
                }
            }
        } else {

        // first which row
        index = (rgb_height / 2) - val;
        if (index < 0) {
            index = 0;
        }
        if (index > (rgb_height - 1)) {
            index = rgb_height - 1;
        }
        index = index * (rgb_width * 3);
        index += (3 * i);

        if (red) {
            rgb_buffer[index + 0] = red;
        } else {
            rgb_buffer[index + 1] = 255;
            rgb_buffer[index + 2] = 125;
        }
        }

        prevVal = val;
    }

    gdk_draw_rgb_image (darea->window,
                        darea->style->black_gc,
                        0,
                        0,
                        rgb_width,
                        rgb_height,
                        GDK_RGB_DITHER_NORMAL,
                        rgb_buffer,
                        3 * rgb_width);
    return TRUE;
}

static gint draw_stuff (gpointer data)
{
    visuals_t *vis = (visuals_t *)data;

    gdk_threads_enter();

    draw_oscil(vis->oscil_in.darea, vis->oscil_in.rgb_buffer, vis->oscil_in.width, vis->oscil_in.height, bufDataIn, bufDataPosIn);

    //FIXME tuner

    draw_oscil(vis->oscil_out.darea, vis->oscil_out.rgb_buffer, vis->oscil_out.width, vis->oscil_out.height, bufDataOut, bufDataPosOut);

    gdk_flush();
    gdk_threads_leave();

    return TRUE;
}

static int same_sign (float f1, float f2)
{
    if (f1 < 0.0f) {
        if (f2 < 0.0f)
            return TRUE;
        else
            return FALSE;
    } else {
        if (f2 >= 0.0f)
            return TRUE;
        else
            return FALSE;
    }
}

void tuner (int sz)
{
    int i;
    //static int carryOver = 0;
    float us, prev, next;
    //int points = 0;
    //float threshold = 0.0005;
    float threshold = 0.00075;
    float tunerRate;

    static int totalPoints = 0;
    static int totalSamples = 0;
    static int fullTotalSamples = 0;
    static float lastSample = 0.0f;
    static float lastMinMax = 0.0f;
    //static int lastMinMaxSampleNum = 0;
    static int firstMinMaxTime = 0;  // time == sample number
    static int lastMinMaxTime = 0;

    //printf("tuner: %d samples\n", sz);
    us = ldata[0];
    prev = lastSample;
    next = ldata[1];

    if ((us == prev  ||  us == next)  ||
        (us > prev  &&  us > next)  ||
        (us < prev  &&  us < next)  )
    {
        if (us >= threshold  ||  us <= -threshold) {
            if (!same_sign(us, lastMinMax)) {
                totalPoints++;
                lastMinMax = us;
                lastMinMaxTime = fullTotalSamples;
                if (totalPoints == 1) {
                    firstMinMaxTime = fullTotalSamples;
                }
            }
        }
    }

    for (i = 1;  i < sz - 1;  i++) {
        us = ldata[i];
        prev = ldata[i - 1];
        next = ldata[i + 1];

        if ((us == prev  ||  us == next)  ||
            (us > prev  &&  us > next)  ||
            (us < prev  &&  us < next)  )
        {
            if (us >= threshold  ||  us <= -threshold) {
                if (!same_sign(us, lastMinMax)) {
                    totalPoints++;
                    lastMinMax = us;
                    lastMinMaxTime = fullTotalSamples + i;
                    if (totalPoints == 1) {
                        firstMinMaxTime = fullTotalSamples + i;
                    }
                }
            }
        }
    }

    lastSample = ldata[sz - 1];

    //totalPoints += points;
    totalSamples += sz;
    fullTotalSamples += sz;

    tunerRate = 0.3f;
    tunerRate = 1.0f;

    if (totalSamples >= SampleRate / tunerRate) {
        float n;
        float fr;
        float nint;
        char *note_names[] = { "A", "Bb", "B", "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab" };
        int noteIndex;

        //fr = totalPoints / 2.0 * tunerRate;
        //FIXME tunerRate
        //FIXME lastMinMaxTime and firstMinMaxTime could be diff (one a min the other max)
        fr = (totalPoints / 2.0) / ((float)(lastMinMaxTime - firstMinMaxTime) / (float)SampleRate);
        //printf("points: %d    samples: %d\n", totalPoints, lastMinMaxTime - firstMinMaxTime);
        //printf("points:   %d  %d    ", totalPoints, totalSamples);
        // N(f) = (12/ln(2))*ln(f/27.5)
        if (fr > 0.0) {
            n = (12.0 / logf(2)) * logf(fr / 27.5);
        } else {
            n = 0.0f;
        }

        nint = roundf(n);
        //printf("%fhz  %f   %f", fr, n, nint);

        //printf("  %s  ", note_names[(int)(nint)%12]);
        noteIndex = fabs((int)(nint) % 12);
        //printf("    %s  %f", note_names[noteIndex], n - nint);
        //printf("\n");
        if (fr >= 27.5f) {
            printf("%s  %f  %0.2f cents\n", note_names[noteIndex], fr, (n - nint) * 100.0);
        }

        totalPoints = 0;
        totalSamples = 0;
    }
}

static void destroy_cb (GtkWidget *widget, gpointer data)
{
    programExit = TRUE;
    exit(0);
}

static void test_cb (GtkWidget *widget, gpointer data)
{
    effect_t *e;
    plugin_list_t *p;

    printf("test\n");
    for (e = effects[currentEffectBank];  e != NULL;  e = e->next) {
        printf("%p  %s\n", e, e->name);
    }
    printf("\n");

    for (p = plugins;  p != NULL;  p = p->next) {
        //printf("plugin: %s  %s\n", p->libname, p->name);
    }
}

static void get_ladspa_default_input_value_bounds_and_type (const LADSPA_Descriptor *desc, int inPort, effect_t *eff)
{
    LADSPA_PortRangeHintDescriptor iHintDescriptor;
    unsigned long lPortIndex;
    const LADSPA_Descriptor * psDescriptor;
    LADSPA_Data fBound;
    LADSPA_Data fDefault;

    if (!desc) {
        xerror();
    }
    if (!eff) {
        xerror();
    }

    psDescriptor = desc;
    lPortIndex = eff->inputToPort[inPort];
    printf("\t%s (%d -> %ld): ", desc->PortNames[lPortIndex], inPort, lPortIndex);

    iHintDescriptor = psDescriptor->PortRangeHints[lPortIndex].HintDescriptor;

    // copy paste analyseplugin.c
    if (LADSPA_IS_HINT_BOUNDED_BELOW(iHintDescriptor)
        || LADSPA_IS_HINT_BOUNDED_ABOVE(iHintDescriptor)) {
        if (LADSPA_IS_HINT_BOUNDED_BELOW(iHintDescriptor)) {
            fBound = psDescriptor->PortRangeHints[lPortIndex].LowerBound;
            eff->ihasmin[inPort] = TRUE;
            eff->imin[inPort] = fBound;
            if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fBound != 0) {
                printf("s %g", fBound * SampleRate);
                eff->imin[inPort] *= SampleRate;
                printf("FIXME double checking: %f    %f\n", eff->imin[inPort], fBound * SampleRate);
            } else {
                printf("%g", fBound);
            }
        } else {
            eff->ihasmin[inPort] = FALSE;
            eff->imin[inPort] = -FLT_MAX;
            printf("...");
        }
        printf(" to ");
        if (LADSPA_IS_HINT_BOUNDED_ABOVE(iHintDescriptor)) {
            fBound = psDescriptor->PortRangeHints[lPortIndex].UpperBound;
            eff->ihasmax[inPort] = TRUE;
            eff->imax[inPort] = fBound;
            if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fBound != 0) {
                printf("%g", fBound * SampleRate);
                eff->imax[inPort] *= SampleRate;
            } else {
                printf("%g", fBound);
            }
        } else {
            eff->ihasmax[inPort] = FALSE;
            eff->imax[inPort] = FLT_MAX;
            printf("...");
        }
    }

    if (LADSPA_IS_HINT_TOGGLED(iHintDescriptor)) {
        if ((iHintDescriptor
               | LADSPA_HINT_DEFAULT_0
             | LADSPA_HINT_DEFAULT_1)
            != (LADSPA_HINT_TOGGLED
                   | LADSPA_HINT_DEFAULT_0
                | LADSPA_HINT_DEFAULT_1)) {
            //FIXME wtf??
            printf(", ERROR: TOGGLED INCOMPATIBLE WITH OTHER HINT");
        } else {
            printf(", toggled");
        }
        eff->itoggled[inPort] = TRUE;
    } else {
        eff->itoggled[inPort] = FALSE;
    }

    switch (iHintDescriptor & LADSPA_HINT_DEFAULT_MASK) {
    case LADSPA_HINT_DEFAULT_MINIMUM:
        fDefault = psDescriptor->PortRangeHints[lPortIndex].LowerBound;
        if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fDefault != 0) {
            printf(", default %g", fDefault * SampleRate);
        } else {
            printf(", default %g", fDefault);
        }
        eff->iDefaultVal[inPort] = eff->imin[inPort];
        break;
    case LADSPA_HINT_DEFAULT_LOW:
        if (LADSPA_IS_HINT_LOGARITHMIC(iHintDescriptor)) {
            fDefault
                = exp(log(psDescriptor->PortRangeHints[lPortIndex].LowerBound)
                    * 0.75
                      + log(psDescriptor->PortRangeHints[lPortIndex].UpperBound)
                      * 0.25);
            eff->iDefaultVal[inPort] = fDefault;
        } else {
            fDefault
                = (psDescriptor->PortRangeHints[lPortIndex].LowerBound
                 * 0.75
                 + psDescriptor->PortRangeHints[lPortIndex].UpperBound
                   * 0.25);
            eff->iDefaultVal[inPort] = fDefault;
        }
        if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fDefault != 0) {
            printf(", default (sample rate) %g", fDefault * SampleRate);
            eff->iDefaultVal[inPort] *= SampleRate;
        } else {
            printf(", default %g", fDefault);
        }
        break;
    case LADSPA_HINT_DEFAULT_NONE:
        // fuck you
        printf(", NO DEFAULT USING MIDDLE");
    case LADSPA_HINT_DEFAULT_MIDDLE:
        if (LADSPA_IS_HINT_LOGARITHMIC(iHintDescriptor)) {
            fDefault
                = sqrt(psDescriptor->PortRangeHints[lPortIndex].LowerBound
                       * psDescriptor->PortRangeHints[lPortIndex].UpperBound);
        } else {
            fDefault
                = 0.5 * (psDescriptor->PortRangeHints[lPortIndex].LowerBound
                         + psDescriptor->PortRangeHints[lPortIndex].UpperBound);
        }
        eff->iDefaultVal[inPort] = fDefault;
        if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fDefault != 0) {
            printf(", default (sample rate) %g", fDefault * SampleRate);
            eff->iDefaultVal[inPort] *= SampleRate;
        } else {
            printf(", default %g", fDefault);
        }
        break;
    case LADSPA_HINT_DEFAULT_HIGH:
        if (LADSPA_IS_HINT_LOGARITHMIC(iHintDescriptor)) {
            fDefault
                = exp(log(psDescriptor->PortRangeHints[lPortIndex].LowerBound)
                    * 0.25
                      + log(psDescriptor->PortRangeHints[lPortIndex].UpperBound)
                      * 0.75);
        } else {
            fDefault
                = (psDescriptor->PortRangeHints[lPortIndex].LowerBound
                 * 0.25
                 + psDescriptor->PortRangeHints[lPortIndex].UpperBound
                   * 0.75);
        }
        eff->iDefaultVal[inPort] = fDefault;
        if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fDefault != 0) {
            printf(", default (sample rate) %g", fDefault * SampleRate);
            eff->iDefaultVal[inPort] *= SampleRate;
        } else {
            printf(", default %g", fDefault);
        }
        break;
    case LADSPA_HINT_DEFAULT_MAXIMUM:
        fDefault = psDescriptor->PortRangeHints[lPortIndex].UpperBound;
        eff->iDefaultVal[inPort] = fDefault;
        if (LADSPA_IS_HINT_SAMPLE_RATE(iHintDescriptor) && fDefault != 0) {
            printf(", default (sample rate) %g", fDefault * SampleRate);
            eff->iDefaultVal[inPort] *= SampleRate;
        } else {
            printf(", default %g", fDefault);
        }
        break;
    case LADSPA_HINT_DEFAULT_0:
        printf(", default [0]");
        eff->iDefaultVal[inPort] = 0.0f;
        break;
    case LADSPA_HINT_DEFAULT_1:
        printf(", default [1]");
        eff->iDefaultVal[inPort] = 1.0f;
        break;
    case LADSPA_HINT_DEFAULT_100:
        printf(", default [100]");
        eff->iDefaultVal[inPort] = 100.0f;
        break;
    case LADSPA_HINT_DEFAULT_440:
        printf(", default [440]");
        eff->iDefaultVal[inPort] = 440.0f;
        break;
    default:
        printf(", UNKNOWN DEFAULT CODE setting to 0.0f");
        eff->iDefaultVal[inPort] = 0.0f;
        /* (Not necessarily an error - may be a newer version.) */
        // yeah ^^^^^ cock sucker
        break;
    }

    if (LADSPA_IS_HINT_LOGARITHMIC(iHintDescriptor)) {
        printf(", logarithmic");
        eff->iIsLog[inPort] = TRUE;
    } else {
        eff->iIsLog[inPort] = FALSE;
    }

    if (LADSPA_IS_HINT_INTEGER(iHintDescriptor)) {
        printf(", integer");
        eff->iIsInt[inPort] = TRUE;
    } else {
        eff->iIsInt[inPort] = FALSE;
    }

    printf("\n");
}

static void scan_plugins (void)
{
    DIR *dp;
    struct dirent *ep;
    char fullLibName[MAX_CHARS];
    void *libHandle;
    LADSPA_Descriptor_Function descFunc;
    const LADSPA_Descriptor *desc;
    int i;

    // builtin first
    for (i = 0;  i < numBuiltinEffects;  i++) {
        //printf("add %d %d\n", i, builtin_effects[i].id);
        plugin_append("builtin", builtin_effects[i].label, builtin_effects[i].description, EFFECT_BUILTIN, builtin_effects[i].id);
    }

    dp = opendir(LADSPA_PLUGIN_DIR);
    if (!dp) {
        fprintf(stderr, "couldn't open plugin dir %s\n", LADSPA_PLUGIN_DIR);
        return;
    }

    while ((ep = readdir(dp))) {
        snprintf(fullLibName, MAX_CHARS, "%s/%s", LADSPA_PLUGIN_DIR, ep->d_name);
        libHandle = dlopen(fullLibName, RTLD_NOW);
        if (!libHandle) {
            printf("ignoring: %s\n", fullLibName);
            continue;
        }
        descFunc = (LADSPA_Descriptor_Function)dlsym(libHandle, "ladspa_descriptor");
        if (!descFunc) {
            fprintf(stderr, "couldn't get desc function\n");
            dlclose(libHandle);
            continue;
        }
        for (i = 0;  ;  i++) {
            desc = descFunc(i);
            if (!desc) {
                break;
            }
            plugin_append(fullLibName, desc->Label, desc->Name, EFFECT_LADSPA, desc->UniqueID);
        }
        dlclose(libHandle);
    }
    closedir(dp);
    printf("done scanning\n");
}

//FIXME name change
//static effect_t *ladspa_name_to_effect (const char *libname, const char *label)
static effect_t *ladspa_effect_new (const char *libname, const char *label)
{
    void *libHandle;
    LADSPA_Descriptor_Function descFunc;
    const LADSPA_Descriptor *pdesc;
    LADSPA_Handle instanceHandle;
    LADSPA_PortRangeHintDescriptor hint;
    int nInBuffers;
    int nOutBuffers;
    int nInPorts;
    int nOutPorts;
    effect_t *e;
    int i;

    printf("%s\n", libname);
    //FIXME keep track of already open libs?
    libHandle = dlopen(libname, RTLD_NOW);
    if (!libHandle) {
        fprintf(stderr, "couldn't open lib\n");
        return NULL;
    }

    descFunc = (LADSPA_Descriptor_Function)dlsym(libHandle, "ladspa_descriptor");
    if (!descFunc) {
        fprintf(stderr, "couldn't get desc function\n");
        dlclose(libHandle);
        return NULL;
    }
    for (i = 0;  ;  i++) {
        pdesc = descFunc(i);
        if (!pdesc) {
            fprintf(stderr, "couldn't find descriptor\n");
            dlclose(libHandle);
            return NULL;
        }
        if (str_matches(label, pdesc->Label)) {
            // found it
            printf("%s\n", pdesc->Label);
            printf("\"%s\"\n", pdesc->Name);
            break;
        }
    }

    if (ladspa_plugin_bad(pdesc)) {
        printf("bad plugin, skipping\n");
        dlclose(libHandle);
        return NULL;
    }

    instanceHandle = pdesc->instantiate(pdesc, SampleRate);
    if (!instanceHandle) {
        xerror();
    }
    printf("instanceHandle: %p\n", instanceHandle);

    // crashing plugins -- need to have ports connected before activation
#if 0
    if (pdesc->activate) {
        pdesc->activate(instanceHandle);
        printf("activated\n");
    }
#endif

    e = effect_new();

    // first just count ports and inputs
    nInBuffers = nOutBuffers = nInPorts = nOutPorts = 0;

    for (i = 0;  i < pdesc->PortCount;  i++) {
        LADSPA_PortDescriptor pd;

        pd = pdesc->PortDescriptors[i];
        if (LADSPA_IS_PORT_AUDIO(pd)) {
            if (LADSPA_IS_PORT_INPUT(pd)) {
                hint = pdesc->PortRangeHints[i].HintDescriptor;
                e->inChannelToPort[nInBuffers] = i;
                // check if input buffers are bounded
                if (hint) {
                    e->inputBufferIsBounded[i] = TRUE;
                    if (LADSPA_IS_HINT_BOUNDED_BELOW(hint)) {
                        e->inputBufferMin[i] = pdesc->PortRangeHints[i].LowerBound;
                        if (LADSPA_IS_HINT_SAMPLE_RATE(hint)) {
                            e->inputBufferMin[i] *= SampleRate;
                        }
                        printf("inputBuffer %d min  %f\n", nInBuffers, e->inputBufferMin[i]);
                    } else {
                        fprintf(stderr, "warning no lower bound for input buffer\n");
                        e->inputBufferMin[i] = -FLT_MAX;  //FIXME what else can you fucking do?
                    }
                    if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint)) {
                        e->inputBufferMax[i] = pdesc->PortRangeHints[i].UpperBound;
                        if (LADSPA_IS_HINT_SAMPLE_RATE(hint)) {
                            e->inputBufferMax[i] *= SampleRate;
                        }
                        printf("inputBuffer %d max  %f\n", nInBuffers, e->inputBufferMax[i]);
                    } else {
                        fprintf(stderr, "warning no upper bound for input buffer\n");
                        e->inputBufferMax[i] = FLT_MAX;  //FIXME what else can you fucking do?
                    }

                }
                if (nInBuffers == 0) {  // left
                    pdesc->connect_port(instanceHandle, i, lindata);
                } else if (nInBuffers == 1) {  // right
                    pdesc->connect_port(instanceHandle, i, rindata);
                } else {
                    printf("FIXME plugin with %d input buffers\n", nInBuffers);
                    pdesc->connect_port(instanceHandle, i, dummydata);
                }
                printf("audio in port %d connected\n", i);
                nInBuffers++;
            } else if (LADSPA_IS_PORT_OUTPUT(pd)) {
                hint = pdesc->PortRangeHints[i].HintDescriptor;
                if (hint) {
                    // don't tell me this shit can be bounded too
                    printf("wtf ---------  bounded output\n");
                }
                e->outChannelToPort[nOutBuffers] = i;

                if (nOutBuffers == 0) {  // left
                    pdesc->connect_port(instanceHandle, i, loutdata);
                } else if (nOutBuffers == 1) {  // right
                    pdesc->connect_port(instanceHandle, i, routdata);
                } else {
                    printf("FIXME plugin with %d output buffers\n", nOutBuffers);
                    pdesc->connect_port(instanceHandle, i, dummydata);
                }
                printf("audio outport %d connected\n", i);
                nOutBuffers++;
            }
        } else if (LADSPA_IS_PORT_CONTROL(pd)) {
            if (LADSPA_IS_PORT_INPUT(pd)) {
                // get_ladspa_default...()  needs this
                e->inputToPort[nInPorts] = i;

                get_ladspa_default_input_value_bounds_and_type(pdesc, nInPorts, e);
                e->ifval[nInPorts] = e->iDefaultVal[nInPorts];
                pdesc->connect_port(instanceHandle, i, &e->ifval[nInPorts]);
                printf("in port %d connected\n", i);

                nInPorts++;
            } else if (LADSPA_IS_PORT_OUTPUT(pd)) {
                pdesc->connect_port(instanceHandle, i, &e->ofval[nOutPorts]);
                printf("out port %d connected\n", i);
                e->outputToPort[nOutPorts] = i;
                nOutPorts++;
            }
        }
    }

    // that's it, ready to go

#if 1
    //FIXME let caller do this?
    if (pdesc->activate) {
        pdesc->activate(instanceHandle);
        printf("activated\n");
    }
#endif

    printf("ready to go\n");
    e->valid = 1;
    e->on = 1;
    e->type = EFFECT_LADSPA;
    e->pdesc = pdesc;
    e->instanceHandle = instanceHandle;
    e->inChannels = nInBuffers;
    e->outChannels = nOutBuffers;
    e->nInPorts = nInPorts;
    e->nOutPorts = nOutPorts;
    e->mixType = MIX_REPLACE;  //MIX_ADD;
    e->mixvaldb = 0.0f;
    e->libHandle = libHandle;

    strncpy(e->label, pdesc->Name, MAX_CHARS);
    strncpy(e->libFname, libname, MAX_CHARS);
    strncpy(e->pluginName, label, MAX_CHARS);
    return e;
}

static effect_t *builtin_effect_new (int id)
{
    effect_t *e;
    //void *instanceHandel;
    int i;
    int numStrings;

    e = effect_new();
    if (!e) {
        xerror("couldn't create effect");
    }

    printf("new builtin effect %d, %s\n", id, builtin_effects[id].label);

    e->instanceHandle = builtin_effects[id].instantiate(SampleRate);
    if (!e->instanceHandle) {
        xerror();
    }

    builtin_effects[id].activate(e->instanceHandle);

    numStrings = 0;
    for (i = 0;  i < builtin_effects[id].nInputs;  i++) {
        switch(builtin_effects[id].inputType[i]) {
        case INPUT_FLOAT: {
            builtin_effects[id].connect_input(e->instanceHandle, i, &e->ifval[i]);
            e->iDefaultVal[i] = builtin_effects[id].iDefault[i];
            e->ifval[i] = builtin_effects[id].iDefault[i];
            e->itoggled[i] = FALSE;
            break;
        }
        case INPUT_BOOL: {
            builtin_effects[id].connect_input(e->instanceHandle, i, &e->ifval[i]);
            e->iDefaultVal[i] = builtin_effects[id].iDefault[i];
            e->ifval[i] = builtin_effects[id].iDefault[i];
            e->itoggled[i] = TRUE;
            break;
        }
        case INPUT_STRING: {
            //FIXME check num strings -- else dynamic
            builtin_effects[id].connect_input(e->instanceHandle, i, e->strings[numStrings]);
            numStrings++;
            e->itoggled[i] = FALSE;
            break;
        }
        case INPUT_FILE: {
            //FIXME check num strings -- else dynamic
            builtin_effects[id].connect_input(e->instanceHandle, i, e->strings[numStrings]);
            numStrings++;
            e->itoggled[i] = FALSE;
            break;
        }
        default:
            xerror();
        }
    }

    for (i = 0;  i < builtin_effects[id].nInputBuffers;  i++) {
        if (i == 0) {  // left
            builtin_effects[id].connect_input_buffer(e->instanceHandle, i, lindata);
        } else if (i == 1) {  // right
            builtin_effects[id].connect_input_buffer(e->instanceHandle, i, rindata);
        } else {
            fprintf(stderr, "FIXME builtin effect with more than two input buffers\n");
            builtin_effects[id].connect_input_buffer(e->instanceHandle, i, dummydata);
        }
    }

    for (i = 0;  i < builtin_effects[id].nOutputBuffers;  i++) {
        if (i == 0) {  // left
            builtin_effects[id].connect_output_buffer(e->instanceHandle, i, loutdata);
        } else if (i == 1) {  // right
            builtin_effects[id].connect_output_buffer(e->instanceHandle, i, routdata);
        } else {
            fprintf(stderr, "FIXME builtin effect with more than two output buffers\n");
            builtin_effects[id].connect_output_buffer(e->instanceHandle, i, dummydata);
        }
    }

    builtin_effects[id].activate(e->instanceHandle);

    e->valid = 1;
    e->on = 1;
    e->type = EFFECT_BUILTIN;
    e->id = id;
    e->inChannels = builtin_effects[id].nInputBuffers;
    e->outChannels = builtin_effects[id].nOutputBuffers;
    e->nInPorts = builtin_effects[id].nInputs;
    //FIXME need to add to builtin effect if needed
    //e->nOutPorts
    e->mixType = MIX_REPLACE;  //MIX_ADD;
    e->mixvaldb = 0.0f;

    for (i = 0;  i < builtin_effects[id].nInputs;  i++) {
        e->ihasmin[i] = builtin_effects[id].iHasMin[i];
        if (e->ihasmin[i]) {
            e->imin[i] = builtin_effects[id].iMin[i];
        } else {
            e->imin[i] = -FLT_MAX;
        }

        e->ihasmax[i] = builtin_effects[id].iHasMax[i];
        if (e->ihasmax[i]) {
            e->imax[i] = builtin_effects[id].iMax[i];
        } else {
            e->imax[i] = FLT_MAX;
        }

        e->iDefaultVal[i] = builtin_effects[id].iDefault[i];
    }

    strncpy(e->label, builtin_effects[id].description, MAX_CHARS);
    strncpy(e->pluginName, builtin_effects[id].label, MAX_CHARS);
    return e;
}

static void gui_add_ladspa_effect (effect_t *e, int pos)
{
    int i;
    gui_effect_t *ge;


    if (e->type != EFFECT_LADSPA) {
        xerror("e->type != EFFECT_LADSPA");
    }

    ge = gui_effect_new(e, pos);
    gui_add_label(ge, e->pdesc->Name);

    for (i = 0;  i < e->nInPorts;  i++) {
        if (e->itoggled[i]) {
            gui_add_bool_toggle(ge, e->pdesc->PortNames[e->inputToPort[i]], &e->ifval[i], i);
        } else {
            gui_add_entry_float(ge, e->pdesc->PortNames[e->inputToPort[i]], &e->ifval[i], i);
        }
    }
    for (i = 0;  i < e->nOutPorts;  i++) {
        //FIXME gui needs to update this
        gui_add_output(ge, e->pdesc->PortNames[e->outputToPort[i]], &e->ofval[i]);
    }
}

static void gui_add_builtin_effect (effect_t *e, int pos)
{
    int i;
    gui_effect_t *ge;
    int numStrings;

    if (e->type != EFFECT_BUILTIN) {
        xerror("e->type != EFFECT_BUILTIN");
    }

    ge = gui_effect_new(e, pos);
    gui_add_label(ge, builtin_effects[e->id].description);

    numStrings = 0;
    for (i = 0;  i < builtin_effects[e->id].nInputs;  i++) {
        switch(builtin_effects[e->id].inputType[i]) {
        case INPUT_FLOAT: {
            gui_add_entry_float(ge, builtin_effects[e->id].inputNames[i], &e->ifval[i], i);
            break;
        }
        case INPUT_STRING: {
            //FIXME check num strings, or dynamic
            gui_add_entry_string(ge, builtin_effects[e->id].inputNames[i], e->strings[numStrings]);
            numStrings++;
            break;
        }
        case INPUT_FILE: {
            //FIXME check num strings, or dynamic
            gui_add_entry_file(ge, builtin_effects[e->id].inputNames[i], e->strings[numStrings]);
            numStrings++;
            break;
        }
        case INPUT_BOOL: {
            gui_add_bool_toggle(ge, builtin_effects[e->id].inputNames[i], &e->ifval[i], i);
            break;
        }
        default:
            xerror();
        }
    }
}

static void prepend_effect_button_cb (GtkWidget *button, gpointer data)
{
    plugin_list_t *p = (plugin_list_t *)data;
    effect_t *e;

    switch (p->type) {
    case EFFECT_BUILTIN:
        e = builtin_effect_new(p->id);
        effect_prepend_to_bank(e, currentEffectBank);
        gui_add_builtin_effect(e, 0);
        break;
    case EFFECT_LADSPA: {
        e = ladspa_effect_new(p->libname, p->name);
        if (!e) {
            fprintf(stderr, "couldn't add ladspa plugin %s\n", p->name);
            return;
        }

        effect_prepend_to_bank(e, currentEffectBank);
        gui_add_ladspa_effect(e, 0);
        break;
    }
    default: {
        fprintf(stderr, "unknown effect type\n");
        break;
    }
    }
}

static void add_effect_button_cb (GtkWidget *button, gpointer data)
{
    plugin_list_t *p = (plugin_list_t *)data;
    effect_t *e;

    switch (p->type) {
    case EFFECT_BUILTIN:
        e = builtin_effect_new(p->id);
        effect_append_to_bank(e, currentEffectBank);
        gui_add_builtin_effect(e, -1);
        break;
    case EFFECT_LADSPA: {
        e = ladspa_effect_new(p->libname, p->name);
        if (!e) {
            fprintf(stderr, "couldn't add ladspa plugin %s\n", p->name);
            return;
        }

        effect_append_to_bank(e, currentEffectBank);
        gui_add_ladspa_effect(e, -1);
        break;
    }
    default: {
        fprintf(stderr, "unknown effect type\n");
        break;
    }
    }
}

static void write_config_button_cb (GtkWidget *button, gpointer data)
{
    effect_t *e;
    FILE *f;
    int i;
    //FIXME file to write to
    //FIXME other things besides fx

    f = fopen("gtrfx-config", "w");  //FIXME
    if (!f) {
        printf("couldn't open config file\n");
        return;
    }

    if (optionMute) {
        fprintf(f, "option mute 1\n");
    }
    if (optionClampLadspaInput) {
        fprintf(f, "option clampLadspaInput 1\n");
    }

    //FIXME other effect banks
    for (e = effects[currentEffectBank];  e != NULL;  e = e->next) {
        fprintf(f, "fx");
        if (e->on)
            fprintf(f, " on");
        else
            fprintf(f, " off");

        if (e->mixType == MIX_REPLACE)
            fprintf(f, " replace");
        else
            fprintf(f, " add %f", e->mixvaldb);

        if (e->type == EFFECT_BUILTIN) {
            fprintf(f, " builtin %s", e->pluginName);
            for (i = 0;  i < builtin_effects[e->id].nInputs;  i++) {
                if (builtin_effects[e->id].inputType[i] == INPUT_FLOAT) {
                    fprintf(f, " %f", e->ifval[i]);
                } else if (builtin_effects[e->id].inputType[i] == INPUT_BOOL) {
                    fprintf(f, " %f", e->ifval[i]);
                } else if (builtin_effects[e->id].inputType[i] == INPUT_STRING) {
                    //FIXME more than one string
                    fprintf(f, " %s", e->strings[0]);
                } else if (builtin_effects[e->id].inputType[i] == INPUT_FILE) {
                    //FIXME more than one string
                    fprintf(f, " %s", e->strings[0]);
                }
            }
        } else if (e->type == EFFECT_LADSPA) {
            fprintf(f, " ladspa %s %s", e->libFname, e->pluginName);
            for (i = 0;  i < e->nInPorts;  i++) {
                fprintf(f, " %f", e->ifval[i]);
            }
        }

        fprintf(f, "\n");
    }

    fclose(f);
}

static void clamp_ladspa_input_button_cb (GtkWidget *button, gpointer data)
{
    optionClampLadspaInput = !optionClampLadspaInput;
    printf("clamp ladspa input:  %d\n", optionClampLadspaInput);
}

static void mute_button_cb (GtkWidget *button, gpointer data)
{
    optionMute = !optionMute;
    printf("mute:  %d\n", optionMute);
}

static void tuner_button_cb (GtkWidget *button, gpointer data)
{
    optionTuner = !optionTuner;
    printf("Tuner:  %d\n", optionTuner);
}

static void effect_list_window_destroy_cb (GtkWidget *widget, gpointer data)
{
    gui.effectListWindow = NULL;
}

static void add_to_effect_list_window_vbox (GtkWidget *vbox, plugin_list_t *p)
{
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *button;
    GtkWidget *sep;
    GdkColor color;

    gdk_color_parse("dark red", &color);

        label = gtk_label_new(p->description);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        button = gtk_button_new_with_label("prepend");
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(prepend_effect_button_cb), p);

        button = gtk_button_new_with_label("add");
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(add_effect_button_cb), p);

        sep = gtk_vseparator_new();
        gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

        label = gtk_label_new(p->name);
        gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &color);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 20);

        sep = gtk_vseparator_new();
        gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

        label = gtk_label_new(p->libname);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        sep = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
}

static gint compare_plugin_names (gconstpointer a, gconstpointer b)
{
    plugin_list_t *ap = (plugin_list_t *) a;
    plugin_list_t *bp = (plugin_list_t *) b;
    int r;

    r = strcasecmp(ap->name, bp->name);

    return r;
}

static void effect_list_window_button_cb (GtkWidget *cb_button, gpointer data)
{
    GtkWidget *swin;
    GtkWidget *vbox;
    GtkWidget *sep;
    plugin_list_t *p;
    plugin_list_t *px;
    int i;
    GSList *gslist = NULL;
    GSList *g;

    if (gui.effectListWindow) {
        gtk_window_present(GTK_WINDOW(gui.effectListWindow));
        return;
    }

    gui.effectListWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui.effectListWindow), "effect list");
    gtk_widget_set_usize(GTK_WIDGET(gui.effectListWindow), 600, 500);
    gtk_signal_connect(GTK_OBJECT(gui.effectListWindow), "destroy", GTK_SIGNAL_FUNC(effect_list_window_destroy_cb), NULL);

    swin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(gui.effectListWindow), swin);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), vbox);

    gslist = NULL;
    for (p = plugins;  p != NULL;  p = p->next) {
        if (p->type != EFFECT_BUILTIN)
            break;
        gslist = g_slist_insert_sorted(gslist, p, compare_plugin_names);
        //add_to_effect_list_window_vbox(vbox, p);
    }
    px = p;
    for (g = gslist;  g != NULL;  g = g->next) {
        add_to_effect_list_window_vbox(vbox, (plugin_list_t *)g->data);
    }
    g_slist_free(gslist);


    for (i = 0;  i < 20;  i++) {
        sep = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    }


    gslist = NULL;

    for (p = px;  p != NULL;  p = p->next) {
        gslist = g_slist_insert_sorted(gslist, p, compare_plugin_names);
    }

    for (g = gslist;  g != NULL;  g = g->next) {
        add_to_effect_list_window_vbox(vbox, (plugin_list_t *)g->data);
    }

    g_slist_free(gslist);

    gtk_widget_show_all(gui.effectListWindow);
}

int main (int argc, char *argv[])
{
    gtkthread_t args;
    GError *error;
    GtkWidget *w;
    GtkWidget *bin;
    GtkWidget *wid;
    GtkWidget *sep;
    GtkWidget *h;
    int i;
    visuals_t vis;

    g_thread_init(NULL);
    gdk_threads_init();
    //gdk_threads_enter();

    // hmm
    gtk_init(&argc, &argv);
    gdk_rgb_init();

    effects_lock = g_mutex_new();
    if (!effects_lock) {
        xerror("couldn't create effects lock");
    }


    for (i = 0;  i < FREQ_TABLE_MAX;  i++) {
        freq_table[i] = 27.5 * powf(2, i / 12.0);
        printf("%d  %f\n", i, freq_table[i]);
    }

    numBuiltinEffects = sizeof(builtin_effects) / sizeof(builtin_effect_t);
    for (i = 0;  i < numBuiltinEffects;  i++) {
        builtin_effects[i].init(&builtin_effects[i]);
    }

#if 0
    rgb_buffer = g_malloc(3 * rgb_width * rgb_height);
    if (!rgb_buffer) {
        xerror("couldn't allocate rgb_buffer memory");
    }
#endif
    vis.oscil_in.darea = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(vis.oscil_in.darea), OSCIL_WIDTH, OSCIL_HEIGHT);
    vis.oscil_in.width = OSCIL_WIDTH;
    vis.oscil_in.height = OSCIL_HEIGHT;
    vis.oscil_in.rgb_buffer = g_malloc(3 * OSCIL_WIDTH * OSCIL_HEIGHT);
    if (!vis.oscil_in.rgb_buffer) {
        xerror();
    }

    vis.oscil_out.darea = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(vis.oscil_out.darea), OSCIL_WIDTH, OSCIL_HEIGHT);
    vis.oscil_out.width = OSCIL_WIDTH;
    vis.oscil_out.height = OSCIL_HEIGHT;
    vis.oscil_out.rgb_buffer = g_malloc(3 * OSCIL_WIDTH * OSCIL_HEIGHT);
    if (!vis.oscil_out.rgb_buffer) {
        xerror();
    }


    gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui.window), "gtrfx");
#if 0
    snprintf(st, MAX_CHARS, "gtrfx %s", title);
    printf ("st:  %s\n", st);
    gtk_window_set_title(GTK_WINDOW(gui.window), st);
#endif
    gtk_widget_set_usize(gui.window, 700, 700);
    gtk_signal_connect(GTK_OBJECT(gui.window), "destroy", GTK_SIGNAL_FUNC(destroy_cb), NULL);

    h = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gui.window), h);
    sep = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(h), sep, FALSE, FALSE, 0);

    w = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), w, TRUE, TRUE, 0);

    wid = gtk_button_new_with_label("test");
    gtk_box_pack_start(GTK_BOX(w), wid, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(wid), "clicked", GTK_SIGNAL_FUNC(test_cb), NULL);

    bin = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(w), bin, FALSE, FALSE, 0);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(w), sep, FALSE, FALSE, 0);

    //////////////////////////////
    //gui.darea = gtk_drawing_area_new();
    //gtk_drawing_area_size(GTK_DRAWING_AREA(gui.darea), rgb_width, rgb_height);

    //gtk_box_pack_start(GTK_BOX(bin), gui.darea, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bin), vis.oscil_in.darea, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(bin), vis.oscil_out.darea, FALSE, FALSE, 5);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(w), sep, FALSE, FALSE, 0);

    h = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(w), h, FALSE, FALSE, 0);

    gui.addEffectButton = gtk_button_new_with_label("add effect");
    gtk_box_pack_start(GTK_BOX(h), gui.addEffectButton, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(gui.addEffectButton), "clicked", GTK_SIGNAL_FUNC(effect_list_window_button_cb), NULL);

    gui.writeConfigButton = gtk_button_new_with_label("write config");
    gtk_box_pack_start(GTK_BOX(h), gui.writeConfigButton, FALSE, FALSE, 10);
    gtk_signal_connect(GTK_OBJECT(gui.writeConfigButton), "clicked", GTK_SIGNAL_FUNC(write_config_button_cb), NULL);

    gui.clampLadspaInputButton = gtk_button_new_with_label("clamp ladspa input");
    gtk_box_pack_start(GTK_BOX(h), gui.clampLadspaInputButton, FALSE, FALSE, 10);
    gtk_signal_connect(GTK_OBJECT(gui.clampLadspaInputButton), "clicked", GTK_SIGNAL_FUNC(clamp_ladspa_input_button_cb), NULL);

    gui.tunerButton = gtk_button_new_with_label("tuner");
    gtk_box_pack_start(GTK_BOX(h), gui.tunerButton, FALSE, FALSE, 10);
    gtk_signal_connect(GTK_OBJECT(gui.tunerButton), "clicked", GTK_SIGNAL_FUNC(tuner_button_cb), NULL);

    gui.muteButton = gtk_button_new_with_label("mute");
    gtk_box_pack_start(GTK_BOX(h), gui.muteButton, FALSE, FALSE, 10);
    gtk_signal_connect(GTK_OBJECT(gui.muteButton), "clicked", GTK_SIGNAL_FUNC(mute_button_cb), NULL);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(w), sep, FALSE, FALSE, 0);

    gui.swindow = gtk_scrolled_window_new(NULL, NULL);

    gtk_box_pack_start(GTK_BOX(w), gui.swindow, TRUE, TRUE, 0);

    gui.effectVbox = gtk_vbox_new(FALSE, 0);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(gui.swindow), gui.effectVbox);


    gtk_widget_show_all(gui.window);
    //FIXME this might depend on sample rate
    gtk_timeout_add(40, draw_stuff, (gpointer)&vis);

    scan_plugins();

    args.argc = argc;
    args.argv = argv;
    if (!g_thread_create(gtrfx_main_thread, &args, FALSE, &error)) {
        g_printerr ("Failed to create dsp thread: %s\n", error->message);
        exit(1);
    }

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    return 0;
}
