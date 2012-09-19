#include <stdlib.h>
#include <string.h>
#include <l4arg.h>

/* 為什麼叫做 qarg 呢？因為這是用來解析很像 QEMU 命令列參數的參數 */

L4QARG* l4qarg_parse(const char* str){
	char** pargv = l4arg_toargv(str, ",", "\"\'", "\\");
	if(pargv == NULL){
		return NULL;
	}
	int i, allc;
	L4QARG* qargarr;
	char* pos;
	for(i=0; pargv[i]!=NULL; i++);
	allc = i + 1;
	qargarr = (L4QARG*) malloc(sizeof(L4QARG) * allc);
	if(qargarr == NULL){
		l4arg_toargv_free(pargv);
		return NULL;
	}
	for(i=0; pargv[i]!=NULL; i++){
		pos = strchr(pargv[i], '=');
		if(pos == NULL){
			qargarr[i].arg_name = pargv[i];
			qargarr[i].arg_value = NULL;
		}else{
			*pos = '\0';
			qargarr[i].arg_name = pargv[i];
			pos++;
			qargarr[i].arg_value = (char*) malloc(strlen(pos)+1);
			if(qargarr[i].arg_value == NULL){
				l4arg_toargv_free(pargv);
				return NULL;
			}
			strcpy(qargarr[i].arg_value, pos);
		}
	}
	free(pargv);
	qargarr[i].arg_name = NULL;
	qargarr[i].arg_value = NULL;
	return qargarr;
}

void l4qarg_free(L4QARG* qarg){
	int i;
	for(i=0; !(qarg[i].arg_name == NULL && qarg[i].arg_value == NULL); i++){
		free(qarg[i].arg_name);
		if(qarg[i].arg_value != NULL){
			free(qarg[i].arg_value);
		}
	}
	free(qarg);
}
