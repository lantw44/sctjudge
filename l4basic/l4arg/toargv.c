#include <string.h>
#include <stdlib.h>
#include <l4arg.h>
#include <l4darr.h>

/* 基本上優先順序是 escape -> quoting -> delimiter */

#define abort_l4arg_toargv \
	do{ \
		l4da_free(parr); \
		l4da_free(tmpstr); \
		return NULL; \
	}while(0)

char** l4arg_toargv(const char* str, 
		const char* delim, const char* quoting, const char* esc){
	int i;
	char escaped = 0, quoted = 0, delimed = 0;
	L4DA* parr;
	L4DA* tmpstr;
	char* addstr, tmpchar;
	char** rval;
	parr = l4da_create(sizeof(char*), 0);
	if(parr == NULL){
		return NULL;
	}
	tmpstr = l4da_create(sizeof(char), 0);
	if(tmpstr == NULL){
		l4da_free(parr);
		return NULL;
	}
	for(i=0; str[i]!='\0'; i++){
		if(escaped){
			if(l4da_pushback(tmpstr, &str[i]) < 0){
				abort_l4arg_toargv;
			}
			escaped = 0;
			delimed = 0;
			continue;
		}
		if(quoted){
			if(strchr(quoting, str[i]) != NULL){
				quoted = 0;
				continue;
			}
			if(l4da_pushback(tmpstr, &str[i]) < 0){
				abort_l4arg_toargv;
			}
			delimed = 0;
			continue;
		}
		if(strchr(esc, str[i]) != NULL){
			escaped = 1;
			continue;
		}
		if(strchr(quoting, str[i]) != NULL){
			quoted = 1;
			continue;
		}
		if(strchr(delim, str[i]) != NULL){
			if(l4da_getlen(tmpstr) > 0){
				tmpchar = '\0';
				if(l4da_pushback(tmpstr, &tmpchar) < 0){
					abort_l4arg_toargv;
				}
				addstr = (char*)l4da_drop_struct(tmpstr);
				if(l4da_pushback(parr, &addstr) < 0){
					l4da_free(parr);
					return NULL;
				}
				tmpstr = l4da_create(sizeof(char), 0);
				if(tmpstr == NULL){
					l4da_free(parr);
					return NULL;
				}
			}
			delimed = 1;
			continue;
		}
		if(l4da_pushback(tmpstr, &str[i]) < 0){
			abort_l4arg_toargv;
		}
		delimed = 0;
	}
	if(!delimed){
		tmpchar = '\0';
		if(l4da_pushback(tmpstr, &tmpchar) < 0){
			abort_l4arg_toargv;
		}
		addstr = (char*)l4da_drop_struct(tmpstr);
		if(l4da_pushback(parr, &addstr) < 0){
			l4da_free(parr);
			return NULL;
		}
	}
	addstr = NULL;
	if(l4da_pushback(parr, &addstr) < 0){
		l4da_free(parr);
		return NULL;
	}
	rval = (char**)l4da_drop_struct(parr);
	return rval;
}

void l4arg_toargv_free(char** pargv){
	int i;
	for(i=0; pargv[i]!=NULL; i++){
		free(pargv[i]);
	}
	free(pargv);
}
