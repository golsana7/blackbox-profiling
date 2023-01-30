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

#define NEW_SECTION_NAME ".profile"

/* This function performs all the ELF manipulation steps in the
 * required order. */
void add_profile_section(const char *elf_name, const char *profile);

/* This function reads the content of the profile file and returns a
 * buffer with its data content. */
void* read_profile_file(const char* profile, size_t* sec_size);

/* This function adds a new section name at the end of the existing
 * section names section. */
size_t add_section_name(Elf * elf, char * new_section_name);

/* This function computes the offset of the section to be added. It
 * does so by scanning through the list of section and finding the one
 * with the largest offset. The size of that section is also added. */
size_t compute_section_offset(Elf * elf);

/* This function adjusts the offset of the section header table to
 * account for any addition to the file. */
void adjust_sec_table_offset(Elf * elf, size_t extra_offset);

/* This function creates a new section and sets it payload to the
 * content of the profile file. */
Elf_Scn * add_section_payload(Elf * elf, char * profile_content, size_t profile_length);

/* This function inserts a new entry in the section header table for
 * the newly created profile section. */
void update_section_header(Elf_Scn * scn, size_t profile_length, size_t name_offset,
			   size_t section_offset);


int main(int argc, char* argv[])
{
	const char *elf_file;
	const char* profile_path;

	if (argc < 3) {
		fprintf(stderr, "Not enough parameters.\n");
		fprintf(stderr, "Usage: %s <input_elf> <profile>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	/* Take arguments from command line */
	elf_file = argv[1]; //original elf we want to augment
	profile_path = argv[2]; //profile info to be added
	
	/* Set ELF version to the current one (default) */
	elf_version(EV_CURRENT);

	add_profile_section(elf_file, profile_path);
	
	return 0;
}

/* This function reads the content of the profile file and returns a
 * buffer with its data content. */
void* read_profile_file(const char* profile, size_t* sec_size)
{
	FILE * fr;
	void * buf;
	size_t bytes_read;
	struct stat profile_stats;
  
	//file that we want its info (profile file)
	if(stat(profile, &profile_stats) != 0)
	{
		fprintf(stderr, "Unable to retrieve profile file size.\n");
		exit(EXIT_FAILURE);
	}

	/* Return file size to callee */
	*sec_size = profile_stats.st_size;

	buf = malloc (*sec_size);

	if (buf == NULL)
	{
		fprintf(stderr, "Unable allocate buffer data of %ld bytes\n", *sec_size);
		exit (EXIT_FAILURE);
	}
	//printf("*sec_size is : %ld\n", *sec_size);
	fr = fopen(profile, "rb");
	if (!fr)
	{
		fprintf(stderr, "Unable to open profile file %s.\n", profile);
		exit(EXIT_FAILURE);
	}

	
	bytes_read = fread(buf, 1, *sec_size, fr);
	if (bytes_read != *sec_size) {
		fprintf(stderr, "Unable to read full profile file. Size: %ld != Bytes read: %ld\n",
			*sec_size, bytes_read);
		fclose(fr);
		exit(EXIT_FAILURE);
	}

	fclose(fr);

	return buf;
}

/* This function adds a new section name at the end of the existing
 * section names section. */
size_t add_section_name(Elf * elf, char * new_section_name)
{
	size_t name_offset;

	/* index of each section name on string table (shstrtab) */
	size_t shstrndx; 
	if(elf_getshdrstrndx (elf, &shstrndx) < 0)
	{
		fprintf(stderr, "Cannot get shstrtab scn: %s\n",elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}

	/* Content of the descriptor for a section in section header table*/
	Elf_Scn *shstrtab_scn = elf_getscn (elf, shstrndx);
	if(shstrtab_scn == NULL)
	{
		fprintf(stderr, "Couldn't get shstrtab scn: %s\n", elf_errmsg (-1));
		exit (EXIT_FAILURE);
	}

	Elf_Data *shstrtab_data = elf_getdata(shstrtab_scn, NULL); //data of shstrtab entry (which is content of section names string table)
	if (shstrtab_data == NULL)
	{
		fprintf(stderr, "Couldn't get shstrtab data: %s\n", elf_errmsg (-1));
		exit (EXIT_FAILURE);
	}

	name_offset = shstrtab_data->d_size;
	size_t new_shstrtab_size = (shstrtab_data->d_size + strlen(new_section_name) + 1);

	printf("Old section size: 0x%lx, New section size: 0x%lx\n",
	       shstrtab_data->d_size, new_shstrtab_size);
	
	void * new_shstrtab_buf = malloc(new_shstrtab_size);// 
	if (new_shstrtab_buf == NULL)
	{
		printf ("Couldn't allocate new shstrtab data d_buf\n");
		exit (EXIT_FAILURE);
	}
	
	/* Make sure that the buffer is zero-terminated */
	memcpy (new_shstrtab_buf, shstrtab_data->d_buf, shstrtab_data->d_size);
	strcpy (new_shstrtab_buf + name_offset, new_section_name);

	shstrtab_data->d_size = new_shstrtab_size;
	shstrtab_data->d_buf = new_shstrtab_buf;

	/* Mark this data as dirty to tell libelf to write the modified buffer into the file */
	elf_flagdata(shstrtab_data, ELF_C_SET, ELF_F_DIRTY);
	
	/* Finally update the descriptor of the section names header in the section header table. */
	GElf_Shdr * shdr = (GElf_Shdr *)malloc(sizeof(GElf_Shdr));
	gelf_getshdr (shstrtab_scn, shdr);
	if (shdr == NULL) {
		fprintf(stderr, "Unable to retrieve section header descriptor.\n");
		exit(EXIT_FAILURE);
	}
	
	shdr->sh_size = new_shstrtab_size;

	if (gelf_update_shdr(shstrtab_scn, shdr) == 0) {
		fprintf(stderr, "Unable to update section header.\n");
		exit(EXIT_FAILURE);
	}

	size_t pos;
	printf("INFO: Dump of new shstrtab content\n---\n");
	for (pos = 0; pos < new_shstrtab_size; ++pos) 
		printf("%c", ((char *)new_shstrtab_buf)[pos]?((char *)new_shstrtab_buf)[pos]:'_');
	printf("\n---\n");	
	return name_offset;
}


size_t compute_section_offset(Elf * elf)
{
	/* Figure out the offset and size of the last section */
	/* index of each section name on string table (shstrtab) */
	GElf_Ehdr ehdr;
	size_t sec_idx, last_sec_off = 0, last_sec_size;
	
	if(gelf_getehdr (elf, &ehdr) == NULL)
	{
		fprintf(stderr, "Unable to retrieve ELF header: %s\n",elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}
	
	size_t shdrnum = ehdr.e_shnum;

	/* Loop over all the sections to find the one with the
	 * greatest offset */
	for (sec_idx = 0; sec_idx < shdrnum; ++sec_idx) {
		/* Content of the descriptor for the current section. */
		Elf_Scn *cur_scn = elf_getscn (elf, sec_idx);
		if(cur_scn == NULL)
		{
			fprintf(stderr, "Couldn't get section %ld: %s\n", sec_idx, elf_errmsg (-1));
			exit (EXIT_FAILURE);
		}
		
		/* And the descriptor in the section header table */
		GElf_Shdr * cur_shdr = (GElf_Shdr *)malloc(sizeof(GElf_Shdr));
		gelf_getshdr (cur_scn, cur_shdr);
		
		if (cur_shdr == NULL) {
			fprintf(stderr, "Unable to retrieve section header descriptor for section %ld.\n",
				sec_idx);
			exit(EXIT_FAILURE);
		}

		if (cur_shdr->sh_offset > last_sec_off) {
			last_sec_off = cur_shdr->sh_offset;
			last_sec_size = cur_shdr->sh_size;
		}
		
		
		free(cur_shdr);
	}
	
	
	/* Compute the offset of the new section as last_scn offset + last_scn size */
	size_t profile_offset = last_sec_off + last_sec_size;

	printf("INFO: New section will be placed at offset 0x%lx.\n", profile_offset);
	
	return profile_offset;

}

void adjust_sec_table_offset(Elf * elf, size_t extra_offset)
{
	GElf_Ehdr ehdr;
	
	if (gelf_getehdr (elf, &ehdr) == NULL)
	{
		fprintf(stderr, "Unable to retrieve ELF header: %s\n",elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}

	ehdr.e_shoff += extra_offset + strlen(NEW_SECTION_NAME) + 1;

	if (gelf_update_ehdr(elf, &ehdr) == 0)
	{
		fprintf(stderr, "Unable to update ELF header: %s\n",elf_errmsg(-1));
		exit (EXIT_FAILURE);		
	}
}

Elf_Scn * add_section_payload(Elf * elf, char * profile_content, size_t profile_length)
{	
	/* Add new profile section */
	Elf_Scn *scn = elf_newscn (elf);
	if (scn == NULL)
	{
		fprintf (stderr, "Unable to create extra section %s\n", elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}

	/* Add data to the section */
	Elf_Data *data = elf_newdata (scn);
	if (data == NULL)
	{
		fprintf (stderr, "Couldn't create new section data : %s\n", elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}
	
	data->d_size = profile_length;
	data->d_buf = profile_content;
	data->d_type = ELF_T_BYTE;
	data->d_align = 1;

	printf("INFO: Created new section with index %ld and offset %ld\n", elf_ndxscn(scn), data->d_off);
	
	return scn;

}

void update_section_header(Elf_Scn * scn, size_t profile_length, size_t name_offset, size_t section_offset)
{
	/* Finally update the descriptor of the section names header in the section header table. */
	GElf_Shdr * shdr = (GElf_Shdr *)malloc(sizeof(GElf_Shdr));
	gelf_getshdr (scn, shdr);

	if (shdr == NULL) {
		fprintf(stderr, "Unable to retrieve section header descriptor.\n");
		exit(EXIT_FAILURE);
	}

	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = 0;
	shdr->sh_addr = 0;
	shdr->sh_link = SHN_UNDEF;
	shdr->sh_info = SHN_UNDEF;
	shdr->sh_addralign = 1;
	shdr->sh_entsize = 0;
	shdr->sh_size = profile_length;
	shdr->sh_name = name_offset;
	shdr->sh_offset = section_offset;
	
	if (gelf_update_shdr(scn, shdr) == 0) {
		fprintf(stderr, "Unable to update section header.\n");
		exit(EXIT_FAILURE);
	}
	
}

void add_profile_section(const char *elf_name, const char *profile)
{

	/* This function operates according to the following logic.
	 * 1. Attempt to retrieve the full content of the profile file.
	 * 2. Modify the content of the Section Names Section to add the .profile entry.
	 * 3. Add new section with profile content into file payload.
	 * 4. Add new section entry into sectio header table.
	 * 5. Update number of section in program header. 
	 */


	/* Step 1: Read profile file. */
	size_t profile_length;
	char * profile_content = read_profile_file(profile, &profile_length);

	/* Prologue to manipulate the main ELF file. */

	/* Main handler for ELF manipulation */
	Elf * elf;

	
	int elf_fd = open(elf_name, O_RDWR);
	if (elf_fd < 0)
	{
		fprintf(stderr, "Couldn't open ELF file '%s' : %s\n",elf_name, strerror(errno));
		exit (EXIT_FAILURE);
	}

	/*begining of modifying ELF file*/
	elf = elf_begin(elf_fd, ELF_C_RDWR, NULL);
	if (elf == NULL)
	{
		fprintf(stderr, "Couldn't open ELF file '%s' : %s\n",elf_name, elf_errmsg(-1));
		exit (EXIT_FAILURE);
	}

	/* Tell libelf to not change the overall elf layout */
	if (elf_flagelf(elf, ELF_C_SET, ELF_F_LAYOUT) == 0)
	{
		fprintf(stderr, "Unable to flag ELF layput as manually managed: %s\n", elf_errmsg(-1));
		exit (EXIT_FAILURE);		
	}
	
	/* Step 2: Add .profile entry in Section Names Section. */
	size_t name_offset = add_section_name(elf, NEW_SECTION_NAME);

	/* Compute the offset in the file where the new section should be placed */
	size_t section_offset = compute_section_offset(elf);
	
	/* Step 3: Add a new section with the content of the profile */
	Elf_Scn * new_section = add_section_payload(elf, profile_content, profile_length);

	/* Step 4: Update section header with the descriptor of the new section */
	update_section_header(new_section, profile_length, name_offset, section_offset);

	/* Step 5: Compute the new offset where to relocate the section headers table */
	adjust_sec_table_offset(elf, profile_length);

	/* Make sure we tell libelf that the section headers table has been updated */
	if (elf_flagelf(elf, ELF_C_SET, ELF_F_DIRTY) == 0){
		fprintf(stderr,"Unable to flag ELF as dirty: %s\n", elf_errmsg(-1));
	}
	
	/* Commit changes to the elf. */
	// Write everything to disk.
	if (elf_update (elf, ELF_C_WRITE) < 0)
	{
		fprintf (stderr, "Failure in elf_update: %s\n", elf_errmsg (-1));
		exit (EXIT_FAILURE);
	}

	if (elf_end (elf) != 0)
	{
		fprintf (stderr, "Couldn't cleanup elf '%s': %s\n", elf_name, elf_errmsg (-1));
		exit (EXIT_FAILURE);
	}

	if (close (elf_fd) != 0)
	{
	        fprintf (stderr, "Couldn't close '%s': %s\n", elf_name, strerror (errno));
		exit (EXIT_FAILURE);
	}

	free (profile_content);	
}

