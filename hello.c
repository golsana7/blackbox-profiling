#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     /* Support all standards    */
#include <sys/mman.h>   /* Memory locking functions */
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BUF_SIZE 50*1024

#define HELLO 0
#define NON_CACHEABLE 0
#define ALL_CACHEABLE_EXCEPT_ONE 0

#define get_timing(cycleLo) {                                           \
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (cycleLo) ); \
	}


//struct timespec {
//time_t   tv_sec;        /* seconds */
//long     tv_nsec;       /* nanoseconds */
//};



#if HELLO ==  1
#warning COMPILE AS HELLO

int main() {
 
	while(1)
	{  printf("Hello, World!");}
  
	return 0;
}


#else
#warning COMPILE BUFFER ONE


int main (int argc, char** argv)
{

  
  int page_number = atoi (argv[1]);

        //allocating buffer
        char * buf = malloc(BUF_SIZE);
        //char *buf;  
	//for measuring time using ARM counter (PMU) 
	//  struct timespec start, finish;     
	unsigned long time_start;
	unsigned long time_end;
  
	int procfd;
	
	char buff[100];
	char *ptr;

	//int page_number = 25;
	//long page_number = strtol(argv[1],&ptr,0);
	//flag was supposed to work as enabler of print_debug() in kernel module 
	//int page_number = atoi(argv[1]);
	//printf("\npage number is: %d",page_number);
	//printf("\nsize argv[2]:%d ",strlen(argv[1]));
	//printf("\nsize of long :%d ",sizeof(long));
	//buf  =  malloc(BUF_SIZE);  
	//locking the memory
	//both MCL_CURRENT and MCL_FUTURE mean  current pages are locked and subsequent growth is automatically locked into memory
	if ( mlockall(MCL_CURRENT|MCL_FUTURE) == -1 )
		perror("mlockall:3");

#if ALL_CACHEABLE_EXCEPT_ONE

	procfd = open("/proc/cacheable", O_RDWR);
	if(procfd < 0) {
		printf("Unable to open procfile. Are you root?");
	}
	*(int *)buff = page_number;
	write(procfd, buff, 1* sizeof(int));

#else
	procfd = open("/proc/memprofile", O_RDWR);
	if(procfd < 0) {
		printf("Unable to open procfile. Are you root?");
	}
	*(int *)buff = page_number;
	write(procfd, buff, 1* sizeof(int));

      

#endif
	
	//starting time measurment
	time_start = 0;
	get_timing(time_start);

	//do real stuff (writing into heap)
	for(int i=0; i<500; ++i)
	  {
	    for(int c=0; c< BUF_SIZE; c+=32)
		  {
			if (c%5 == 0)
			{
			  buf[c] = c;
			}
			buf[c] = i;
		}
	}

  
	//ending time mesurment
	//clock_gettime(CLOCK_REALTIME, &finish);
	time_end = 0;
	get_timing(time_end);
  

	
	//printf("\nCycles when page %ld is cacheable are: %lu\n",*(long *)buff,time_end - time_start);
	//printf("\nCycles when page %ld is cacheable are: %lu\n",strtol(argv[1],&ptr,0),time_end - time_start);
	//printf("\n %ld:%lu\n",atoi(argv[1],&ptr,0),time_end - time_start);
	printf("\nCycles when page %d is cacheable are: %lu\n",page_number,time_end - time_start);  
	//}
	//free (buf);
	return 0;

}

#endif
