#include <stdlib.h>
#include <string.h>
#include <l4darr.h>

L4DA* l4da_create_setmax(int itemsize, int len, int maxlen){
	if(itemsize <= 0 || len < 0 || maxlen < len){
		return NULL;
	}
	L4DA* arr = (L4DA*)malloc(sizeof(L4DA));
	if(arr == NULL){
		return NULL;
	}
	arr->arr_itemsize = itemsize;
	arr->arr_curlen = len;
	arr->arr_maxlen = maxlen;
	if(maxlen != 0){
		arr->arr_data = malloc(itemsize * maxlen);
		if(arr->arr_data == NULL){
			free(arr);
			return NULL;
		}
	}else{
		arr->arr_data = NULL;
	}
	return arr;
}

L4DA* l4da_create(int itemsize, int len){
	return l4da_create_setmax(itemsize, len, len);
}

void l4da_free(L4DA* arr){
	if(arr->arr_data != NULL){
		free(arr->arr_data);
	}
	free(arr);
}

int l4da_pushback(L4DA* arr, const void* data){
	if((arr->arr_maxlen) < (arr->arr_curlen + 1)){
		if(arr->arr_maxlen != 0){
			if(l4da_setmax(arr, arr->arr_maxlen*2) < 0){
				return -1;
			}
		}else{
			if(l4da_setmax(arr, 1) < 0){
				return -1;
			}
		}
	}
	memcpy(l4da_vp(arr, arr->arr_curlen), data, arr->arr_itemsize);
	arr->arr_curlen++;
	return 0;
}

int l4da_setlen(L4DA* arr, int len){
	if(len > (arr->arr_maxlen)){
		if(l4da_setmax(arr, len) < 0){
			return -1;
		}else{
			arr->arr_curlen = len;
		}
	}else{
		arr->arr_curlen = len;
		return 0;
	}
	return 0;
}

int l4da_setmax(L4DA* arr, int max){
	void* newptr;
	if(arr->arr_data == NULL){
		newptr = malloc((arr->arr_itemsize)*max);
		if(newptr == NULL){
			return -1;
		}else{
			arr->arr_maxlen = max;
			arr->arr_data = newptr;
			return 0;
		}
	}
	newptr = realloc(arr->arr_data, (arr->arr_itemsize)*max);
	if(newptr == NULL){
		return -1;
	}else{
		arr->arr_maxlen = max;
		arr->arr_data = newptr;
	}
	return 0;
}

int l4da_strip(L4DA* arr){
	if(arr->arr_data == NULL){
		return 0;
	}
	L4DA* newptr; 
	/* 其實縮小空間營該一定會成功才對......
	 * 不過還是保險一點，加個判斷式，別說 memory leak 是我害的
	 * 當然也是避免編譯器一直跳 warning
	 */
	if((arr->arr_maxlen) > (arr->arr_curlen)){
		arr->arr_maxlen = arr->arr_curlen;
		newptr = realloc(arr->arr_data, (arr->arr_curlen)*(arr->arr_itemsize));
		if(newptr == NULL){
			return -1;
		}
		arr->arr_data = newptr;
	}
	return 0;
}

/* 基本上直接玩 struct 的函式還是少用吧 */
void* l4da_drop_struct(L4DA* arr){
	void* toreturn = arr->arr_data;
	free(arr);
	return toreturn;
}

L4DA* l4da_make_struct(void* data, int itemsize, int len, int maxlen){
	if(itemsize <= 0 || len < 0 || maxlen < len){
		return NULL;
	}
	L4DA* arr = (L4DA*)malloc(sizeof(L4DA));
	if(arr == NULL){
		return NULL;
	}
	arr->arr_itemsize = itemsize;
	arr->arr_curlen = len;
	arr->arr_maxlen = maxlen;
	arr->arr_data = data;
	return arr;
}
