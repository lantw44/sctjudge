#ifndef L4LIB_BASIC_DATA_STRUCTURE
#define L4LIB_BASIC_DATA_STRUCTURE

/*********** list ***********/

typedef struct l4lib_list_node{ /* list 中每一個項目用的資料結構，會有很多個 */
	struct l4lib_list_node* node_prev;
	struct l4lib_list_node* node_next;
	void* node_data;
	int node_data_size;
} L4LLNODE;

typedef struct l4lib_list{ /* 管理 list 用的，每個 list 只有一個 */
	struct l4lib_list_node* list_first;
	struct l4lib_list_node* list_last;
	int list_len;
} L4LL;

L4LL* l4ll_create(void);
void l4ll_free(L4LL*);
#define l4ll_prev(node) ((node)->node_prev)
#define l4ll_next(node) ((node)->node_next)
#define l4ll_len(list) ((list)->list_len)
#define l4ll_node_back(list) ((list)->list_last)
#define l4ll_node_front(list) ((list)->list_first)
#define l4ll_data(node) ((node)->node_data)
#define l4ll_datasize(node) ((node)->node_data_size)
L4LLNODE* l4ll_insert_prev(L4LL*, L4LLNODE*, const void*, int);
L4LLNODE* l4ll_insert_next(L4LL*, L4LLNODE*, const void*, int);
void l4ll_remove(L4LL*, L4LLNODE*);
#define l4ll_pushback(list,data,size) \
	(l4ll_insert_next((list),(l4ll_node_back(list)),(data),(size)))
#define l4ll_pushfront(list,data,size) \
	(l4ll_insert_prev((list),(l4ll_node_front(list)),(data),(size)))
#define l4ll_popback(list) \
	(l4ll_remove((list),(l4ll_node_back((list)))))
#define l4ll_popfront(list) \
	(l4ll_remove((list),(l4ll_node_front((list)))))
L4LLNODE* l4ll_goto(L4LLNODE*, int);

/*********** stack ***********/

typedef L4LL L4STACK;
#define l4stack_create() (l4ll_create())
#define l4stack_free(list) (l4ll_free(list))
#define l4stack_push(list,data,size) (l4ll_pushback((list),(data),(size)))
#define l4stack_pop(list) (l4ll_popback(list))
#define l4stack_len(list) (l4ll_len(list))
#define l4stack_data(list) (l4ll_data(l4ll_node_back(list)))
#define l4stack_datasize(list) (l4ll_datasize(l4ll_node_back(list)))

/*********** queue ***********/

typedef L4LL L4QUEUE;
#define l4queue_create() (l4ll_create())
#define l4queue_free(list) (l4ll_free(list))
#define l4queue_push(list,data,size) (l4ll_pushback((list),(data),(size)))
#define l4queue_pop(list) (l4ll_popfront(list))
#define l4queue_len(list) (l4ll_len(list))
#define l4queue_frontdata(list) (l4ll_data(l4ll_node_front(list)))
#define l4queue_frontdatasize(list) (l4ll_datasize(l4ll_node_front(list)))
#define l4queue_backdata(list) (l4ll_data(l4ll_node_back(list)))
#define l4queue_backdatasize(list) (l4ll_datasize(l4ll_node_back(list)))
#define l4queue_data(list) (l4queue_frontdata(list))
#define l4queue_datasize(list) (l4queue_frontdatasize(list))
#endif
