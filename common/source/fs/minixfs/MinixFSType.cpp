/**
 * @file MinixFSType.cpp
 */

#include "fs/minixfs/MinixFSType.h"
#include "fs/minixfs/MinixFSSuperblock.h"

MinixFSType::MinixFSType() : FileSystemType("minixfs")
{
}


MinixFSType::~MinixFSType()
{
}


Superblock *MinixFSType::readSuper(Superblock *superblock, void*) const
{
  return superblock;
}


Superblock *MinixFSType::createSuper(Dentry *root, uint32 s_dev) const
{
  Superblock *super = new MinixFSSuperblock(root, s_dev);
  return super;
}
