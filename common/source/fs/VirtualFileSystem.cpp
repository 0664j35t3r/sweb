/**
 * @file VirtualFileSystem.cpp
 */

#include "fs/FileSystemType.h"
#include "fs/FileSystemInfo.h"
#include "fs/PathWalker.h"
#include "fs/VirtualFileSystem.h"
#include "fs/Dentry.h"
#include "fs/Superblock.h"
#include "fs/VfsMount.h"
#include "util/string.h"
#include "assert.h"
#include "mm/kmalloc.h"
#include "BDManager.h"
#include "BDVirtualDevice.h"
#include "kernel/Thread.h"

#include "console/kprintf.h"

/**
 * Global VirtualFileSystem object
 */
VirtualFileSystem vfs;

void VirtualFileSystem::initialize()
{
    new(this) VirtualFileSystem();
}


VirtualFileSystem::VirtualFileSystem()
{}


VirtualFileSystem::~VirtualFileSystem()
{}


int32 VirtualFileSystem::registerFileSystem ( FileSystemType *file_system_type )
{
  assert ( file_system_type );
  assert ( file_system_type->getFSName() );

  // check whether a file system type with that name has already been
  // registered
  if (getFsType(file_system_type->getFSName()))
      return -1;
  file_system_types_.push_back ( file_system_type );
  return 0;
}


int32 VirtualFileSystem::unregisterFileSystem ( FileSystemType *file_system_type )
{
  assert ( file_system_type != 0 );

  const char *fs_name = file_system_type-> getFSName();
  uint32 fstl_size = file_system_types_.size();

  for ( uint32 counter = 0; counter < fstl_size; ++counter )
  {
    if ( strcmp ( file_system_types_.at ( counter )->getFSName(), fs_name ) == 0 )
    {
      FileSystemType *fst = file_system_types_.at ( counter );
      delete fst;
    }
  }

  return 0;
}


FileSystemType *VirtualFileSystem::getFsType ( const char* fs_name )
{
  assert ( fs_name );

  uint32 fstl_size = file_system_types_.size();

  for ( uint32 counter = 0; counter < fstl_size; ++counter )
  {
    if ( strcmp ( file_system_types_.at ( counter )->getFSName(), fs_name ) == 0 )
    {
      return file_system_types_.at ( counter );
    }
  }

  return 0;

}


VfsMount *VirtualFileSystem::getVfsMount ( const Dentry* dentry, bool is_root )
{
  assert ( dentry );

  uint32 vfs_mount_size = mounts_.size();
  debug ( VFS, "getVfsMount> vfs_mount_size : %d\n",vfs_mount_size );

  if ( is_root == false )
  {
    for ( uint32 counter = 0; counter < vfs_mount_size; ++counter )
    {
      debug ( VFS,"getVfsMount> mounts_.at(counter)->getMountPoint()->getName() : %s\n",mounts_.at ( counter )->getMountPoint()->getName() );
      if ( ( mounts_.at ( counter )->getMountPoint() ) == dentry )
      {
        return mounts_.at ( counter );
      }
    }
  }
  else
  {
    for ( uint32 counter = 0; counter < vfs_mount_size; ++counter )
    {
      if ( ( mounts_.at ( counter )->getRoot() ) == dentry )
      {
        return mounts_.at ( counter );
      }
    }
  }

  return 0;

}


FileSystemInfo *VirtualFileSystem::root_mount ( const char *fs_name, uint32 /*flags*/ )
{
  FileSystemType *fst = getFsType ( fs_name );

  Superblock *super = fst->createSuper ( 0,-1 );
  super = fst->readSuper ( super, 0 );
  Dentry *mount_point = super->getMountPoint();
  mount_point->setMountPoint ( mount_point );
  Dentry *root = super->getRoot();

  VfsMount *root_mount = new VfsMount ( 0, mount_point, root, super, 0 );

  mounts_.push_back ( root_mount );
  superblocks_.push_back ( super );

  // fs_info initialize
  FileSystemInfo *fs_info = new FileSystemInfo();
  fs_info->setFsRoot ( root, root_mount );
  fs_info->setFsPwd ( root, root_mount );

  return fs_info;
}


int32 VirtualFileSystem::mount ( const char* dev_name, const char* dir_name,
                                 const char* fs_name, uint32 /*flags*/ )
{
  FileSystemInfo *fs_info = currentThread->getWorkingDirInfo();
  if ( !dev_name )
    return -1;
  if ( ( !dir_name ) || ( !fs_name ) )
    return -1;


  BDVirtualDevice* bddev = BDManager::getInstance()->getDeviceByName(dev_name);
  uint32 dev = bddev ? bddev->getDeviceNumber() : (uint32) -1;
  if (!bddev && strcmp(dev_name, "") != 0)
  {
    debug(VFS, "mount: device with name %s doesn't exist\n", dev_name);
    return -1;
  }

  debug ( VFS, "dev_nr:%d\n", dev );
  FileSystemType *fst = getFsType ( fs_name );
  if (!fst)
    return -1;

  fs_info->setName ( dir_name );
  const char* test_name = fs_info->getName();
  Dentry* pw_dentry = 0;
  VfsMount* pw_vfs_mount = 0;
  int32 success = PathWalker::pathWalk ( test_name , 0, pw_dentry, pw_vfs_mount);
  fs_info->putName();

  if ( success != 0 )
    return -1;

  // found the mount point
  Dentry *found_dentry = pw_dentry;
  VfsMount *found_vfs_mount = pw_vfs_mount;

  // create a new superblock
  Superblock *super = fst->createSuper ( found_dentry, dev );
  if (!super)
    return -1;
  super = fst->readSuper ( super, 0 ); //?
  Dentry *root = super->getRoot();

  // create a new vfs_mount
  VfsMount *std_mount = new VfsMount ( found_vfs_mount, found_dentry,
                                       root, super, 0 );
  mounts_.push_back ( std_mount );
  superblocks_.push_back ( super );
  return 0;
}


int32 VirtualFileSystem::rootUmount()
{
  if ( superblocks_.size() == 0 )
  {
    return -1;
  }
  VfsMount *root_vfs_mount = mounts_.at ( 0 );
  delete root_vfs_mount;

  Superblock *root_sb = superblocks_.at ( 0 );
  delete root_sb;
  return 0;
}


int32 VirtualFileSystem::umount ( const char* dir_name, uint32 /*flags*/ )
{
  FileSystemInfo *fs_info = currentThread->getWorkingDirInfo();
  if ( dir_name == 0 )
    return -1;

  fs_info->setName ( dir_name );
  const char* test_name = fs_info->getName();
  Dentry* pw_dentry = 0;
  VfsMount* pw_vfs_mount = 0;
  int32 success = PathWalker::pathWalk ( test_name, 0, pw_dentry, pw_vfs_mount);

  fs_info->putName();

  if ( success != 0 )
    return -1;

  // test the umount point\n
  Dentry *found_dentry = pw_dentry;
  VfsMount * found_vfs_mount = pw_vfs_mount;

  if ( found_vfs_mount == 0 )
  {
    debug ( VFS,"umount point error\n" );
    return -1;
  }
  else
    debug ( VFS,"umount point found\n" );

  // in the case, the current-directory is in the local-root of the umounted
  // filesystem
  if ( fs_info->getPwdMnt() == found_vfs_mount )
  {
    if ( fs_info->getPwd() == found_dentry )
    {
      debug ( VFS,"the mount point exchange\n" );
      fs_info->setFsPwd ( found_vfs_mount->getMountPoint(),
                          found_vfs_mount->getParent() );
    }
    else
    {
      debug ( VFS,"set PWD NULL\n" );
      fs_info->setFsPwd ( 0,0 );
    }
  }

  Superblock *sb = found_vfs_mount->getSuperblock();

  mounts_.remove(found_vfs_mount);
  delete found_vfs_mount;
  delete sb;

  return 0;
}


