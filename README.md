## Memory Allocator
This repository includes a memory allocator written in C. It implements malloc(), free(), realloc(), and calloc() using the sbrk() call system from Unix/Linux OS. The main repository this allocator is based on is [here](https://github.com/arjun024/memalloc/tree/master.). 
## Shell
This repository also includes a shell written in C. It takes advantage of system calls to create and manipulate the processes necessary for a shell to operate. The built-in commands currently supported by the shell are cd, help, and exit. More features will be added soon. The main repository this shell is based on and takes inspiration from is [here](https://github.com/brenns10/lsh).
## How to use
First of all, the allocator and shell will work on a modern Unix/Linux environment but will not work on Windows. It contains header files (and thus macros/functions from the files) that are not natively supported on Windows. Using WSL or a VM is a possible workaround to this if you are on Windows. 
<br /> 
<br /> 
Compiling main.c :
<br />

```
$ gcc -o main main.c
 ```
<br />

Compiling mem_allocator.c :
<br />

```
$ gcc -o mem_allocator.so -fPIC -shared mem_allocator.c
 ```

<br />
You can also make any file within the folder use the implementations from the memory allocator (including the shell) by adding the mem_allocator library file to program before running :
<br />
<br />

```
$ export LD_PRELOAD=$PWD/mem_allocator.so
```


<br />
To unlink the library file :
<br />

```
$ unset LD_PRELOAD
```

