#include "fms_mem.h"
#include "fms_log.h"
#include <stdlib.h>

//Binder是通过前后的节点来计算内存块的大小，而我是直接赋值
//在应用层内存需要做内存对齐么?那么Binder内核层呢?

#define MEM_ALIGN_BIT 4

#define mem_align_ptr(p, a) \
    (fms_u8 *) (((fms_u32) (p) + ((fms_u32) (a) - 1)) & ~((fms_u32) (a) - 1))

//注意 free红黑树以缓冲区大小组织，而allocated以地址组织?why?可以直接用链表组织么?反正没有用到查询的功能
//内部函数不需要检查参数

int my_insert(struct rb_root *root, fms_mem_block *new_block)
{
	struct rb_node **pos = &(root->rb_node), *parent = NULL;
//printf("fremaks 1,*pos=%x\n", *pos);
	/* Figure out where to put new node */
	while (*pos)
	{
//printf("fremaks 2\n");
		fms_mem_block *block = container_of(*pos, fms_mem_block, rb_node);
		parent = *pos;
		
		if (new_block->data_size < block->data_size) {
			pos = &((*pos)->rb_left);
		} else {
			pos = &((*pos)->rb_right);
		}	
	}
//printf("fremaks 3\n");
	/* Add new node and rebalance tree. */
	rb_link_node(&new_block->rb_node, parent, pos);
//printf("fremaks 4, *pos=%x\n", *pos);
	rb_insert_color(&new_block->rb_node, root);
//printf("fremaks 5\n");
}



static void mem_block_insert_free(fms_mem_pool *mem_pool, fms_mem_block *new_block) {
	//printf("@@@@@@@@@@@free:%x alloc:%x\n", mem_pool->free_blocks.rb_node, mem_pool->allocated_blocks.rb_node);
	my_insert(&mem_pool->free_blocks, new_block);
}


void mem_block_insert_allocated(fms_mem_pool *mem_pool, fms_mem_block *new_block) {
	//printf("###########alloc:%x free=%x\n", mem_pool->allocated_blocks.rb_node, mem_pool->free_blocks.rb_node);
	my_insert(&mem_pool->allocated_blocks, new_block);
}


fms_mem_pool *fms_mem_pool_new(fms_u32 capacity) {
	fms_mem_pool *mem_pool = NULL;	
	
	mem_pool = (fms_mem_pool *)malloc(sizeof(fms_mem_pool));
	FMS_ASSERT(mem_pool != NULL);
	memset(mem_pool, 0, sizeof(fms_mem_pool));
	
	INIT_LIST_HEAD(&mem_pool->blocks);
	mem_pool->free_blocks = RB_ROOT;//?????????
	mem_pool->allocated_blocks= RB_ROOT;
	mem_pool->block_id = 0;
	mem_pool->capacity = capacity;

	fms_mem_block *mem_block = (fms_mem_block *)malloc(capacity);
	FMS_ASSERT(mem_block != NULL);
	memset(mem_block, 0, capacity);
	
	mem_block->data_size = capacity - sizeof(fms_mem_block);
	mem_block->id = mem_pool->block_id;
	mem_block->free = FMS_TRUE;
	list_add(&mem_block->entry, &mem_pool->blocks);
	mem_block_insert_free(mem_pool, mem_block);
	//printf("@@@@@@@@size=%d\n", sizeof(fms_mem_block));
	//printf(">>>>>>>>>>free=%x allocl=%x\n", mem_pool->free_blocks.rb_node, mem_pool->allocated_blocks.rb_node);
	return mem_pool;
}


void fms_mem_pool_free(fms_mem_pool *mem_pool) {

	
}



void *fms_mem_alloc(fms_mem_pool *mem_pool, fms_u32 data_size) {
	struct rb_node *pos = mem_pool->free_blocks.rb_node;
	fms_mem_block *block = NULL;
	struct rb_node *best_fit = NULL;
	fms_u32 block_data_size = 0;
	data_size = mem_align_ptr(data_size, 4);//不加这一句就段错误� why ?建议做一次地址跟踪测试
	//printf("@@@@@@@@@@fms_mem_alloc:datasize=%u\n", data_size);
	while (pos) {

		block = rb_entry(pos, fms_mem_block, rb_node);
		//printf("lorent 1, need %u[%u]\n", data_size, block->data_size);
		block_data_size = block->data_size;
		if (data_size < block->data_size) {
			best_fit = pos;
			pos = pos->rb_left;
		} else if (data_size > block->data_size) {
			pos = pos->rb_right;
		} else {
			best_fit = pos;
			break;
		}
	}

	if (best_fit == NULL) {
		//没有符合大小的内存块，那么需要再次扩展内存池，一般不会发生此种情况
		printf("mem_pool no space\n");
	}
	
	if (NULL == pos) { //没有找到刚好符合的内存块 pos != best_fit

		block = rb_entry(best_fit, fms_mem_block, rb_node);
		//printf("lorent 2, find %u\n", block->data_size);
		block_data_size = block->data_size;
		//这里有个隐藏条件	block->data_size > data_size
		if (data_size + sizeof(fms_mem_block) > block->data_size) {
			 /* no room for other buffers */
		} else {
			//printf("lroent 2-1, cut %u->%u\n", block->data_size, data_size);
			block->data_size = data_size;//裁剪大小
		}
	}	
//printf("lorent 3\n");
	rb_erase(best_fit, &mem_pool->free_blocks);
//printf("lorent 3-1\n");
	mem_block_insert_allocated(mem_pool, block);
	block->free = FMS_FALSE;

	if (block->data_size != block_data_size) { //裁剪

		fms_mem_block *new_block = (fms_mem_block *)((fms_u8 *)block->data + block->data_size);
		new_block->data_size = block_data_size - block->data_size - sizeof(fms_mem_block);
		//printf("lorent 4, new block size=%u\n", new_block->data_size );
		new_block->id = mem_pool->block_id;//特别注意裁剪出来的Block的ID是相同的
		new_block->free = FMS_TRUE;
		list_add(&new_block->entry, &block->entry);//这里可能会出错?list_add的功能
		mem_block_insert_free(mem_pool, new_block);
	}
//printf("lorent 5\n");
	return (void *)block->data;
}

//释放的时候合并,相对于binder而言，这里的缓冲区会扩展，所以内存不是全部连续，而是一段段连续
void fms_mem_free(fms_mem_pool *mem_pool, void *data) {
	fms_mem_block *block = (fms_mem_block *)((fms_u8 *)data - sizeof(fms_mem_block));//最好用宏来实现
	
	
	rb_erase(&block->rb_node, &mem_pool->allocated_blocks);

	if (!list_is_last(&block->entry, &mem_pool->blocks)) {//如果不是最后一个，向后合并
		fms_mem_block *next = list_entry(&block->entry.next, fms_mem_block, entry);
		if (next->free && next->id == block->id) { //ID相同才是同一段连续内存，才可能合并
			rb_erase(&next->rb_node, &mem_pool->free_blocks);
			list_del(&next->entry);
			block->data_size += sizeof(fms_mem_block) + next->data_size;//数据区扩展，合并
		}
	}
	
	if (!list_is_first(&block->entry, &mem_pool->blocks)) {//如果不是最前面的一个，向前合并
		fms_mem_block *prev = list_entry(&block->entry.prev, fms_mem_block, entry);
		if (prev->free && prev->id == block->id) { 
			rb_erase(&prev->rb_node, &mem_pool->free_blocks);
			prev->data_size += sizeof(fms_mem_block) + block->data_size;
			list_del(&block->entry);
			block = prev;
		}
	}
	
	block->free = FMS_TRUE;
	mem_block_insert_free(mem_pool, block);
	
}


#if 0
int my_insert(struct rb_root *root, struct mynode *data)
{
      struct rb_node **new1 = &(root->rb_node), *parent = NULL;

      /* Figure out where to put new node */
      while (*new1) {
          struct mynode *this1 = container_of(*new1, struct mynode, node);
          int result = strcmp(data->string, this1->string);

        parent = *new1;
          if (result < 0)
              new1 = &((*new1)->rb_left);
          else 
              new1 = &((*new1)->rb_right);
    
      }

      /* Add new node and rebalance tree. */
      rb_link_node(&data->node, parent, new1);
      rb_insert_color(&data->node, root);

    return 1;
}
#endif

