/* rtcmix_parse.h */
#ifdef __cplusplus
extern "C" {
#endif

#define MAXARGS   32

int parse_score(int argc, char *argv[]);
int perl_parse_buf (char *inBuf);
void use_script_file(char *fname);
void destroy_parser(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
