//kernel module for printing a process vma and pte


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/migrate.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/syscalls.h>
#include <asm-generic/getorder.h>
#include <asm/io.h>
#include <asm/cp15.h>
#include <asm/tlbflush.h>
#include <asm/page.h>
#include <linux/hash.h>
#include <linux/ioport.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/proc_fs.h>

#define PROF_PROCFS_NAME                "memprofile"

static struct proc_dir_entry * memprofile_proc;
/* File oeprations for the  procfile */
struct file_operations  memprof_ops;

long p = 0;


/* Adding 8 to this mask, divides cycle counter by 64 */
#define PERF_DEF_OPTS (1 | 16 | 8)


#define get_timing(cycleLo) {                                           \
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (cycleLo) ); \
	}

void enable_cpu_counters(void* data)
{
	/* Enable user-mode access to counters. */
	asm volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(1));
	/* Program PMU and enable all counters */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(PERF_DEF_OPTS));
	asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(0x8000000f));
}

int init_cpu_counter(void)
{
	printk(KERN_INFO "Now enabling performance counters on all cores.\n");
	on_each_cpu(enable_cpu_counters, NULL, 1);
	printk(KERN_INFO "Done.\n");
	return 0;
}

static int print_mem (pte_t *ptep, pgtable_t token ,  unsigned long addr,void *data){
	struct vm_area_struct *cvma = data;
	pte_t *pte = ptep;
	size_t pfn;
	pte_t newpte;
	printk("beginning of the print_mem()");
	printk("\naddress of pte is %p", pte);
	printk("\n*pte is %x", *pte);
	printk("\naddress of cacheable pte after adding 512 (HWPTE):  %p \n", (512 + pte));
	printk("\n*(pte+512) : %x \n", *(512 + pte));
	printk("\nvm_page_prot  before: cvma->vm_page_prot: %x", cvma->vm_page_prot);
	//with this part

	printk("\nwith changing the cacheability");
	//changing prot bits of vma
	//if (cmd == 3){
	/*	cvma->vm_page_prot = pgprot_noncached(cvma->vm_page_prot);
	//}
	printk("\nvm_page_prot after: cvma->vm_page_prot: %x", cvma->vm_page_prot);
	//calculating pfn
	pfn = pte_pfn(*pte); //with the old pte
	printk("\n pfn is: %x",pfn);
	//making new pte
	newpte = pfn_pte(pfn, cvma->vm_page_prot);
	printk("\naddress of the newpte is %x" , newpte);
	//setting these changes
	set_pte_ext(pte, newpte, 0);
	printk("\naddress of pte after set_pte_ext is %p", pte);
	printk("\n*pte after set_pte_ext is %x", *pte);
	printk("\naddress of pte after set_pte_ext, after adding 512 (HWPTE):  %p \n", (512 + pte));
	printk("\n*(pte+512) : %x \n", *(512 + pte));
	//flushing TLB for one page
	__flush_tlb_page(cvma,cvma->vm_start);*/
	return 0;
}
//}
    
/*  printk("\nCode  Segment start = 0x%lx, end = 0x%lx \n"
    "Data  Segment start = 0x%lx, end = 0x%lx\n"
    "Stack Segment start = 0x%lx\n",
    mm->start_code, mm->end_code,
    mm->start_data, mm->end_data,
    mm->start_stack);*/
//}

ssize_t memprofile_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	printk(KERN_ALERT "memprofile_proc_write");
	//read the data from user space
	if(copy_from_user(&p, buffer, sizeof(long))) return -EFAULT;
	else {
		printk("\nuser data is:%ld\n",p);
		struct task_struct *task;
		struct mm_struct *mm;
		//struct vm_area_struct *vma;
		struct vm_area_struct *data;
		//struct vma_srea_struct data = vma;
		char task_name [TASK_COMM_LEN];

		printk("\nwith changing the cacheability\n");
		for_each_process(task){
			get_task_comm(task_name,task);
			if(strncmp(task_name,"hello",TASK_COMM_LEN) == 0) {
				printk("%s[%d]\n", task->comm, task->pid);
				mm = task->mm;
				printk("\nThis mm_struct has %d vmas.\n", mm->map_count);
				data = mm->mmap;
				printk ("\ndata->vm_page_prot: %x\n", data->vm_page_prot);
				for (data = mm->mmap ; data ; data = data->vm_next){
					if (data->vm_start <= mm->brk && data->vm_end >= mm->start_brk){
						printk("\n[heap]");
						printk("\nnumber of pages in heap:%ld\n",(data->vm_end-data->vm_start)/PAGE_SIZE);
						printk ("\ndata->vm_page_prot: %x\n", data->vm_page_prot);
						if (p == 1){
						   printk("\np is one\n");
					           apply_to_page_range(data->vm_mm, data->vm_start+PAGE_SIZE,data->vm_end - (data->vm_start+p* PAGE_SIZE),print_mem, data);}
						else if (p == 45){
						  printk("\np is 45\n");
						  apply_to_page_range(data->vm_mm, data->vm_start, (p-1)*PAGE_SIZE , print_mem, data);}
						else{
						apply_to_page_range(data->vm_mm, data->vm_start, (p-1)*PAGE_SIZE,print_mem, data);
						printk("\nstart of the heap (data->vm_start) is: %ld", data->vm_start);
						printk("\n(data->vm_start+(p-1)*PAGE_SIZE) is: %ld", (data->vm_start+(p-1)*PAGE_SIZE));
						printk("\nnumber of pages in this range is: %ld\n ", ((data->vm_start+(p-1)*PAGE_SIZE) - data->vm_start)/PAGE_SIZE);
						apply_to_page_range(data->vm_mm, data->vm_start+p*PAGE_SIZE, data->vm_end - (data->vm_start+p*PAGE_SIZE),print_mem, data);

						}
						printk("\nnumber of pages in the second  range is: %ld\n ", (data->vm_end - (data->vm_start+p*PAGE_SIZE))/PAGE_SIZE);}
				}
			}

		}
	}
	return 0;
}

static int mm_exp_load(void){

	/* Init PMCs on all the cores */
	init_cpu_counter();

	
	/* Initialize file operations */
	memprof_ops.write = memprofile_proc_write;
	memprof_ops.owner = THIS_MODULE;


	/* Now create proc entry */
	memprofile_proc = proc_create(PROF_PROCFS_NAME, 0, NULL, &memprof_ops);

	if (memprofile_proc == NULL) {
		remove_proc_entry(PROF_PROCFS_NAME, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n", PROF_PROCFS_NAME);
		return ;
	}


	/*	struct task_struct *task;
		struct mm_struct *mm;
		//struct vm_area_struct *vma;
		struct vm_area_struct *data;
		//struct vma_srea_struct data = vma;  
		char task_name [TASK_COMM_LEN];
		// if (myflag == 1) {
		printk("\nwith changing the cacheability"); 
		for_each_process(task){
		get_task_comm(task_name,task);
		if(strncmp(task_name,"hello",TASK_COMM_LEN) == 0) {
		printk("%s[%d]\n", task->comm, task->pid);
		mm = task->mm;
		printk("\nThis mm_struct has %d vmas.\n", mm->map_count);
		data = mm->mmap;
		printk ("\ndata->vm_page_prot: %x\n", data->vm_page_prot);
		for (data = mm->mmap ; data ; data = data->vm_next){
		if (data->vm_start <= mm->brk && data->vm_end >= mm->start_brk){
		printk("\n[heap]");
		printk ("\ndata->vm_page_prot: %x\n", data->vm_page_prot);
		//print_mem(task);
		apply_to_page_range(data->vm_mm, data->vm_start, data->vm_end - data->vm_start,print_mem, data);
		}
		}
		}
		//}
		}*/
	return 0;
}

static void mm_exp_unload(void)
{
	printk("\nPrint segment information module exiting.\n");
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);
//module_param(myflag, int, 0);

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Golsana Ghaemi");
MODULE_DESCRIPTION ("make pages of a vma noncacheable");

