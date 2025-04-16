// simple memory allocator implementing malloc(), realloc(), calloc(), and free()
#include <unistd.h>			// for sbrk() system call which manipulates brk pointer (used in Unix-like OS)
#include <string.h>			// for memory functions
#include <pthread.h>			// for locking mechanism preventing multiple thread access
#include <stdio.h>			// Only added for the printf in debugging function

// to ensure 16-byte alignment for the memory blocks
typedef char ALIGN[16];

// header structure for each memory block callable by a user
union header {
	struct {
		size_t size;					// size of the memory block
		unsigned is_free;				// indicates whether memory block is free or not (0/1)
		union header *next;				// pointer to next header in linked list 
	} s;
	// forces the header to be aligned to 16 bytes 
	ALIGN stub;
};

// defining header_t type as the header union
typedef union header header_t;

// head and tail pointer for keeping track of list
header_t *head = NULL, *tail = NULL;
// global lock to prevent 2 or more threads from concurrently accessing memory
pthread_mutex_t global_malloc_lock;

// traverses linked list to find memory block that is free and that can accomodate given size
header_t *get_free_block(size_t size)
{
	header_t *curr = head;
	while(curr) {
		// see if there's a free block that can accomodate requested size 
		if (curr->s.is_free && curr->s.size >= size)
			return curr;				// return a pointer to that block if so
		curr = curr->s.next;
	}
	return NULL;						// otherwise return null ptr
} 

void free(void *block)
{
	header_t *header, *tmp;
	// program break is the end of the process's data segment 
	void *programbreak;
	
	if (!block)
		return;
	pthread_mutex_lock(&global_malloc_lock);
	header = (header_t*)block - 1;
	/* sbrk(0) gives the current program break address */
	programbreak = sbrk(0);

	/*
	   Check if the block to be freed is the last one in the
	   linked list. If it is, then we could shrink the size of the
	   heap and release memory to OS. Else, we will keep the block
	   but mark it as free.
	 */
	if ((char*)block + header->s.size == programbreak) {
		if (head == tail) {
			head = tail = NULL;
		} else {
			tmp = head;
			while (tmp) {
				if(tmp->s.next == tail) {
					tmp->s.next = NULL;
					tail = tmp;
				}
				tmp = tmp->s.next;
			}
		}
		/*
		   sbrk() with a negative argument decrements the program break.
		   So memory is released by the program to OS.
		*/
		sbrk(0 - header->s.size - sizeof(header_t));
		/* Note: This lock does not really assure thread
		   safety, because sbrk() itself is not really
		   thread safe. Suppose there occurs a foregin sbrk(N)
		   after we find the program break and before we decrement
		   it, then we end up realeasing the memory obtained by
		   the foreign sbrk().
		*/
		pthread_mutex_unlock(&global_malloc_lock);
		return;
	}
	// set marker so that block is free
	header->s.is_free = 1;
	pthread_mutex_unlock(&global_malloc_lock);
}

void *malloc(size_t size)
{
	// size of both header and requested size
	size_t total_size;
	// raw memory pointed to by sbrk()
	void *block;
	// to bookkeep info about blocks header (is memblock free or not, etc.)
	header_t *header;
	// check if size = 0, if so, return null ptr
	if (!size)
		return NULL;
	// only one thread can access allocator (effectively locks it)
	pthread_mutex_lock(&global_malloc_lock);
	// searches linked list for existing free memblock that can hold requested size
	header = get_free_block(size);
	if (header) {
		/* Woah, found a free block to accomodate requested memory. */
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		// return the memory block portion and not the header to the user (done by header + 1)
		// also cast to void ptr since that is what malloc returns (can be casted further by the user)
		return (void*)(header + 1);
	}
	// Otherwise, we need to get memory to fit in the requested block and header from OS using sbrk() 
	total_size = sizeof(header_t) + size;
	block = sbrk(total_size);
	// if memory allocation fails, unlock and return null ptr
	if (block == (void*) -1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}
	// if successfull, set up the header
	header = block;					// 
	header->s.size = size;				// set size of memblock
	header->s.is_free = 0;				// set it to not free
	header->s.next = NULL;				// the next pointer points to nothing
	// if linked list has no entries yet
	if (!head)
		head = header;
	// for updating tail ptr
	if (tail)
		tail->s.next = header;
	tail = header;
	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(header + 1);
}

void *calloc(size_t num, size_t nsize)
{
	size_t size;
	void *block;
	// edge case if either parameters given are 0
	if (!num || !nsize)
		return NULL;
	// total size for memblock
	size = num * nsize;
	// check mul overflow 
	if (nsize != size / num)
		return NULL;
	// 
	block = malloc(size);
	// malloc failure case
	if (!block)
		return NULL;
	// fill block of memory with 0's using memset
	memset(block, 0, size);
	// return the pointer to that memory block
	return block;
}

void *realloc(void *block, size_t size)
{
	header_t *header;
	void *ret;
	// edge case
	if (!block || !size)
		return malloc(size);
	// to get header portion of block
	header = (header_t*)block - 1;
	if (header->s.size >= size)
		return block;
	ret = malloc(size);
	if (ret) {
		// Relocate contents to the new bigger block 
		memcpy(ret, block, header->s.size);
		// Free the old memory block 
		free(block);
	}
	return ret;
}

// A debug function to print the entire link list
void print_mem_list()
{
	header_t *curr = head;
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(curr) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)curr, curr->s.size, curr->s.is_free, (void*)curr->s.next);
		curr = curr->s.next;
	}
}
