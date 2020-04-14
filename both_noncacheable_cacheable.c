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
#define CACHEABLE_PROCFS_NAME              "cacheable"
#define ALL_CACHEABLE_EXCEPT_ONE 0

static struct proc_dir_entry * memprofile_proc;
/* File oeprations for the  procfile */
struct file_operations  memprof_ops,cacheable_ops;

int p = 0;
//char *p;
struct Data
{
	struct vm_area_struct  *vmas;
	unsigned long page_addr;
  
};

/* Adding 8 to this mask, divides cycle counter by 64 */
#define PERF_DEF_OPTS (1 | 16 | 8)

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

#if ALL_CACHEABLE_EXCEPT_ONE
static void cacheable_print_mem(struct task_struct *task)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	size_t pfn;
	pte_t newpte;
	mm = task->mm;
	vma = mm->mmap;
	print_debug(1,"\nbeggining of cacheable_print_mem",0);
	print_debug(0,"\n(wuth debug_print())This mm_struct has %d vmas.\n", (unsigned long)mm->map_count);
	for (vma = mm->mmap ; vma ; vma = vma->vm_next){
		if (vma->vm_start <= mm->brk && vma->vm_end >= mm->start_brk){
			print_debug(1,"\n[heap]",0);
			pgd = pgd_offset(mm, vma->vm_start+((p-1)*PAGE_SIZE));
			print_debug(0,"\naddress of pgd is %p", (unsigned long)pgd);
			pud = pud_offset(pgd,vma->vm_start+((p-1)*PAGE_SIZE));
			print_debug(0,"\naddress of pud is %p",(unsigned long) pud);
			pmd = pmd_offset(pud,vma->vm_start+((p-1)*PAGE_SIZE));
			print_debug(0,"\naddress of pmd is %p", (unsigned long)pmd);
			pte = pte_offset_map(pmd,vma->vm_start+((p-1)*PAGE_SIZE));
			print_debug(0,"\naddress of pte is %p",(unsigned long) pte);
			print_debug(0,"\n*pte is %x", *pte);
			print_debug(0,"\naddress of cacheable pte after adding 512 (HWPTE):  %p \n",(unsigned long) (512 + pte));
			print_debug(0,"\n*(pte+512) : %x \n", *(512 + pte));
			//changing prot bits of vma
			print_debug(0,"\nvm_page_prot before: vma->vm_page_prot: %x", vma->vm_page_prot);
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			print_debug(0,"\nvm_page_prot after: vma->vm_page_prot: %x", vma->vm_page_prot);
			//calculating pfn
			pfn = pte_pfn(*pte); //with the old pte
			print_debug(0,"\n pfn is: %x",pfn);
			//making new pte
			newpte = pfn_pte(pfn, vma->vm_page_prot);
			print_debug(0,"\naddress of the newpte is %x" , newpte);
			//setting these changes
			set_pte_ext(pte, newpte, 0);
			print_debug(0,"\naddress of pte after set_pte_ext is %p",(unsigned long) pte);
			print_debug(0,"\n*pte after set_pte_ext is %x", *pte);
			print_debug(0,"\naddress of pte after set_pte_ext, after adding 512 (HWPTE):  %p \n",(unsigned long) (512 + pte));
			print_debug(0,"\n*(pte+512) : %x \n", *(512 + pte));
			//flushing TLB for one page
			//__flush_tlb_page(vma,vma->vm_start);
			__flush_tlb_page(vma, vma->vm_start+((p-1)*PAGE_SIZE));
			print_debug(0,"\nnumber of pages in heap:%ld\n",(vma->vm_end-vma->vm_start)/PAGE_SIZE);
			//printk("\nstarting address of (heap)  vma->vm_start is 0x%lx", vma->vm_start);*/

		}
	}
}


ssize_t cacheable_proc_write(struct file *file, const char __user *buffer,
			     size_t count, loff_t *data)
{
	printk(KERN_ALERT "memprofile_proc_write");
	//read the data from user space
	if(copy_from_user(&p, buffer, sizeof(int))) return -EFAULT;
	else {
		if (p==0)   printk("baseline when all pages are cacheable");
		else
		{
			printk("\nuser data is:%d\n",p);
			struct task_struct *task;
			char task_name [TASK_COMM_LEN];

			printk("\nALL_CACHEABLE_EXCEPT ONE\n");
			for_each_process(task){
				get_task_comm(task_name,task);
				if(strncmp(task_name,"hello",TASK_COMM_LEN) == 0) {
					printk("%s[%d]\n", task->comm, task->pid);
					cacheable_print_mem(task);
				}
			}
		}
	}

	return 0;
}

#else

static int print_mem (pte_t *ptep, pgtable_t token ,  unsigned long addr,void *data){
	//struct vm_area_struct *cvma = ((struct Data *)data)->vmas;
	//unsigned long page_address = ((struct Data *)data) -> page_addr;
        struct Data *cdata = data;
  
	pte_t *pte = ptep;
	size_t pfn;
	pte_t newpte;
	// if (addr == page address) return;
	if (addr == cdata->page_addr) return 0;
	else
	{
		print_debug(1,"\nbeginning of the print_mem() with print_debug()",0);
		print_debug(0,"\ntest of print_debug, address of pte is %p", (unsigned long)pte);
		print_debug(0,"\n*pte is %x with pte_debug()", *pte);
		print_debug(0,"\naddress of cacheable pte after adding 512 (HWPTE) with print_debug: %p\n",(unsigned long) (512 + pte));
		print_debug(0,"\n*(pte+512) with debug_print() : %x \n", *(512 + pte));
		print_debug(0,"\nwith print_debug() vm_page_prot  before: cvma->vm_page_prot: %x", cdata->vmas->vm_page_prot);
		//changing prot bits of vma to make it noncacheable
		cdata->vmas->vm_page_prot = pgprot_noncached(cdata->vmas->vm_page_prot);
		print_debug(0,"\nvm_page_prot after: cvma->vm_page_prot: %x", cdata->vmas->vm_page_prot);
		//calculating pfn
		pfn = pte_pfn(*pte); //with the old pte
		print_debug(0,"\n pfn is: %x",pfn);
		//making new pte
		newpte = pfn_pte(pfn, cdata->vmas->vm_page_prot);
		print_debug(0,"\naddress of the newpte is %x", newpte);
		//setting these changes
		set_pte_ext(pte, newpte, 0);
		print_debug(0,"\naddress of pte after set_pte_ext is %p",(unsigned long) pte);
		print_debug(0,"\n*pte after set_pte_ext is %x", *pte);
		print_debug(0,"\naddress of pte after set_pte_ext, after adding 512 (HWPTE):  %p \n",(unsigned long) (512 + pte));
		print_debug(0,"\n*(pte+512) : %x \n", *(512 + pte));
		//flushing TLB for one page
		//__flush_tlb_page(cvma,cvma->vm_start);//__flush_tlb_page(cvma,addr);
		__flush_tlb_page(cdata->vmas,addr);
		return 0;
	}
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
	if(copy_from_user(&p, buffer, sizeof(int))) return -EFAULT;
	else {
		printk("\nuser data is:%d\n",p);
		struct task_struct *task;
		struct mm_struct *mm;
		//struct vm_area_struct *data;
		struct Data *data = kmalloc (sizeof(struct Data), GFP_KERNEL);
		//struct vma_srea_struct data = vma;
		char task_name [TASK_COMM_LEN];

		
		for_each_process(task)
		{
			get_task_comm(task_name,task);
			if(strncmp(task_name,"hello",TASK_COMM_LEN) == 0) {
				printk("\n%s[%d]\n", task->comm, task->pid);
				mm = task->mm;
				printk("\nThis mm_struct has %d vmas.\n", mm->map_count);
				data->vmas = mm->mmap;
				print_debug (0,"\ndata->vm_page_prot: %x\n", data->vmas->vm_page_prot);
				for (data->vmas = mm->mmap ; data->vmas ; data->vmas = data->vmas->vm_next)
				{
					if (data->vmas->vm_start <= mm->brk && data->vmas->vm_end >= mm->start_brk)
					{
						print_debug(1,"\n[heap]",0);
						data->page_addr = data->vmas->vm_start+p*PAGE_SIZE;
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

#endif

static int mm_exp_load(void){

	/* Init PMCs on all the cores */
	init_cpu_counter();

#if ALL_CACHEABLE_EXCEPT_ONE
	/*initialize file operations*/
	cacheable_ops.write = cacheable_proc_write;
	cacheable_ops.owner = THIS_MODULE;

	/* Now create proc entry */
	memprofile_proc = proc_create(CACHEABLE_PROCFS_NAME, 0666, NULL, &cacheable_ops);

	if (memprofile_proc == NULL) {
		remove_proc_entry(CACHEABLE_PROCFS_NAME, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n", CACHEABLE_PROCFS_NAME);
		return ;
	}
#else
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

#endif

	return 0;
}

static void mm_exp_unload(void)
{
	//#if ALL_CACHEABLE_EXCEPT_ONE
        remove_proc_entry(CACHEABLE_PROCFS_NAME, NULL);
	//#else
        remove_proc_entry(PROF_PROCFS_NAME, NULL);
	//#endif
	printk("\nPrint segment information module exiting.\n");
	
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);
//module_param(myflag, int, 0);

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Golsana Ghaemi");
MODULE_DESCRIPTION ("make pages of a vma noncacheable");

