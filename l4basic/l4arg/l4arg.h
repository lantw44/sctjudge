#ifndef L4LIB_ARG_PARSER
#define L4LIB_ARG_PARSER

#ifdef __cplusplus
extern "C" {
#endif

char** l4arg_toargv(const char*, const char*, const char*, const char*);
void l4arg_toargv_free(char**);

typedef struct l4lib_qarg {
	char* arg_name;
	char* arg_value;
} L4QARG;

L4QARG* l4qarg_parse(const char*);
void l4qarg_free(L4QARG*);
#define l4qarg_n(qargitem) ((qargitem).arg_name)
#define l4qarg_v(qargitem) ((qargitem).arg_value)
#define l4qarg_hasvalue(qargitem) (((qargitem).arg_value != NULL))
#define l4qarg_end(qargitem) \
	((((qargitem).arg_name) == NULL) && (((qargitem).arg_value) == NULL))

#ifdef __cplusplus
}
#endif

#endif
