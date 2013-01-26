#include <string.h>
#include <l4darr.h>

L4DA* l4da_dup(const L4DA* arr){
	L4DA* newarr = l4da_create_setmax(
			l4da_itemsize(arr), l4da_getlen(arr), l4da_getmax(arr));
	if(newarr == NULL){
		return NULL;
	}
	memcpy(newarr->arr_data, arr->arr_data, 
			l4da_getlen(arr) * l4da_itemsize(arr));
	return newarr;
}

int l4da_combine(L4DA* arr, const L4DA* att){
	if(l4da_itemsize(arr) != l4da_itemsize(att)){
		return -2;
	}
	if(l4da_setlen(arr, l4da_getlen(arr) + l4da_getlen(att)) < 0){
		return -1;
	}
	memcpy(l4da_vp(arr, l4da_getlen(arr)), att->arr_data,
			l4da_getlen(att) * l4da_itemsize(att));
	return 0;
}

L4DA* l4da_filereadline_delim(FILE* infile, int chr){
	L4DA* newarr = l4da_create(1, 0);
	if(newarr == NULL){
		return NULL;
	}
	int c;
	char towrite;
	while((c = getc(infile)) != chr && !feof(infile)){
		towrite = c;
		if(l4da_pushback(newarr, (void*)&towrite) < 0){
			l4da_free(newarr);
			return NULL;
		}
	}
	towrite = '\0';
	if(l4da_pushback(newarr, (void*)&towrite) < 0){
		l4da_free(newarr);
		return NULL;
	}
	return newarr;
}
