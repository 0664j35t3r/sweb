/**
 * @file Loader.cpp
 */

#include "Loader.h"
#include "console/kprintf.h"
#include "ArchThreads.h"
#include "mm/PageManager.h"
#include "ArchMemory.h"
#include "ArchCommon.h"
#include "Syscall.h"
#include "fs/VfsSyscall.h"
//#include "File.h"
#include <ustl/uvector.h>

//extern VfsSyscall vfs_syscall;

#define EI_NIDENT 16

// Elf file types

#define ET_NONE     0
#define ET_REL      1
#define ET_EXEC     2
#define ET_DYN      3
#define ET_CORE     4
#define ET_LOPROC   0xff00
#define ET_HIPROC   0xffff

// Elf machine types

#define EM_NONE     0
#define EM_M32      1
#define EM_SPARC    2
#define EM_386      3
#define EM_68K      4
#define EM_88K      5
#define EM_860      7
#define EM_MIPS     8

// Elf version

#define EV_NONE     0
#define EV_CURRENT    1

// ELF Magic numbers
// An ELF file should have MAG0,MAG1,MAG2,MAG3
// set to 0x7f,'E','L','F' respectively
//

#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION    6
#define EI_PAD      7


// ELF CLASSES

#define ELFCLASSNONE    0
#define ELFCLASS32    1
#define ELFCLASS64    2

// ELF DATAS

#define ELFDATANONE   0
#define ELFDATA2LSB   1
#define ELFDATA2MSB   2


// ELF SECTION HEADERS

#define SHN_UNDEF   0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC    0xff00
#define SHN_HIPROC    0xff1f
#define SHN_ABS     0xfff1
#define SHN_COMMON    0xfff2
#define SHN_HIRESERVE   0xffff


#define SHT_NULL    0
#define SHT_PROGBITS    1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA    4
#define SHT_HASH    5
#define SHT_DYNAMIC   6
#define SHT_NOTE    7
#define SHT_NOBITS    8
#define SHT_REL     9
#define SHT_SHLIB   10
#define SHT_DYNSYM    11
#define SHT_LOPROC    0x70000000
#define SHT_HIPROC    0x7fffffff
#define SHT_LOUSER    0x80000000
#define SHT_HIUSER    0xffffffff


// ELF Relocation types

#define R_386_NONE    0
#define R_386_32    1
#define R_386_PC32    2
#define R_386_GOT32   3
#define R_386_PLT32   4
#define R_386_COPY    5
#define R_386_GLOB_DAT    6
#define R_386_JMP_SLOT    7
#define R_386_RELATIVE    8
#define R_386_GOTOFF    9
#define R_386_GOTPC   10

// ELF PROGRAM SEGMENT TYPES

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC    2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff


#define EXECUTEABLE 1
#define WRITEABLE   2
#define READABLE    4

// thanks spoon for typing in all this

typedef uint32 Elf32_Addr;
typedef uint16 Elf32_Half;
typedef uint32 Elf32_Off;
typedef int32 Elf32_Sword;
typedef uint32 Elf32_Word;


struct sELF32_Ehdr
{
  uint8 e_ident[EI_NIDENT];
  Elf32_Half    e_type;
  Elf32_Half    e_machine;
  Elf32_Word    e_version;
  Elf32_Addr    e_entry;
  Elf32_Off   e_phoff;
  Elf32_Off   e_shoff;
  Elf32_Word    e_flags;
  Elf32_Half    e_ehsize;
  Elf32_Half    e_phentsize;
  Elf32_Half    e_phnum;
  Elf32_Half    e_shentsize;
  Elf32_Half    e_shnum;
  Elf32_Half    e_shstrndx;

};

typedef struct sELF32_Ehdr ELF32_Ehdr;

static void printElfHeader ( ELF32_Ehdr &hdr )
{
  kprintfd ( "hdr addr: %x\n",&hdr );
#define foobar(x) kprintfd("hdr " #x ": %x\n",hdr. x)
  foobar ( e_type );
  foobar ( e_machine );
  foobar ( e_version );
  foobar ( e_entry );
  foobar ( e_phoff );
  foobar ( e_shoff );
  foobar ( e_flags );
  foobar ( e_ehsize );
  foobar ( e_phentsize );
  foobar ( e_phnum );
  foobar ( e_shentsize );
  foobar ( e_shnum );
  foobar ( e_shstrndx );
}

typedef struct
{
  Elf32_Word    sh_name;
  Elf32_Word    sh_type;
  Elf32_Word    sh_flags;
  Elf32_Addr    sh_addr;
  Elf32_Off   sh_offset;
  Elf32_Word    sh_size;
  Elf32_Word    sh_link;
  Elf32_Word    sh_info;
  Elf32_Word    sh_addralign;
  Elf32_Word    sh_entsize;
}
ELF32_Shdr;


typedef struct
{
  Elf32_Word    st_name;
  Elf32_Addr    st_value;
  Elf32_Word    st_size;
  uint8 st_info;
  uint8 st_other;
  Elf32_Half    st_shndx;
}
ELF32_Sym;


typedef struct
{
  Elf32_Addr    r_offset;
  Elf32_Word    r_info;
}
ELF32_Rel;

typedef struct
{
  Elf32_Addr    r_offset;
  Elf32_Word    r_info;
  Elf32_Sword   r_added;
}
ELF32_Rela;

struct sELF32_Phdr
{
  Elf32_Word    p_type;
  Elf32_Off   p_offset;
  Elf32_Addr    p_vaddr;
  Elf32_Addr    p_paddr;
  Elf32_Word    p_filesz;
  Elf32_Word    p_memsz;
  Elf32_Word    p_flags;
  Elf32_Word    p_align;
};

typedef struct sELF32_Phdr ELF32_Phdr;


Loader::Loader ( int32 fd, Thread *thread ) : page_dir_page_(0), fd_ ( fd ),
    thread_ ( thread ), hdr_(0), phdrs_(), load_lock_("Loader::load_lock_")
{
}

Loader::~Loader()
{
  delete hdr_;
}


void Loader::initUserspaceAddressSpace()
{
  page_dir_page_ = PageManager::instance()->getFreePhysicalPage();
  debug ( LOADER,"Loader::initUserspaceAddressSpace: Got new Page no. %d\n",page_dir_page_ );
  ArchMemory::initNewPageDirectory ( page_dir_page_ );
  debug ( LOADER,"Loader::initUserspaceAddressSpace: Initialised the page dir\n" );

  uint32 page_for_stack = PageManager::instance()->getFreePhysicalPage();

  ArchMemory::mapPage ( page_dir_page_,2*1024*256-1, page_for_stack,1 );
}

void Loader::cleanupUserspaceAddressSpace()
{
  ArchMemory::freePageDirectory ( page_dir_page_ );
}

bool Loader::readHeaders()
{
  //the ehdr and the phdrs are saved as members, since they
  //are often needed

  hdr_ = new ELF32_Ehdr;

  VfsSyscall::instance()->lseek(currentThread->getWorkingDirInfo(), fd_, 0, SEEK_SET);
  if(!hdr_ || VfsSyscall::instance()->read(currentThread->getWorkingDirInfo(), fd_, reinterpret_cast<char*>(hdr_),
              sizeof(ELF32_Ehdr)) != sizeof(ELF32_Ehdr))
  {
    return false;
  }

  //checking elf-magic-numbers, 32-bit format and a few more things
  if (hdr_->e_ident[EI_MAG0]  != 0x7f
  ||  hdr_->e_ident[EI_MAG1]  != 'E'
  ||  hdr_->e_ident[EI_MAG2]  != 'L'
  ||  hdr_->e_ident[EI_MAG3]  != 'F'
  ||  hdr_->e_ident[EI_CLASS] != ELFCLASS32
  ||  hdr_->e_ident[EI_DATA]  != ELFDATA2LSB
  ||  hdr_->e_type            != ET_EXEC
  ||  hdr_->e_machine         != EM_386
  ||  hdr_->e_version         != EV_CURRENT)
  {
    return false;
  }

  if(sizeof(ELF32_Phdr) != hdr_->e_phentsize)
  {
    return false;
  }

  phdrs_.resize(hdr_->e_phnum, true);

  VfsSyscall::instance()->lseek(currentThread->getWorkingDirInfo(), fd_, hdr_->e_phoff, SEEK_SET);

  if(VfsSyscall::instance()->read(currentThread->getWorkingDirInfo(), fd_, reinterpret_cast<char*>(&phdrs_[0]), hdr_->e_phnum*sizeof(ELF32_Phdr))
      != static_cast<int32>(sizeof(ELF32_Phdr)*hdr_->e_phnum))
  {
    return false;
  }

  return true;
}

bool Loader::loadExecutableAndInitProcess()
{
  debug ( LOADER,"Loader::loadExecutableAndInitProcess: going to load an executable\n" );

  initUserspaceAddressSpace();

  if(!readHeaders())
    return false;

  debug ( LOADER,"loadExecutableAndInitProcess: Entry: %x, num Sections %x\n",hdr_->e_entry, hdr_->e_phnum );
  if ( isDebugEnabled ( LOADER ) )
    printElfHeader ( *hdr_ );

  ArchThreads::createThreadInfosUserspaceThread (
        thread_->user_arch_thread_info_,
        hdr_->e_entry,
        2U*1024U*1024U*1024U - sizeof ( pointer ),
        thread_->getStackStartPointer()
  );

  ArchThreads::setPageDirectory ( thread_, page_dir_page_ );

  return true;
}

struct PagePart
{
  uint32 page_byte;
  uint32 vaddr;
  uint32 length;
};

#define ADDRESS_BETWEEN(Value, LowerBound, UpperBound) \
  ((((uint32)Value) >= ((uint32)LowerBound)) && (((uint32)Value) < ((uint32)UpperBound)))

void Loader::loadOnePageSafeButSlow ( uint32 virtual_address )
{
  uint32 virtual_page = virtual_address / PAGE_SIZE;

  MutexLock loadlock(load_lock_);
  //check if page has not been loaded meanwhile
  if(ArchMemory::checkAddressValid(page_dir_page_, virtual_address))
  {
    debug ( LOADER,"loadOnePageSafeButSlow: Page %d (virtual_address=%d) has already been mapped, probably by another thread between pagefault and reaching loader.\n",virtual_page,virtual_address );
    return;
  }

  debug ( LOADER,"loadOnePageSafeButSlow: going to load virtual page %d (virtual_address=%d) for %d:%s\n",virtual_page,virtual_address,currentThread->getPID(),currentThread->getName() );

  debug ( LOADER,"loadOnePage: Sizeof %d %d %d %d\n",sizeof ( uint64 ),sizeof ( uint32 ),sizeof ( uint16 ),sizeof ( uint8 ) );
  debug ( LOADER,"loadOnePage: Num ents: %d\n",hdr_->e_phnum );
  debug ( LOADER,"loadOnePage: Entry: %x\n",hdr_->e_entry );

  uint32 page = 0;
  uint32 i=0;
  uint32 k=0;
  uint32 written=0;

  ustl::vector<PagePart> byte_map;
  PagePart part;
  uint32 min_value = 0xFFFFFFFF;
  uint32 max_value = 0;

  for ( i=0; i < PAGE_SIZE; ++i )
  {
    uint32 load_byte_from_address = virtual_page*PAGE_SIZE + i;
    uint32 found = 0;
    for ( k=0;k < hdr_->e_phnum; ++k )
    {
      ELF32_Phdr& h = phdrs_[k];

      debug ( LOADER,"loadOnePage: PHdr[%d].vaddr=%x .paddr=%x .type=%x .memsz=%x .filez=%x .poff=%x\r\n",k,h.p_vaddr,h.p_paddr,h.p_type,h.p_memsz,h.p_filesz,h.p_offset );

      if (ADDRESS_BETWEEN(load_byte_from_address, h.p_paddr, h.p_paddr + h.p_filesz))
      {
        uint32 byte_to_load = h.p_offset + load_byte_from_address - h.p_paddr;

        if(byte_to_load < min_value)
          min_value = byte_to_load;

        uint32 load_count = (h.p_paddr + h.p_filesz) - load_byte_from_address;
        if (load_count + i > PAGE_SIZE)
            load_count = PAGE_SIZE - i;
        part.page_byte = i;
        part.vaddr = byte_to_load;
        part.length = load_count;
        i += load_count - 1;

        if(byte_to_load + load_count > max_value)
          max_value = byte_to_load + load_count;

        //its VERY MUCH more efficient to only search the bytes, save them in the array byte_map,
        //and read ONCE from the executable; its very expensive to read every byte single from harddisk
        //(we have to read a full zone) -> for this we also need the max- and min-byte from file we have to load

        byte_map.push_back(part);

        ++written;
        ++found;
      }
      // bss is not in the file but in memory
      else if (ADDRESS_BETWEEN(load_byte_from_address, h.p_paddr, h.p_paddr + h.p_memsz))
      {
        // skip ahead to end of section
        i += (h.p_paddr + h.p_memsz) - load_byte_from_address - 1;
        debug ( LOADER,"In segment but not on file, this is .bss\n" );
        ++found;
        ++written;
      }
    }

    if ( !found )
    {
      debug ( LOADER,"Byte not found, byte virtual address is %x\n",load_byte_from_address );
      // this is expected behaviour. Our Loader tries to find every bytes on a needed page in the ELF Header.
      // Of course, often only parts of a page are listed in the ELF Hedaer
    }
    else if ( found >1 )
    {
      kprintfd ( "Loader::loadOnePageSafeButSlow:EEEEEEEEEEEERRRRRRRROR, found the byte (%x) in two different segments\n", load_byte_from_address );
    }
  }

  if ( !written )
  {
    kprintfd ( "Loader::loadOnePageSafeButSlow: ERROR Request for Unknown Memory Location: v_adddr=%x, v_page=%d\n",virtual_address,virtual_page);
    //free unmapped page
    Syscall::exit ( 9997 );
  }

  //in this case all bytes are in bss-section, but not in file
  if(max_value == 0 && min_value == 0xffffffff)
  {
    debug(LOADER, "%x is in .bss\n", virtual_address);
    page = PageManager::instance()->getFreePhysicalPage();
    ArchCommon::bzero ( ArchMemory::get3GBAddressOfPPN ( page ),PAGE_SIZE,false );
    ArchMemory::mapPage ( page_dir_page_, virtual_page, page, true );
    return;
  }

  //read once the bytes we need (and a few more, probably, depends on elf-format)
  uint32 buffersize = max_value - min_value;
  char* buffer = 0;
  char page_buffer[PAGE_SIZE];
  assert(buffersize <= PAGE_SIZE && "how could the other case occur and how would we handle it?");
  if (buffersize <= PAGE_SIZE)
  {
    buffer = page_buffer;
  }
  else
  {
    buffer = new char[buffersize];
  }
  debug(PM, "buffer is %d bytes long\n", buffersize);

  if(!buffer)
  {
    kprintfd ( "Loader::loadOnePageSafeButSlow: ERROR not enough heap memory\n");
    //free unmapped page
    Syscall::exit ( 9996 );
  }


  VfsSyscall::instance()->lseek(currentThread->getWorkingDirInfo(), fd_, min_value, SEEK_SET);
  int32 bytes_read = VfsSyscall::instance()->read(currentThread->getWorkingDirInfo(), fd_, buffer, max_value - min_value);


  if(bytes_read != static_cast<int32>(max_value - min_value))
  {
    kprintfd ( "Loader::loadOnePageSafeButSlow: ERROR part of executable not present in file: v_adddr=%x, v_page=%d\n", virtual_address, virtual_page);
    //free buffer
    if (buffersize > PAGE_SIZE)
      delete[] buffer;
    Syscall::exit ( 9998 );
  }
  page = PageManager::instance()->getFreePhysicalPage();
  debug(PM, "got new page %x\n", page);
  ArchCommon::bzero ( ArchMemory::get3GBAddressOfPPN ( page ),PAGE_SIZE,false );
  debug(PM, "bzero!\n");
  uint8* dest = reinterpret_cast<uint8*> (ArchMemory::get3GBAddressOfPPN ( page ));
  debug(PM, "copying %d elements\n", byte_map.size());
  written = 0;
  for(i=0; i < byte_map.size(); i++)
  {
    part = byte_map[i];

    debug(PM, "copying %dth element from %x to %x ;   page byte: %d, length: %d\n", i, buffer+part.vaddr - min_value, dest + part.page_byte, part.page_byte, part.length);

    assert(part.vaddr - min_value + part.length <= buffersize);
    assert(part.page_byte + part.length <= PAGE_SIZE);
    ArchCommon::memcpy((pointer)(dest + part.page_byte), (pointer)(buffer + part.vaddr - min_value), part.length);
    written += part.length;
  }

  if (buffersize > PAGE_SIZE)
    delete[] buffer;

  ArchMemory::mapPage ( page_dir_page_, virtual_page, page, true );
  debug ( PM,"loadOnePageSafeButSlow: wrote a total of %d bytes\n",written );

}
