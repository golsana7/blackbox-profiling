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

int main (int argc, char* argv[]){

  
        //allocating buffer
        char * buf = malloc(BUF_SIZE);
  
	//for measuring time using ARM counter (PMU) 
	//  struct timespec start, finish;     
	unsigned long time_start;
	unsigned long time_end;
  
	int procfd;
	char buff[10];
	char *ptr;

	long page_number = strtol(argv[1],&ptr,0);
	int flag = atoi(argv[1]);
	//printf("\npage number is: %d",page_number);
	printf("\nsize argv[2]:%d ",strlen(argv[1]));
	printf("\nsize of long :%d ",sizeof(long));
/*	procfd = open("/proc/memprofile", O_RDWR);
	if(procfd < 0) {
	printf("Unable to open procfile. Are you root?");
	}
	*(long *)buf = 3;
	write(procfd, buf,  3*sizeof(long));*/
	
	//locking the memory
	//both MCL_CURRENT and MCL_FUTURE mean  current pages are locked and subsequent growth is automatically locked into memory
		if ( mlockall(MCL_CURRENT|MCL_FUTURE) == -1 )
	         perror("mlockall:3");

	/*#if NON_CACHEABLE == 1
	//#warning COMPILE WITHOUT CACHEABILITY CHANGING
	//invoking kernel module
	//system ("/sbin/insmod  /media/disk/test/vma_printer.ko"); 
	//system ("/sbin/insmod  /media/disk/test/test_pte_print/pte_print.ko");
  
	system("insmod /media/disk/test/test_pte_print/pte_print.ko myflag=1");
	//  system ("modprobe  /media/disk/test/test_pte_print/pte_print.ko myflag = 1"); 
	//modprobe /media/disk/test/test_pte_print/pte_print.ko
	printf("Your Module inserted\n");
	#endif*/
	
	procfd = open("/proc/memprofile", O_RDWR);
	if(procfd < 0) {
		printf("Unable to open procfile. Are you root?");
	}
	
	//for (*(long *)buff = 0; (*(long *)buff)<=45 ; (*(long *)buff)++)
	*(long *)buff = page_number;
		//{
		write(procfd, buff,  sizeof(long));
	//write(procfd, argv[1], strlen(argv[1]));  
		//starting time measurment
		//clock_gettime(CLOCK_REALTIME, &start);
		time_start = 0;
		get_timing(time_start);

		//do real stuff (writing into heap)
		for(int i=0; i<500; ++i) {
			for(int c=0; c<BUF_SIZE; c+=32) {
				if (c%5 == 0)
				{buf[c] = c;}
				buf[c] = i;
			}
		}

  
		//ending time mesurment
		//clock_gettime(CLOCK_REALTIME, &finish);
		time_end = 0;
		get_timing(time_end);
  

		//calculating time
		// long seconds = finish.tv_sec - start.tv_sec;
		//long ns = finish.tv_nsec - start.tv_nsec;

		/* if (start.tv_nsec > finish.tv_nsec) {//clock underflow
		   --seconds;
		   ns += 1000000000;
		   }

		   printf("\nseconds without ns: %ld\n", seconds);
		   printf("nanoseconds: %ld\n", ns);
		   printf("total seconds: %e\n", (double)seconds + (double)ns/(double)1000000000); 
		*/
	
		//printf("\nCycles when page %ld is cacheable are: %lu\n",*(long *)buff,time_end - time_start);
               printf("\nCycles when page %ld is cacheable are: %lu\n",strtol(argv[1],&ptr,0),time_end - time_start);
		//}  
	return 0;

}

#endif
