#ifndef L4LIB_DYNAMIC_ARRAY
#define L4LIB_DYNAMIC_ARRAY

#include <stdio.h> /* 取得 FILE */

/*********** 一維陣列 ***********/

typedef struct l4lib_dyn_arr{
	int arr_itemsize;		/* 每個項目的大小 */
	int arr_curlen;			/* 陣列總長度 */
	int arr_maxlen;			/* 陣列最大長度 */
	void* arr_data;			/* 資料區 */
} L4DA ;

L4DA* l4da_create_setmax(int, int, int);
L4DA* l4da_create(int, int);
void l4da_free(L4DA*);
int l4da_pushback(L4DA*, const void*);
#define l4da_popback(arr) (((arr)->arr_curlen)--)
#define l4da_getlen(arr) ((arr)->arr_curlen)
int l4da_setlen(L4DA*, int);
#define l4da_getmax(arr) ((arr)->arr_maxlen)
int l4da_setmax(L4DA*, int);
int l4da_strip(L4DA*);
#define l4da_itemsize(arr) ((arr)->arr_itemsize)
#define l4da_v(arr, type, num) \
	(*(((type*)((arr)->arr_data))+(num)))
#define l4da_vp(arr, num) \
	((void*)(((char*)((arr)->arr_data))+(((arr)->arr_itemsize)*(num))))

#define l4da_readline (l4da_filereadline_delim(stdin, '\n'))
#define l4da_readline_delim(delim) (l4da_filereadline_delim(stdin, (delim)))
#define l4da_filereadline(infile) (l4da_filereadline_delim((infile), '\n'))
L4DA* l4da_filereadline_delim(FILE*, int);

L4DA* l4da_dup(const L4DA*);
int l4da_combine(L4DA*, const L4DA*);

void* l4da_drop_struct(L4DA*);
L4DA* l4da_make_struct(void*, int, int, int);

/*********** 二維陣列 (其實是用一維陣列來模擬，功能有限) ***********/

typedef struct l4lib_dyn_2darr{
	int arr_itemsize;		/* 每個項目的大小 */
	int arr_lenx;	 	 	/* 陣列 x 方向長度 */
	int arr_leny;  			/* 陣列 y 方向長度 */
	void* arr_data; 	   	/* 資料區 */
} L4DA2 ;

L4DA2* l4da2_create(int, int, int);
void l4da2_free(L4DA2*);
#define l4da2_getlenx(arr) ((arr)->arr_lenx)
#define l4da2_getleny(arr) ((arr)->arr_leny)
#define l4da2_itemsize(arr) ((arr)->arr_itemsize)
#define l4da2_v(arr, type, numx, numy) \
	(*(((type*)((arr)->arr_data))+((numx)*(l4da2_getleny(arr)))+(numy)))
#define l4da2_vp(arr, numx, numy) \
	((void*)(((char*)((arr)->arr_data))+ \
	((arr)->arr_itemsize)*((numx)*(l4da2_getleny(arr))+(numy))))

#endif
