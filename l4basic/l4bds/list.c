#include <stdlib.h>
#include <string.h>
#include <l4bds.h>

L4LL* l4ll_create(void){
	L4LL* newlist = (L4LL*)malloc(sizeof(L4LL));
	if(newlist == NULL){
		return NULL;
	}
	newlist->list_first = NULL;
	newlist->list_last = NULL;
	newlist->list_len = 0;
	return newlist;
}

void l4ll_free(L4LL* oldlist){
	if(oldlist->list_len <= 0){
		free(oldlist);
		return;
	}
	L4LLNODE* i;
	L4LLNODE* next;
	for(i=oldlist->list_first; i!=NULL; i=next){
		next = i->node_next;
		if(i->node_data_size){ /* 只要資料大小不為零，就還要釋放資料區 */
			free(i->node_data);
		}
		free(i);
	}
	free(oldlist);
}

static L4LLNODE* 
l4ll_initfirst(L4LL* list, const void* data, int size){
	/* 插入第一個資料，如果 list 不是空的就別用！
	 * 否則會造成資料結構混亂和 memory leak */
	L4LLNODE* node = (L4LLNODE*)malloc(sizeof(L4LLNODE));
	if(node == NULL){
		return NULL;
	}
	void* newdata;
   	if(size != 0){
		newdata = malloc(size);
		if(newdata == NULL){
			free(node);
			return NULL;
		}
		memcpy(newdata, data, size);
	}
	list->list_first = node;
	list->list_last = node;
	list->list_len = 1;
	node->node_prev = NULL;
	node->node_next = NULL;
	node->node_data = (size != 0) ? newdata : NULL;
	node->node_data_size = size;
	return node;
}

L4LLNODE* l4ll_insert_prev(L4LL* list, L4LLNODE* node, 
		const void* data, int size){
	/* list 送 NULL 來的我不理它，就自己去 segfault 吧
	 * node 送 NULL 來只能在 list 是空的時候用 */
	if(list->list_len == 0){
		return l4ll_initfirst(list, data, size);
	}
	L4LLNODE* newnode = (L4LLNODE*)malloc(sizeof(L4LLNODE));
	if(newnode == NULL){
		return NULL;
	}
	void* newdata;
	if(size != 0){
		newdata = malloc(size);
		if(newdata == NULL){
			free(newnode);
			return NULL;
		}
		memcpy(newdata, data, size);
	}
	list->list_len++;
	if(list->list_first == node){ /* 如果是第一個，那要修改 list_first */
		list->list_first = newnode;
	}
	L4LLNODE* oldprev = node->node_prev;
	node->node_prev = newnode;
	newnode->node_next = node;
	newnode->node_prev = oldprev;
	if(oldprev != NULL){
		oldprev->node_next = newnode;
	}
	newnode->node_data = (size != 0) ? newdata : NULL;
	newnode->node_data_size = size;
	return newnode;
}

L4LLNODE* l4ll_insert_next(L4LL* list, L4LLNODE* node, 
		const void* data, int size){
	/* 基本上同 l4ll_insert_prev */
	if(list->list_len == 0){
		return l4ll_initfirst(list, data, size);
	}
	L4LLNODE* newnode = (L4LLNODE*)malloc(sizeof(L4LLNODE));
	if(newnode == NULL){
		return NULL;
	}
	void* newdata;
	if(size != 0){
		newdata = malloc(size);
		if(newdata == NULL){
			free(newnode);
			return NULL;
		}
		memcpy(newdata, data, size);
	}
	list->list_len++;
	if(list->list_last == node){
		list->list_last = newnode;
	}
	L4LLNODE* oldnext = node->node_next;
	node->node_next = newnode;
	newnode->node_prev = node;
	newnode->node_next = oldnext;
	if(oldnext != NULL){
		oldnext->node_prev = newnode;
	}
	newnode->node_data = (size != 0) ? newdata : NULL;
	newnode->node_data_size = size;
	return newnode;
}

void l4ll_remove(L4LL* list, L4LLNODE* node){
	/* 不要在 node 傳 NULL，這一點意義也沒有且我不知道會發生什麼事 */
	if(list->list_first == node){
		list->list_first = node->node_next;
	}
	if(list->list_last == node){
		list->list_last = node->node_prev;
	}
	list->list_len--; /* 不考慮長度為零的情況，空白是想要刪什麼？ */
	L4LLNODE* oldnext = node->node_next;
	L4LLNODE* oldprev = node->node_prev;
	if(node->node_data_size != 0){
		free(node->node_data);
	}
	free(node);
	if(oldnext != NULL){
		oldnext->node_prev = oldprev;
	}
	if(oldprev != NULL){
		oldprev->node_next = oldnext;
	}
}

L4LLNODE* l4ll_goto(L4LLNODE* node, int count){
	int i;
	if(count == 0){
		return node;
	}else if(count > 0){
		for(i=1; i<=count; i++){
			node = node->node_next;
			if(node == NULL){
				return NULL;
			}
		}
	}else{
		count = -count;
		for(i=1; i<=count; i++){
			node = node->node_prev;
			if(node == NULL){
				return NULL;
			}
		}
	}
	return node;
}

#if 0
int l4ll_pushback(L4LL* list, void* data, int size){
	L4LLNODE* cur_save = list->list_current; /* 等一下要把現在位置搬回去 */
	list->list_current = list->list_last;
	int result = l4ll_insnext(list, data, size);
	if(result == 0){ /* 成功 */
		if(cur_save != NULL){ /* 原先的現在位置若為空就無意義了 */
			list->list_current = cur_save;
		}
	}else{ /* 失敗 */
		list->list_current = cur_save;
	}
	return result;
}


int l4ll_pushfront(L4LL*, void*){ /* 取自 l4ll_pushback */
	L4LLNODE* cur_save = list->list_current; 
	list->list_current = list->list_front;
	int result = l4ll_insprev(list, data, size);
	if(result == 0){
		if(cur_save != NULL){ 
			list->list_current = cur_save;
		}
	}else{ 
		list->list_current = cur_save;
	}
	return result;
}

int l4ll_popback(L4LL*){
	L4LLNODE* cur_save = list->list_current;
	L4LLNODE* last_save = list->list_last
	list->list_current = last_save;
	int result = l4ll_remove(list, L4LL_PREV);
	if(result == 0){ 
		if(cur_save != NULL && ){ 
			list->list_current = cur_save;
		}
	}else{ 
		list->list_current = cur_save;
	}
	return result;
}

int l4ll_popfront(L4LL*){
	L4LLNODE* cur_save = list->list_current;
	list->list_current = list->list_first;
	int result = l4ll_remove(list, L4LL_NEXT);
	if(result == 0){ 
		if(cur_save != NULL){ 
			list->list_current = cur_save;
		}
	}else{ 
		list->list_current = cur_save;
	}
	return result;
}
#endif
