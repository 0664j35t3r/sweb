/**
 * @file FileSystemInfo.cpp
 */

#include "FileSystemInfo.h"
#include "Dentry.h"
#include <assert.h>
#include "string.h"

FileSystemInfo *fs_info = 0;

FileSystemInfo::FileSystemInfo() :
    root_(0), pwd_(0), pathname_(0)
{
}

FileSystemInfo::~FileSystemInfo()
{
}

int32 FileSystemInfo::setName(const char* pathname, uint32 length)
{
  if (pathname == 0)
    return -1;

  uint32 path_len = strlen(pathname);
  assert(length < path_len);
  if (length == 0)
    path_len += 1;
  else
    path_len = length + 1;

  pathname_ = new char[path_len];
  strncpy(pathname_, pathname, path_len);
  pathname_[path_len - 1] = 0;

  return 0;
}

void FileSystemInfo::putName()
{
  delete[] pathname_;
  pathname_ = 0;
}

