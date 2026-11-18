/* mac_launch.h -- the parsed launch (mac_launch.c). */
#ifndef MAC_LAUNCH_H
#define MAC_LAUNCH_H

typedef struct {
    const char *segs, *bundle, *layer_dir;
    const char *support_dir, *save_dir, *mods_dir, *modscan_path, *gate_log_path;
    const char *stage;
    int vanilla, debug_console, lua_debug;
    int positional_save;             /* the 4th positional argument was a save dir */
    const char *launch_settings;
} mac_cfg;

extern mac_cfg g_mac_cfg;
int mac_launch_parse(int argc, char **argv);
int mac_guest_argc(void);
const char *mac_guest_argv(int i);

#endif
