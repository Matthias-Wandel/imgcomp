extern char * progname;  // program name for error messages
extern char DoDirName[200];
extern char SaveDir[200];
extern char SaveNames[200];
extern int FollowDir;
extern int ScaleDenom;
extern int SpuriousReject;
extern int PreMotionKeep;
extern int PostMotionKeep;

extern int BrightnessChangeRestart;
extern int MotionFatigueTc;
extern int FatigueGainPercent;
extern int FatigueSkipCount;

extern char DiffMapFileName[200];
extern Regions_t Regions;

extern int Verbosity;
extern char LogToFile[200];
extern char MoveLogNames[200];

extern int Sensitivity;
extern int Raspistill_restarted;
extern int TimelapseInterval;
extern char raspistill_cmd[200];
extern char blink_cmd[200];
extern char UdpDest[30];
extern int wait_close_write;
extern int relaunch_timeout;
extern int give_up_timeout;

void usage (void);// complain about bad command line 
void read_config_file(char *name);
int parse_switches (int argc, char **argv, int last_file_arg_seen);
