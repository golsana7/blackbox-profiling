#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <stdlib.h>
#include <sys/mman.h>   /* Memory locking functions */
#include <fcntl.h>
#include <stdbool.h>
#include <sched.h>
#include <limits.h>
#include <inttypes.h>

/* this part for reading elf */
#include <sys/types.h>
#include <sys/stat.h>
#include <libelf.h>
#include <gelf.h>
#include <sched.h>
#include <errno.h>
#include <sys/stat.h>


//#include ELFUTILS_HEADER(elf)

void* read_file(char* profile, size_t* sec_size)
{
  FILE *fr,*fw;
  void *buf;
  
  struct stat profile_stats;
  printf("TEST inside read_file\n");
  
  //file that we want its info (profile file)
  if(stat(profile, &profile_stats) == 0)
    {
      //filesize
      printf("File size is: %ld\n", profile_stats.st_size);
    }
  *sec_size = profile_stats.st_size; //sec_size is an address, so *sec_size is the content

  buf = malloc (*sec_size);

  if (buf == NULL)
    {
      printf ("cannot allocate buffer data of %ld bytes\n", sec_size);
      exit (-1); // or return NULL
    }
  //printf("*sec_size is : %ld\n", *sec_size);
  fr = fopen(profile, "rb");
  if (!fr)
    {
      printf("unable to open %s\n", profile);
    }
  //fw = fopen("test.bin", "wb");
  //printf("sec_size is : %zd\n", sec_size);
  fread(buf, *sec_size, 1, fr);
  //fwrite(buf, *sec_size,1, fw);

  fclose(fr);

  return buf;
  //fclose(fw);
  
}


/* shstrndx is special, might overflow into section zero header sh_link.  */
static int setshstrndx (Elf *elf, size_t ndx)
{
  printf ("setshstrndx: %zd\n", ndx);

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      return -1;
    }
  if (ndx < SHN_LORESERVE)
    {
      ehdr->e_shstrndx = ndx;
    }
  else
    {
      ehdr->e_shstrndx = SHN_XINDEX;
      Elf_Scn *zscn = elf_getscn (elf, 0);
      GElf_Shdr zshdr_mem;
      GElf_Shdr *zshdr = gelf_getshdr (zscn, &zshdr_mem);
      if (zshdr == NULL)
	return -1;
      zshdr->sh_link = ndx;
      if (gelf_update_shdr (zscn, zshdr) == 0)
	return -1;
    }
   if (gelf_update_ehdr (elf, ehdr) == 0)
     return -1;

   return 0;
}

void add_elf_sections(const char *elf_name, char *profile,  size_t nr)
{
        printf("adding section to %s\n",elf_name);
	
	size_t sec_size; // size of the new section whichis gonna be added
	int fd = open(elf_name, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Couldn't open ELF file '%s' : %s\n",elf_name, strerror(errno));
		exit (1);
	}

	/*begining of modifying ELF file*/
	Elf *elf = elf_begin(fd,ELF_C_RDWR, NULL);
	if (elf == NULL)
	{
		fprintf(stderr, "Couldn't open ELF file '%s' : %s\n",elf_name, elf_errmsg(-1));
		exit (1);
	}
	
	/* We will add a new shstrtab section with two new names at the end.
        Just get the current shstrtab table and add two entries '.extra'
	and '.old_shstrtab' at the end of the table, so all existing indexes
	are still valid*/
	size_t shstrndx; // index of each section name on string table (shstrtab)
	if(elf_getshdrstrndx (elf, &shstrndx) < 0)
	{
		printf("cannot get shstrtab scn: %s\n",elf_errmsg(-1));
		exit (-1);
	}

	/*descriptor for a section in section header table*/
	Elf_Scn *shstrtab_scn = elf_getscn (elf, shstrndx);// get the section which shdrndx says (shstrtab entry) from sec hdr table
	if(shstrtab_scn == NULL)
	{
		printf("couldn't get shstrtab scn: %s\n", elf_errmsg (-1));
		exit(1);
	}

	Elf_Data *shstrtab_data = elf_getdata(shstrtab_scn, NULL); //data of shstrtab entry (which is content of section names string table)
	if (shstrtab_data == NULL)
	{
		printf("couldn't get shstrtab data: %s\n", elf_errmsg (-1));
		exit (1);
	}

	size_t new_shstrtab_size = (shstrtab_data->d_size + strlen (".profile") + 1
				    /*+ strlen (".old_shstrtab") + 1*/);
	void *new_shstrtab_buf = malloc (new_shstrtab_size);// 
	if (new_shstrtab_buf == NULL)
	  {
	    printf ("couldn't allocate new shstrtab data d_buf\n");
	    exit (1);
	  }

	memcpy (new_shstrtab_buf, shstrtab_data->d_buf, shstrtab_data->d_size);
	
	size_t extra_idx = shstrtab_data->d_size; //size of original name table
	//size_t old_shstrtab_idx = extra_idx + strlen (".extra") + 1;
	strcpy (new_shstrtab_buf + extra_idx, ".profile");
	//strcpy (new_shstrtab_buf + old_shstrtab_idx, ".old_shstrtab");

	/* Change the name of the old shstrtab section, because elflint
	   has a strict check on the name/type for .shstrtab.  */
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (shstrtab_scn, &shdr_mem);
	if (shdr == NULL)
	  {
	    printf ("cannot get header for old shstrtab section: %s\n", elf_errmsg (-1));
	    exit (-1);
	  }

	size_t shstrtab_idx = shdr->sh_name; //offset of name of this special section (sec name str tab)
	///shdr->sh_name = old_shstrtab_idx;

	 if (gelf_update_shdr (shstrtab_scn, shdr) == 0)
	   {
	     printf ("cannot update old shstrtab section header: %s\n",elf_errmsg (-1));
	     exit (-1);
	   }

	
	 void *buf;
	 size_t bufsz;
	 buf  = read_file(profile, &sec_size);
	 // printf ("sec_size is: %ld\n", sec_size);	
	 bufsz = sec_size;
	

	  // Add lots of .extra/.profile sections...
	 Elf_Scn *scn = elf_newscn (elf);
	 if (scn == NULL)
	   {
	     printf ("cannot create .extra section %s\n", elf_errmsg(-1));
	     exit (1);
	   }
	 Elf_Data *data = elf_newdata (scn);
	  if (data == NULL)
	    {
	      printf ("couldn't create new section data : %s\n", elf_errmsg(-1));
	      exit (1);
	    }

	  data->d_size = bufsz;
	  data->d_buf = buf;
	  data->d_type = ELF_T_BYTE;
	  data->d_align = 1;

	  shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    {
	      printf ("cannot get header for new section: %s\n", elf_errmsg (-1));
	      exit (1);
	    }

	  shdr->sh_type = SHT_PROGBITS;
	  shdr->sh_flags = 0;
	  shdr->sh_addr = 0;
	  shdr->sh_link = SHN_UNDEF;
	  shdr->sh_info = SHN_UNDEF;
	  shdr->sh_addralign = 1;
	  shdr->sh_entsize = 0;
	  shdr->sh_size = data->d_size;
	  shdr->sh_name = extra_idx;

	  if (gelf_update_shdr (scn, shdr) == 0)
	    {
	      printf ("cannot update new section header: %s\n", elf_errmsg(-1));
	      exit (1);
	    }

	 // Create new shstrtab section.
	  ///	 Elf_Scn *new_shstrtab_scn = elf_newscn (elf);
	  ///if (new_shstrtab_scn == NULL)
	  ///{
	  ///printf ("cannot create new shstrtab section: %s\n", elf_errmsg (-1));
	  ///exit (1);
	  ///}

	  ///Elf_Data *new_shstrtab_data = elf_newdata (new_shstrtab_scn);
	  ///if (new_shstrtab_data == NULL)
	  ///{
	  ///printf ("couldn't create new shstrtab section data: %s\n",elf_errmsg(-1));
	  /// exit (1);
	  ///}

	  /* new_*/shstrtab_data->d_size = new_shstrtab_size;
	  /*new_*/shstrtab_data->d_buf = new_shstrtab_buf;
	  ///new_shstrtab_data->d_type = ELF_T_BYTE;
	  ///new_shstrtab_data->d_align = 1;

	 //bc each time u want to modify it, first u should get it
	  shdr = gelf_getshdr (/*new_*/shstrtab_scn, &shdr_mem);
	 if (shdr == NULL)
	   {
	     printf ("cannot get header for new shstrtab section: %s\n", elf_errmsg(-1));
	     exit (1);
	   }

	 shdr->sh_type = SHT_STRTAB;
	 shdr->sh_flags = 0;
	 shdr->sh_addr = 0;
	 shdr->sh_link = SHN_UNDEF;
	 shdr->sh_info = SHN_UNDEF;
	 shdr->sh_addralign = 1;
	 shdr->sh_entsize = 0;
	 shdr->sh_size = new_shstrtab_size;
	 shdr->sh_name = shstrtab_idx;

	 // Finished new shstrtab section, update the header.
	 if (gelf_update_shdr (/*new_*/shstrtab_scn, shdr) == 0)
	   {
	     printf ("cannot update new shstrtab section header: %s\n", elf_errmsg(-1));
	     exit (1);     
	   }

	 // Set it as the new shstrtab section to get the names correct.
	 ///size_t new_shstrndx = elf_ndxscn (new_shstrtab_scn);
	 ///if (setshstrndx (elf, new_shstrndx) < 0)
	 ///{
	 ///  printf ("cannot set shstrndx: %s\n", elf_errmsg (-1));
	 ///  exit (1);
	 ///}

	  // Write everything to disk.
	  if (elf_update (elf, ELF_C_WRITE) < 0)
	    {
	      printf ("Failure in elf_update: %s\n", elf_errmsg (-1));
	      exit (1);
	    }

	  if (elf_end (elf) != 0)
	    {
	      printf ("couldn't cleanup elf '%s': %s\n", elf_name, elf_errmsg (-1));
	      exit (1);
	    }

	   if (close (fd) != 0)
	     {
	        printf ("couldn't close '%s': %s\n", elf_name, strerror (errno));
		exit (1);
	     }

	   free (buf);
	   free (new_shstrtab_buf);
	
}

int main(int argc, char* argv[])
{
	int i, fd;
	const char *elf_file;
	char* profile;

	
	elf_file = argv[1]; //original elf we want to augment
	profile = argv[2]; //profile info to be added


	if (!elf_file || !profile) {
		printf("Invalid ELF path or profile info.");
		return 0;
	}
	
	/* Set ELF version to the current one (default) */
	elf_version(EV_CURRENT);

	add_elf_sections(elf_file, profile, 1);
	
	return 0;
}
