// simple memory allocator implementing malloc(), realloc(), calloc(), and free()
#include <unistd.h>			// for sbrk() system call which manipulates brk pointer (used in Unix-like OS)
#include <string.h>			// for using memset() in calloc() and memcpy() in realloc()
#include <pthread.h>		// for locking mechanism preventing multiple thread access
#include <stdio.h>			// Only added for the printf in debugging function

// to ensure 16-byte alignment for the memory blocks (an array of 16 chars a.k.a. 16 bytes since 1 char = 1 byte)
// for performance gains, compatibility, and to avoid weird behavior for some architectures when working with different data types
// most modern architectures/processors/CPU's like 16-byte alignment
typedef char ALIGN[16];

// header structure for each memory block callable by a user 
union header {
	struct {
		size_t size;					// size (in bytes) of the memory block
		unsigned is_free;				// indicates whether memory block is free or not (0/1), if free, we can allocate the block to another malloc() call
		union header *next;				// pointer to next header in a linked list we make for each malloc() call (useful to determine whether or not a memblock is free)
	} s;
	// forces the size of the union to be a multiple of 16-bytes (16, 32, 64, etc.)
	ALIGN stub;
};

// is a type alias for the header union
// using typedef so that: header_t = union header 
typedef union header header_t;

// points to first and last blocks of the linked list
header_t *head = NULL, *tail = NULL;
// a global mutex to prevent multiple threads from accessing the memory allocator (synchronize the access to it so that only 1 thread can execute at a time)
// from pthread header, useful mutex documentation
pthread_mutex_t global_malloc_lock;

// traverses the linked list to find a memory block that is free and that can accomodate given size within the parameter
// notice we are using header_t as a shortcut for union header
header_t *get_free_block(size_t size)
{
	// standard linked list traversal starting at head (the beginning) of linked list
	// by using curr ptr, we manipulate its position in linked list by traversal instead of the head ptr
	header_t *curr = head;
	// while there are still memory blocks in linked list
	while(curr) {
		// see if there's a free block that can accomodate requested size 
		if (curr->s.is_free && curr->s.size >= size)
			return curr;				// return a pointer to that block if so
		curr = curr->s.next;			// otherwise go to next block
	}
	return NULL;						// if not found within list, return null ptr
} 

// the free implementation that takes a void ptr (returned by other functions) to the memory block
void free(void *block)
{
	header_t *header, *tmp;
	// program break is ptr to the end of the process's data segment 
	void *programbreak;
	
	// edge case: if block is null then just return
	if (!block)
		return;
	pthread_mutex_lock(&global_malloc_lock);			// lock since we will be performing operations on linked list
	header = (header_t*)block - 1;						// get the header of the block (by casting block to header_t, then subtracting it by 1 which moves ptr back by size of header_t, effectively pointing to header)
	/* sbrk(0) gives the current program break address */
	programbreak = sbrk(0);								

	/*
	   Check if the block to be freed is the last one in the
	   linked list. If it is, then we could shrink the size of the
	   heap and release memory to OS. Else, we will keep the block
	   but mark it as free.
	 */
	// cast to char for proper pointer arithmetic (header size is in bytes, so we need to add the memory block in bytes (1 char = 1 byte))
	// if the block's ending address in linked list is the current program break address, it means it is the last block in list so we can free it from OS
	if ((char*)block + header->s.size == programbreak) {
		// only 1 block in linked list
		if (head == tail) {
			head = tail = NULL;
		} else {
			// if not only block, find the tail and free it from the list
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
		// removes memory allocated for block 
		sbrk(0 - header->s.size - sizeof(header_t));
		/* Note: This lock does not really assure thread
		   safety, because sbrk() itself is not really
		   thread safe. Suppose there occurs a foreign sbrk(N)
		   after we find the program break and before we decrement
		   it, then we end up realeasing the memory obtained by
		   the foreign sbrk().
		*/
		pthread_mutex_unlock(&global_malloc_lock);		// unlock right before returning
		return;
	}
	// after that or if block not at end of linked list, set marker so that block is free
	header->s.is_free = 1;
	pthread_mutex_unlock(&global_malloc_lock);			// unlock right before function ends
}

// given size, returns void ptr to same size allocated memory in heap
void *malloc(size_t size)
{
	// size of both header and requested size (given in function parameter)
	size_t total_size;
	// raw memory pointed to by sbrk()
	void *block;
	// useful to also bookkeep info about each blocks header (is memblock free or not, etc.)
	header_t *header;
	// edge case: check if size = 0, if so, return null ptr
	if (!size)
		return NULL;
	// only one thread can access allocator when operating on critical code like manipulating list, so we lock it
	pthread_mutex_lock(&global_malloc_lock);
	// searches linked list for existing free memblock that can hold requested size
	header = get_free_block(size);
	// remember that if header = NULL, we could not find a suitable block
	if (header) {
		/* Woah, found a free block to accomodate requested memory. */
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);		// unlock after traversal
		// return the memory block portion and not the header to the user (done by header + 1)
		// also cast to void ptr since that is what malloc returns (can be casted further by the user)
		return (void*)(header + 1);
	}
	// Otherwise, we need to get memory to fit in the requested block and header from OS using sbrk() 
	// basically add a new block to end of list if any previous blocks are not suitable using sbrk(total size)
	total_size = sizeof(header_t) + size;
	block = sbrk(total_size);
	// if memory allocation fails, unlock and return null ptr
	if (block == (void*) -1) {
		pthread_mutex_unlock(&global_malloc_lock);		// unlock before returning NULL ptr
		return NULL;
	}
	// if successfull, set up the header
	header = block;						// basically sets start of newly allocated memory as a header_t (union header) structure (so it has a header and memblock) 
	header->s.size = size;				// set size of memblock
	header->s.is_free = 0;				// set it to not free
	header->s.next = NULL;				// the next pointer points to nothing since it is the tail of linked list
	// if linked list has no entries yet, the block is head
	if (!head)
		head = header;
	// for updating tail ptr
	if (tail)
		tail->s.next = header;
	// otherwise tail is header if only block in linked list
	tail = header;
	pthread_mutex_unlock(&global_malloc_lock);		// unlock after the head/tail manipulation
	// get memblock location, then cast it to void ptr and return it
	return (void*)(header + 1);
}

// given # of elements and type size, does the job of malloc() except sets/initializes memory to 0
void *calloc(size_t num, size_t nsize)
{
	// total size for memblock
	size_t size;
	// raw memory ptr
	void *block;
	// edge case: if either parameters given are 0 then return NULL
	if (!num || !nsize)
		return NULL;
	// calculating total size for memblock
	size = num * nsize;
	/* check mul overflow */
	if (nsize != size / num)
		return NULL;
	// do the job of malloc() first
	block = malloc(size);
	// if malloc() fails, return NULL ptr
	if (!block)
		return NULL;
	// then if malloc() does its job, fill the block of memory with 0's using memset()
	memset(block, 0, size);
	// return the pointer to that memory block
	// we do not have to mess with header of memblock, malloc() does it for us
	return block;
}

// for resizing memory allocations 
void *realloc(void *block, size_t size)
{
	// for pointing to the header of a memblock
	header_t *header;
	// readjusted raw memory ptr 
	void *ret;
	// edge case: if either block is NULL or size is 0, defer to malloc() (would return NULL ptr ideally)
	if (!block || !size)
		return malloc(size);
	// to get header portion of block (casts it so that it points to header_t type, then moves ptr back by the size of header_t to get header location)
	header = (header_t*)block - 1;
	// if current block is already large enough to fit requested size, then return same block ptr without any changes
	if (header->s.size >= size)
		return block;
	// if block not large enough, we will malloc() another block with requested size
	ret = malloc(size);
	// if mallocated memory for new, bigger block successfull
	if (ret) {
		// Relocate contents from the old block to the new bigger block using memcpy() 
		memcpy(ret, block, header->s.size);
		// Then free the old memory block 
		free(block);
	}
	// return resized ptr to memblock
	return ret;
}

// A debug function to print the entire link list
void print_mem_list()
{	
	// standard linked list traversal
	header_t *curr = head;
	// typecast ptrs to void to ensure they are printed as addresses with "%p"
	// "%zu" = size_t values (size depends on platform), "%u" = unsigned int 
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(curr) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)curr, curr->s.size, curr->s.is_free, (void*)curr->s.next);
		curr = curr->s.next;
	}
}
