#include <stdlib.h>
#include <l4darr.h>

L4DA2* l4da2_create(int itemsize, int lenx, int leny){
	if(lenx <= 0 || leny <= 0 || itemsize <= 0){
		return NULL;
	}
	L4DA2* arr = (L4DA2*)malloc(sizeof(L4DA2));
	if(arr == NULL){
		return NULL;
	}
	arr->arr_itemsize = itemsize;
	arr->arr_lenx = lenx;
	arr->arr_leny = leny;
	arr->arr_data = malloc(itemsize*lenx*leny);
	if(arr->arr_data == NULL){
		free(arr);
		return NULL;
	}
	return arr;
}

void l4da2_free(L4DA2* arr){
	free(arr->arr_data);
	free(arr);
}
