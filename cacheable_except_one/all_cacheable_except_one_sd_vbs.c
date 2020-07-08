//kernel module for making just one page at a time non-cacheable

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

/*page number*/
long p = 0;


/* Adding 8 to this mask, divides cycle counter by 64 */
#define PERF_DEF_OPTS (1 | 16 | 8)


#define HW_PL310_CL_INV_PA      0x07F0 / 4
// PL310 Base for iMX.6 Dual/Quad (Wandboard, PICO) 
#define HW_PL310_BASE           0x00A02000

volatile unsigned long __iomem * pl310_area;


//testing for invalidating page
inline void invalidate_page_l1(ulong va_addr)
{
	// Invalidation procedure -- via coprocessor 15 
	ulong tmp = 0;

	__asm__ __volatile__
		(
			"mov %0, %1\n"
			"1: \n"
			"MCR p15, 0, %0, c7, c5, 1\n" // invalidate I-cache line 
			"MCR p15, 0, %0, c7, c14, 1\n" // invalidate+clean D-cache line 
			"add %0, #32\n"
			"cmp %0, %2\n"
			"bne 1b\n"
			: "=&r"(tmp)
			: "r"(va_addr), "r"(va_addr + PAGE_SIZE) // Inputs 
			: "memory"
			);


}

inline void invalidate_page_l2(ulong pa_addr)
{
	volatile ulong * inval_reg = &pl310_area[HW_PL310_CL_INV_PA];
	ulong tmp = 0;

	//Invalidation procedure -- atomic operations 
	__asm__ __volatile__
		(
			"mov %0, %1\n"
			"1: str %0, [%2]\n"
			"add %0, #32\n"
			"cmp %0, %3\n"
			"bne 1b\n"
			: "=&r"(tmp)
			: "r"(pa_addr), "r"(inval_reg), "r"(pa_addr + PAGE_SIZE) // Inputs 
			: "memory"
			);
}
//end of testing   

/*all three following functions for capturing cycles*/
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

static void print_mem(struct task_struct *task){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	size_t pfn;
	pte_t newpte;
	struct page *page = NULL; /*for making page and finding physical address */
	unsigned long phys; /*physical addr*/
	mm = task->mm;
	vma = mm->mmap;
	printk("\nThis mm_struct has %d vmas.\n", mm->map_count);
	for (vma = mm->mmap ; vma ; vma = vma->vm_next)
	{
		if (vma->vm_start <= mm->brk && vma->vm_end >= mm->start_brk){
			printk("\n[heap]");
			//pgd = pgd_offset(mm, vma->vm_start);
			//printk("\naddress of pgd is %p", pgd);
			pgd = pgd_offset(mm, vma->vm_start+((p-1)*PAGE_SIZE));
			//printk("\naddress of pgd is %p", pgd);
			//pud = pud_offset(pgd,vma->vm_start);
			//printk("\naddress of pud is %p", pud);
			pud = pud_offset(pgd,vma->vm_start+((p-1)*PAGE_SIZE));
			//printk("\naddress of pud is %p", pud);
			pmd = pmd_offset(pud,vma->vm_start+((p-1)*PAGE_SIZE));
			//printk("\naddress of pmd is %p", pmd);
			pte = pte_offset_map(pmd,vma->vm_start+((p-1)*PAGE_SIZE));
			/*printk("\naddress of pte is %p", pte);
			  printk("\n*pte is %x", *pte);
			  printk("\naddress of cacheable pte after adding 512 (HWPTE):  %p \n", (512 + pte));
			  printk("\n*(pte+512) : %x \n", *(512 + pte));*/
			//changing prot bits of vma
			//printk("\nvm_page_prot before: vma->vm_page_prot: %x", vma->vm_page_prot);
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			//printk("\nvm_page_prot after: vma->vm_page_prot: %x", vma->vm_page_prot);

			//making page struct
			page = pte_page(*pte);
			phys = page_to_phys(page);

			//calculating pfn
			pfn = pte_pfn(*pte); //with the old pte
			//printk("\n pfn is: %x",pfn);
			//making new pte
			newpte = pfn_pte(pfn, vma->vm_page_prot);
			//printk("\naddress of the newpte is %x" , newpte);
			//setting these changes
			//invalidate_page_l2(phys);
			invalidate_page_l1(vma->vm_start+((p-1)*PAGE_SIZE));
			invalidate_page_l2(phys);

			//setting new pte
			set_pte_ext(pte, newpte, 0);
			/*printk("\naddress of pte after set_pte_ext is %p", pte);
			  printk("\n*pte after set_pte_ext is %x", *pte);
			  printk("\naddress of pte after set_pte_ext, after adding 512 (HWPTE):  %p \n", (512 + pte));
			  printk("\n*(pte+512) : %x \n", *(512 + pte));*/
			//flushing TLB for one page
			__flush_tlb_page(vma,vma->vm_start+((p-1)*PAGE_SIZE));
			// pte = pte_offset_map(pmd,vma->vm_start);
			//printk("\naddress of pte is %p", pte);
			//printk("\n*pte is %x", *pte);
			printk("\nnumber of pages in heap:%ld\n",(vma->vm_end-vma->vm_start)/PAGE_SIZE);
			//printk("\nstarting address of (heap)  vma->vm_start is 0x%lx", vma->vm_start);*/
		}}}

ssize_t memprofile_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	printk(KERN_ALERT "memprofile_proc_write");
	//read the data from user space
	if(copy_from_user(&p, buffer, sizeof(long))) return -EFAULT;

	else
	{
		if (p==0)   printk("baseline when all pages are cacheable");

		else
		{
			printk("\nuser data is:%ld\n",p);
			struct task_struct *task;
			char task_name [TASK_COMM_LEN];

			printk("\nwith changing the cacheability\n");
			for_each_process(task){
				get_task_comm(task_name,task);
				if(strncmp(task_name,"mser",TASK_COMM_LEN) == 0) {
					printk("%s[%d]\n", task->comm, task->pid);
					print_mem(task);
				}
			}
		}
	}
	return 0;
}

static int mm_exp_load(void){

	/* Init PMCs on all the cores */
	init_cpu_counter();


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
	return 0;
}

static void mm_exp_unload(void)
{
        remove_proc_entry(PROF_PROCFS_NAME, NULL);
	printk("\nPrint segment information module exiting.\n");
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Golsana Ghaemi");
MODULE_DESCRIPTION ("make pages of a vma noncacheable");
