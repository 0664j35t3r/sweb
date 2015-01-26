/**
 * @file MinixFSSuperblock.cpp
 */

#include "FileDescriptor.h"
#include "MinixFSSuperblock.h"
#include "MinixFSInode.h"
#include "MinixFSFile.h"
#include "Dentry.h"

#include "MinixFSTypes.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>
#include <iostream>
#include "Buffer.h"
#include <assert.h>

#define ROOT_NAME "/"

extern std::list<FileDescriptor*> global_fd;


MinixFSSuperblock::MinixFSSuperblock ( Dentry* s_root, uint32 s_dev, uint64 offset ) : Superblock ( s_root, s_dev )
{
  //BDManager::getInstance()->getDeviceByNumber ( s_dev )->setBlockSize ( BLOCK_SIZE );
  //read Superblock data from disc
  //std::cout << "dev: " << s_dev << std::endl;
  offset_ = offset;
  Buffer *buffer = new Buffer ( BLOCK_SIZE );
  readBlocks ( 1, 1, buffer->getBuffer() );
  s_num_inodes_ = buffer->get2Bytes ( 0 );
  //assert(s_num_inodes_);
  s_num_zones_ = buffer->get2Bytes ( 2 );
  assert(s_num_zones_);
  s_num_inode_bm_blocks_ = buffer->get2Bytes ( 4 );
  assert(s_num_inode_bm_blocks_);
  s_num_zone_bm_blocks_ = buffer->get2Bytes ( 6 );
  assert(s_num_zone_bm_blocks_);
  s_1st_datazone_ = buffer->get2Bytes ( 8 );
  s_log_zone_size_ = buffer->get2Bytes ( 10 );
  s_max_file_size_ = buffer->get4Bytes ( 12 );
  s_magic_ = buffer->get2Bytes ( 16 );

  //printf ( "s_num_inodes_ : %d\ns_num_zones_ : %d\ns_num_inode_bm_blocks_ : %d\ns_num_zone_bm_blocks_ : %d\ns_1st_datazone_ : %d\ns_log_zone_size_ : %d\ns_max_file_size_ : %d\ns_magic_ : %d\n",s_num_inodes_,s_num_zones_,s_num_inode_bm_blocks_,s_num_zone_bm_blocks_,s_1st_datazone_,s_log_zone_size_,s_max_file_size_,s_magic_ );


  delete buffer;

  //create Storage Manager
  uint32 bm_size = s_num_inode_bm_blocks_ + s_num_zone_bm_blocks_;
  Buffer *bm_buffer = new Buffer ( BLOCK_SIZE*bm_size );
  readBlocks ( 2, bm_size, bm_buffer->getBuffer() );
  //debug ( M_SB,"---creating Storage Manager\n" );
  storage_manager_ = new MinixStorageManager ( bm_buffer->getBuffer(),
                     s_num_inode_bm_blocks_,
                     s_num_zone_bm_blocks_,
                     s_num_inodes_, s_num_zones_ );
  /*if ( isDebugEnabled ( M_SB ) )
  {
    storage_manager_->printBitmap();
  }*/
  delete bm_buffer;

  //std::cout << storage_manager_->isInodeSet ( 0 ) << " " << storage_manager_->isInodeSet ( 1 ) << " " <<
    //storage_manager_->isInodeSet ( 2 ) << " " << storage_manager_->isInodeSet ( 3 ) << " " << storage_manager_->isInodeSet ( 4 ) << " " << storage_manager_->isInodeSet ( 5 ) << std::endl;

  initInodes();
}


void MinixFSSuperblock::initInodes()
{
  MinixFSInode *root_inode = getInode ( 1 );
  Dentry *root_dentry = new Dentry ( ROOT_NAME );
  if ( s_root_ )
  {
//     root_dentry->setMountPoint(s_root_);
    s_root_->setMountPoint ( root_dentry );
    mounted_over_ = s_root_; // MOUNT
    s_root_ = root_dentry;
  }
  else
  {
    mounted_over_ = root_dentry; // ROOT
    s_root_ = root_dentry;
  }
  root_dentry->setParent ( root_dentry );
  root_inode->i_dentry_ = root_dentry;
  root_dentry->setInode ( root_inode );

  all_inodes_.pushBack ( root_inode );
  //read children from disc
  root_inode->loadChildren();

}

MinixFSInode* MinixFSSuperblock::getInode ( uint16 i_num, bool &is_already_loaded )
{
  for(uint32 i=0; i < all_inodes_.getLength(); i++)
  {
    if(((MinixFSInode*)all_inodes_.at(i))->i_num_ == i_num)
    {
      is_already_loaded = true;
      return (MinixFSInode*) all_inodes_[i];
    }
  }
  return getInode(i_num);
}

MinixFSInode* MinixFSSuperblock::getInode ( uint16 i_num )
{
  if(i_num >= storage_manager_->getNumUsedInodes())
  {
    return 0;
  }
  //std::cout << "getInode(" << i_num << ") called!" << std::endl;
  if(!storage_manager_->isInodeSet ( i_num ))
  {
    if(i_num == 1)
      assert ( storage_manager_->isInodeSet ( 1 ) );

    return 0;
  }
  uint32 inodes_start = s_num_inode_bm_blocks_ + s_num_zone_bm_blocks_ + 2;
  uint32 inode_block_num = inodes_start + ( i_num - 1 ) / INODES_PER_BLOCK;
  MinixFSInode *inode = 0;
  Buffer *ibuffer = new Buffer ( BLOCK_SIZE );
  //debug ( M_SB,"getInode::reading block num: %d\n", inode_block_num );
  readBlocks ( inode_block_num, 1, ibuffer->getBuffer() );
  //debug ( M_SB,"getInode:: returned reading block num: %d\n", inode_block_num );
  uint32 offset = ( ( i_num - 1 ) % INODES_PER_BLOCK ) *INODE_SIZE;
  //debug ( M_SB,"getInode:: setting offset: %d\n", offset );
  ibuffer->setOffset ( offset );
  uint16 *i_zones = new uint16[9];
  for ( uint32 num_zone = 0; num_zone < 9; num_zone ++ )
  {
    i_zones[num_zone] = ibuffer->get2Bytes ( 14 + ( num_zone * 2 ) );
  }
  //debug ( M_SB,"getInode:: calling creating Inode\n" );
  inode = new MinixFSInode ( this,
                             ibuffer->get2Bytes ( 0 ),
                             ibuffer->get2Bytes ( 2 ),
                             ibuffer->get4Bytes ( 4 ),
                             ibuffer->get4Bytes ( 8 ),
                             ibuffer->getByte ( 12 ),
                             ibuffer->getByte ( 13 ),
                             i_zones,
                             i_num
                           );
  //debug ( M_SB,"getInode:: returned creating Inode\n" );
  delete[] i_zones;
  delete ibuffer;
  return inode;
}


MinixFSSuperblock::~MinixFSSuperblock()
{
  //debug ( M_SB,"~MinixSuperblock\n" );
  assert ( dirty_inodes_.empty() == true );
  storage_manager_->flush ( this );
  uint32 num = s_files_.getLength();
  for ( uint32 counter = 0; counter < num; counter++ )
  {
    FileDescriptor *fd = s_files_.at ( 0 );
    File* file = fd->getFile();
    s_files_.remove ( fd );
/////////////////////////////////
    //global_fd.remove(fd);??????
/////////////////////////////////

    if ( file )
    {
      delete file;
    }
    delete fd;
  }

  assert ( s_files_.empty() == true );

  num = all_inodes_.getLength();

  //debug ( M_SB,"~MinixSuperblock num: %d inodes to delete\n",num );
  for ( uint32 counter = 0; counter < num; counter++ )
  {
    Inode* inode = all_inodes_.at ( 0 );

    //debug ( M_SB,"~MinixSuperblock writing inode to disc\n" );
    writeInode ( inode );

    //debug ( M_SB,"~MinixSuperblock inode written to disc\n" );
    all_inodes_.remove ( inode );
    Dentry* dentry = inode->getDentry();

    //debug ( M_SB,"~MinixSuperblock deleteing denty->getName() : %s\n",dentry->getName() );
    delete dentry;

    //debug ( M_SB,"~MinixSuperblock deleting inode\n" );
    //std::cout << "deleteing inode " << inode;
    delete inode;
  }

  delete storage_manager_;

  assert ( all_inodes_.empty() == true );

  //debug ( M_SB,"~MinixSuperblock finished\n" );
}


Inode* MinixFSSuperblock::createInode ( Dentry* dentry, uint32 type )
{
  uint16 mode = 0x01ff;
  if ( type == I_FILE )
    mode |= 0x8000;
  else if ( type == I_DIR )
    mode |= 0x4000;
  //else link etc.
  uint16 *zones = new uint16[9];
  for ( uint32 i = 0; i < 9; i++ )
    zones[i] = 0;
  uint32 i_num = storage_manager_->acquireInode();
  //debug ( M_SB,"createInode> acquired inode %d mode: %d\n", i_num,mode );
  Inode *inode = ( Inode* ) ( new MinixFSInode ( this, mode, 0, 0, 0, 0, 0, zones, i_num ) );
  //debug ( M_SB,"createInode> created Inode\n" );
  delete[] zones;
  all_inodes_.pushBack ( inode );
  //debug ( M_SB,"createInode> calling write Inode to Disc\n" );
  writeInode ( inode );
  //debug ( M_SB,"createInode> returned from write Inode to Disc\n" );
  if ( type == I_DIR )
  {
    //debug ( M_SB,"createInode> mkdir\n" );
    assert ( inode->mkdir ( dentry ) == 0 );
  }
  else if ( type == I_FILE )
  {
    //debug ( M_SB,"createInode> mkfile\n" );
    assert ( inode->mkfile ( dentry ) == 0 );
  }
  //debug ( M_SB,"createInode> finished\n" );
  return inode;
}


int32 MinixFSSuperblock::readInode ( Inode* inode )
{
  assert ( inode );
  MinixFSInode *minix_inode = ( MinixFSInode * ) inode;
  assert ( all_inodes_.included ( inode ) );
  uint32 block = 2 + s_num_inode_bm_blocks_ + s_num_zone_bm_blocks_ + ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE / BLOCK_SIZE );
  uint32 offset = ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE ) % BLOCK_SIZE;
  Buffer *buffer = new Buffer ( INODE_SIZE );
  readBytes ( block, offset, INODE_SIZE, buffer->getBuffer() );
  uint16 *i_zones = new uint16[9];
  for ( uint32 num_zone = 0; num_zone < 9; num_zone ++ )
  {
    i_zones[num_zone] = buffer->get2Bytes ( 14 + ( num_zone * 2 ) );
  }
  MinixFSZone *to_delete_i_zones = minix_inode->i_zones_;
  minix_inode->i_zones_ = new MinixFSZone ( this, i_zones );
  minix_inode->i_nlink_ = buffer->getByte ( 13 );
  minix_inode->i_size_ = buffer->get4Bytes ( 4 );
  delete to_delete_i_zones;
  delete buffer;
  return 0;
}


void MinixFSSuperblock::writeInode ( Inode* inode )
{
  //std::cout << "writing inode " << inode;
  assert ( inode );
  assert ( all_inodes_.included ( inode ) );
  //flush zones
  MinixFSInode *minix_inode = ( MinixFSInode * ) inode;
  uint32 block = 2 + s_num_inode_bm_blocks_ + s_num_zone_bm_blocks_ + ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE / BLOCK_SIZE );
  uint32 offset = ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE ) % BLOCK_SIZE;
  Buffer *buffer = new Buffer ( INODE_SIZE );
  buffer->clear();
  //debug ( M_SB,"writeInode> reading block %d with offset %d from disc\n", block, offset );
  readBytes ( block, offset, INODE_SIZE, buffer->getBuffer() );
  //debug ( M_SB,"writeInode> read data from disc\n" );
  //debug ( M_SB,"writeInode> the inode: i_type_: %d, i_nlink_: %d, i_size_: %d\n",minix_inode->i_type_,minix_inode->i_nlink_,minix_inode->i_size_ );
  if ( minix_inode->i_type_ == I_FILE )
  {
    //debug ( M_SB,"writeInode> setting mode to file : %x\n",buffer->get2Bytes ( 0 ) | 0x8000 );
    buffer->set2Bytes ( 0, buffer->get2Bytes ( 0 ) | 0x81FF );
    //buffer->set2Bytes ( 0, 0x81B6 );
  }
  else if ( minix_inode->i_type_ == I_DIR )
  {
    //debug ( M_SB,"writeInode> setting mode to dir : %x\n",buffer->get2Bytes ( 0 ) | 0x4000 );
    buffer->set2Bytes ( 0, buffer->get2Bytes ( 0 ) | 0x41FF );
    //buffer->set2Bytes ( 0, 0x41FF );
  }
  else
  {
    //TODO; link etc.
  }
///////////
  buffer->set4Bytes ( 8, 0 );
  buffer->setByte ( 12, 0 );
  buffer->set2Bytes ( 2, 0 );
///////////
  buffer->setByte ( 13, minix_inode->i_nlink_ );

  //std::cout << minix_inode->i_num_ << std::endl;

  buffer->set4Bytes ( 4, minix_inode->i_size_);

  //std::cout << "in SB::writeInode: " << minix_inode << " size = " << inode->getSize() << " vs. " << buffer->get4Bytes(4) << std::endl;
  //debug ( M_SB,"writeInode> writing bytes to disc on block %d with offset %d\n",block,offset );
  //buffer->print();
  writeBytes ( block, offset, INODE_SIZE, buffer->getBuffer() );
  //debug ( M_SB,"writeInode> flushing zones\n" );
  minix_inode->i_zones_->flush ( minix_inode->i_num_ );
  delete buffer;
}


void MinixFSSuperblock::delete_inode ( Inode* inode )
{
  assert ( !used_inodes_.included ( inode ) );
  if ( dirty_inodes_.included ( inode ) )
  {
    dirty_inodes_.remove ( inode );
  }
  assert ( all_inodes_.remove ( inode ) == 0 );
  all_inodes_.remove ( inode );
  MinixFSInode *minix_inode = ( MinixFSInode * ) inode;
  assert ( minix_inode->i_files_.empty() );
  /*for ( uint32 index = 0; index < minix_inode->i_zones_->getNumZones(); index++ )
  {
    storage_manager_->freeZone ( minix_inode->i_zones_->getZone ( index ) );
  }*/
  minix_inode->i_zones_->freeZones();
  storage_manager_->freeInode ( minix_inode->i_num_ );
  uint32 block = 2 + s_num_inode_bm_blocks_ + s_num_zone_bm_blocks_ + ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE / BLOCK_SIZE );
  uint32 offset = ( ( minix_inode->i_num_ - 1 ) * INODE_SIZE ) % BLOCK_SIZE;
  Buffer *buffer = new Buffer ( INODE_SIZE );
  buffer->clear();
  writeBytes ( block, offset, INODE_SIZE, buffer->getBuffer() );
  delete buffer;
  delete inode;
}


int32 MinixFSSuperblock::createFd ( Inode* inode, uint32 flag )
{
  assert ( inode );

  File* file = inode->link ( flag );
  FileDescriptor* fd = new FileDescriptor ( file );
  s_files_.pushBack ( fd );
  global_fd.push_back ( fd );

  if ( !used_inodes_.included ( inode ) )
  {
    used_inodes_.pushBack ( inode );
  }

  return ( fd->getFd() );
}


int32 MinixFSSuperblock::removeFd ( Inode* inode, FileDescriptor* fd )
{
  assert ( inode );
  assert ( fd );

  s_files_.remove ( fd );
  global_fd.remove(fd);

  File* file = fd->getFile();
  int32 tmp = inode->unlink ( file );

  //debug ( M_SB,"remove the fd num: %d\n", fd->getFd() );
  if ( inode->getNumOpenedFile() == 0 )
  {
    used_inodes_.remove ( inode );
  }
  delete fd;

  return tmp;
}


uint16 MinixFSSuperblock::allocateZone()
{
  debug ( M_SB,"MinixFSSuperblock allocateZone>\n" );
//   storage_manager_->printBitmap();
  uint16 ret = ( storage_manager_->acquireZone() + s_1st_datazone_ - 1 ); // -1 because the zone nr 0 is set in the bitmap and should never be used!
//   storage_manager_->printBitmap();
  debug ( M_SB,"MinixFSSuperblock allocateZone> returning %d\n",ret );
  return ret;
}

void MinixFSSuperblock::readZone ( uint16 zone, char* buffer )
{
  readBlocks ( zone, ZONE_SIZE/BLOCK_SIZE, buffer );
}

void MinixFSSuperblock::readBlocks(uint16 block, uint32 num_blocks, char* buffer)
{
  lseek(s_dev_, offset_ + block * BLOCK_SIZE, SEEK_SET);
  read(s_dev_, buffer, BLOCK_SIZE * num_blocks);
}

void MinixFSSuperblock::writeZone(uint16 zone, char* buffer)
{
  writeBlocks(zone, ZONE_SIZE / BLOCK_SIZE, buffer);
}

void MinixFSSuperblock::writeBlocks(uint16 block, uint32 num_blocks, char* buffer)
{
  lseek(s_dev_, offset_ + block * BLOCK_SIZE, SEEK_SET);
  write(s_dev_, buffer, BLOCK_SIZE * num_blocks);
}


int32 MinixFSSuperblock::readBytes(uint32 block, uint32 offset, uint32 size, char* buffer)
{
  assert(offset+size <= BLOCK_SIZE);
  Buffer* rbuffer = new Buffer( BLOCK_SIZE);
  readBlocks(block, 1, rbuffer->getBuffer());
  rbuffer->setOffset(offset);
  for (uint32 index = 0; index < size; index++)
  {
    buffer[index] = rbuffer->getByte(index);
  }
  delete rbuffer;
  return size;
}

int32 MinixFSSuperblock::writeBytes(uint32 block, uint32 offset, uint32 size, char* buffer)
{
  assert(offset+size <= BLOCK_SIZE);
  Buffer* wbuffer = new Buffer( BLOCK_SIZE);
  readBlocks(block, 1, wbuffer->getBuffer());
  wbuffer->setOffset(offset);
  for (uint32 index = 0; index < size; index++)
  {
    wbuffer->setByte(index, buffer[index]);
  }
  wbuffer->setOffset(0);
  writeBlocks(block, 1, wbuffer->getBuffer());
  delete wbuffer;
  return size;
}

void MinixFSSuperblock::freeZone(uint16 index)
{
  storage_manager_->freeZone ( index - s_1st_datazone_ + 1 );
}

