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
#include <asm/cacheflush.h> /*for processor L1 cache flushing*/
#include <asm/outercache.h>
#include <asm/hardware/cache-l2x0.h>

#define PROF_PROCFS_NAME                "memprofile"

static struct proc_dir_entry * memprofile_proc;
/* File oeprations for the  procfile */
struct file_operations  memprof_ops;

long p = 0;
//char *p;

struct Data
{
	struct vm_area_struct  *vmas;
	unsigned long page_addr;

};

/* Adding 8 to this mask, divides cycle counter by 64 */
#define PERF_DEF_OPTS (1 | 16 | 8)

#define HW_PL310_CL_INV_PA      0x07F0 / 4
/* PL310 Base for iMX.6 Dual/Quad (Wandboard, PICO) */
#define HW_PL310_BASE           0x00A02000

volatile unsigned long __iomem * pl310_area;
//for debugging
void print_debug(bool enable, const char* statement, unsigned long variable)
{
	if (enable)
	{
		if (variable == 0)
			printk(statement);
		else
			printk(statement,variable);
	}

}

//testing for invalidating page
inline void invalidate_page_l1(ulong va_addr)
{
	/* Invalidation procedure -- via coprocessor 15 */
	ulong tmp = 0;

	__asm__ __volatile__
		(
			"mov %0, %1\n"
			"1: \n"
			"MCR p15, 0, %0, c7, c5, 1\n" /* invalidate I-cache line */
			"MCR p15, 0, %0, c7, c14, 1\n" /* invalidate+clean D-cache line */
			"add %0, #32\n"
			"cmp %0, %2\n"
			"bne 1b\n"
			: "=&r"(tmp)
			: "r"(va_addr), "r"(va_addr + PAGE_SIZE) /* Inputs */
			: "memory"
			);


}

inline void invalidate_page_l2(ulong pa_addr)
{
	volatile ulong * inval_reg = &pl310_area[HW_PL310_CL_INV_PA];
	ulong tmp = 0;

	/* Invalidation procedure -- atomic operations */
	__asm__ __volatile__
		(
	                "mov %0, %1\n"
			"1: str %0, [%2]\n"
			"add %0, #32\n"
			"cmp %0, %3\n"
			"bne 1b\n"
			: "=&r"(tmp)
			: "r"(pa_addr), "r"(inval_reg), "r"(pa_addr + PAGE_SIZE) /* Inputs */
			: "memory"
			);
}
//end of testing


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


static int print_mem (pte_t *ptep, pgtable_t token ,  unsigned long addr,void *data)
{
	struct Data *cdata = data;
	//printk("\nBEGINNING OF THE PRINT_MEM()\n");
	pte_t *pte = ptep;
	size_t pfn;
	pte_t newpte;
	struct page *page = NULL; /*for making page and finding physical address */
	unsigned long phys; /*physical addr*/
  
	// if (addr == page address) return;
		if (addr == cdata->page_addr)
	{
		printk("\nwe skip page with index p %ld\n",p);
	}
	else
	{
	  //print_debug(1,"\n\nbeginning of the else print_mem() with print_debug()",0);
		//printk("\ncountt is: %d\n",countt++);
		print_debug(0,"\ntest of print_debug, address of pte is %p", (unsigned long)pte);
		print_debug(0,"\n*pte is %x with pte_debug()", *pte);
		print_debug(0,"\naddress of cacheable pte after adding 512 (HWPTE) with print_debug: %p\n",(unsigned long) (512 + pte));
		print_debug(0,"\n*(pte+512) with debug_print() : %x \n", *(512 + pte));
		print_debug(0,"\nwith print_debug() vm_page_prot  before: cdata->vmas->vm_page_prot: %x", cdata->vmas->vm_page_prot);
		//changing prot bits of vma to make it noncacheable
		cdata->vmas->vm_page_prot = pgprot_noncached(cdata->vmas->vm_page_prot);
		print_debug(0,"\nvm_page_prot after: cvma->vm_page_prot: %x", cdata->vmas->vm_page_prot);
		//making page struct
		page = pte_page(*pte);
		phys = page_to_phys(page);
		//printk("\nphys is:%ld\n ",phys);
		//calculating pfn
		pfn = pte_pfn(*pte); //with the old pte
		print_debug(0,"\n pfn is: %x",pfn);
		//making new pte
		newpte = pfn_pte(pfn, cdata->vmas->vm_page_prot);
		print_debug(0,"\naddress of the newpte is %x", newpte);
		/*
		//flushing L1 cache
		__cpuc_flush_user_range(addr,addr+PAGE_SIZE,cdata->vmas->vm_flags);
		printk("\nafter flush_cache_page\n");
		//flushing L 2 cache
		outer_cache.clean_range(phys,phys+PAGE_SIZE);//PHYSICAL ADDR
		// outer_clean_range(phys,phys+PAGE_SIZE);
		*/
      
		/* Perform PA-based invaluidation on L1 and L2 */
		
		//invalidate_page_l2(phys);
		invalidate_page_l1(addr);
		invalidate_page_l2(phys);
		//setting new pte
		set_pte_ext(pte, newpte, 0);
		print_debug(0,"\naddress of pte after set_pte_ext is %p",(unsigned long) pte);
		print_debug(0,"\n*pte after set_pte_ext is %x", *pte);
		print_debug(0,"\naddress of pte after set_pte_ext, after adding 512 (HWPTE):  %p \n",(unsigned long) (512 + pte));
		print_debug(0,"\n*(pte+512) : %x \n", *(512 + pte));
		//flushing TLB for one page
		//__flush_tlb_page(cvma,cvma->vm_start);//__flush_tlb_page(cvma,addr);
		// each time addr is added by 4KB 
		//printk("\nadress is: %ld\n", addr);
		__flush_tlb_page(cdata->vmas,addr);
		//printk("\nend of print_mem()\n");
		//return 0;
	      	}
	return 0;
}

ssize_t memprofile_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	printk(KERN_ALERT "memprofile_proc_write");
	//int countt = 0;
	//read the data from user space
	if(copy_from_user(&p, buffer, sizeof(int))) return -EFAULT;
	else {
		printk("\nuser data is:%ld\n",p);
		struct task_struct *task;
		struct mm_struct *mm;
		//struct vm_area_struct *data;
		struct Data *data = kmalloc (sizeof(struct Data), GFP_KERNEL);
		//struct vma_srea_struct data = vma;
		char task_name [TASK_COMM_LEN];


		for_each_process(task)
		{
			get_task_comm(task_name,task);
			if(strncmp(task_name,"mser",TASK_COMM_LEN) == 0) {
				printk("\n%s[%d]\n", task->comm, task->pid);
				mm = task->mm;
				printk("\nThis mm_struct has %d vmas.\n", mm->map_count);

				data->vmas = mm->mmap;
				//int countt = 0;
				print_debug (0,"\ndata->vm_page_prot: %x\n", data->vmas->vm_page_prot);
				for (data->vmas = mm->mmap ; data->vmas ; data->vmas = data->vmas->vm_next)
				{
					if (data->vmas->vm_start <= mm->brk && data->vmas->vm_end >= mm->start_brk)
					{
						print_debug(1,"\n[heap]",0);
						data->page_addr = data->vmas->vm_start+((p-1)*PAGE_SIZE);
						printk("\ndata->vmas->vm_start is: %ld\n",data->vmas->vm_start);
						printk("\np is: %ld. PAGE_SIZE is: %ld\n",p,PAGE_SIZE);
						printk("\ndata->page_addr(data->vmas->vm_start+p*PAGE_SIZE) is: %ld\n",data->vmas->vm_start+(p*PAGE_SIZE));
						print_debug(1,"\nnumber of pages in heap:%ld\n",(data->vmas->vm_end-data->vmas->vm_start)/PAGE_SIZE);
						//print_debug (0,"\ndata->vm_page_prot: %x\n", data->vm_page_prot);
						apply_to_page_range(data->vmas->vm_mm, data->vmas->vm_start, data->vmas->vm_end - data->vmas->vm_start,print_mem, data);



					}
				}
			}

		}
	}
	return 0;
}

static int mm_exp_load(void){

	/* Init PMCs on all the cores */
	init_cpu_counter();

	
	/*PL310 L2 cache for using those clean,invalidate funcs*/
	/*struct resource * pl310_res = NULL;
	//Setup the I/O memory for the PL310 cache controller
	pl310_res = request_mem_region(HW_PL310_BASE, PAGE_SIZE, "PL310 Area");
	printk(KERN_INFO "PL310 area @ 0x%p\n", pl310_area);
	if (!pl310_res) {
	printk(KERN_INFO "Unable to request mem region. Is it already mapped?");
	return 1;
	}
	*/

	printk("test for PL310");
	
	pl310_area = ioremap_nocache(HW_PL310_BASE, PAGE_SIZE);
	printk(KERN_INFO "PL310 area @ 0x%p\n", pl310_area);

	if (!pl310_area) {
		printk(KERN_INFO "Unable to perform ioremap.");
		return 1;
	}

	
	/* Initialize file operations */
	memprof_ops.write = memprofile_proc_write;
	memprof_ops.owner = THIS_MODULE;


	/* Now create proc entry */
	memprofile_proc = proc_create(PROF_PROCFS_NAME, 0666, NULL, &memprof_ops);

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
        remove_proc_entry(PROF_PROCFS_NAME, NULL);
	printk("\nPrint segment information module exiting.\n");
	
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);
//module_param(myflag, int, 0);
MODULE_LICENSE("GPL");

MODULE_AUTHOR ("Golsana Ghaemi");
MODULE_DESCRIPTION ("make pages of a vma noncacheable");



