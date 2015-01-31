#ifndef CHAR_DEV_H__
#define CHAR_DEV_H__

#include "kstring.h"
#include "FiFo.h"

/**
 * @class CharacterDevice Links the character devices to the Device File System.
 */
class CharacterDevice
{
  public:

    /**
     * Constructor
     * @param name the device name
     * @param super_block the superblock (0)
     * @param inode_type the inode type (cahracter device)
     */
    CharacterDevice(const char* name) :
        _in_buffer(CD_BUFFER_SIZE, FIFO_NOBLOCK_PUT | FIFO_NOBLOCK_PUT_OVERWRITE_OLD),
        _out_buffer(CD_BUFFER_SIZE, FIFO_NOBLOCK_PUT | FIFO_NOBLOCK_PUT_OVERWRITE_OLD)
    {
      uint32 name_len = strlen(name) + 1;
      device_name = new char[name_len];
      strncpy(device_name, name, name_len);
      device_name[name_len - 1] = 0;
    }

    ~CharacterDevice()
    {
    }

    /**
     * reads the data from the character device
     * @param buffer is the buffer where the data is written to
     * @param count is the number of bytes to read.
     * @param offset is never to be used, because there is no offset
     *        in character devices, but it is defined in the Inode interface
     */
    virtual int32 readData(uint32 offset, uint32 size, char *buffer)
    {
      if (offset)
        return -1; // offset reading not supprted with char devices

      char *bptr = buffer;
      do
      {
        *bptr++ = _in_buffer.get();
      } while ((bptr - buffer) < (int32) size);

      return (bptr - buffer);
    }

    /**
     * writes to the character device
     * @param buffer is the buffer where the data is read from
     * @param count is the number of bytes to write.
     * @param offset is never to be used, because there is no offset
     *        in character devices, but it is defined in the Inode interface
     */
    virtual int32 writeData(uint32 offset, uint32 size, const char*buffer)
    {
      if (offset)
        return -1; // offset writing also not supp0rted

      const char *bptr = buffer;
      do
      {
        _out_buffer.put(*bptr++);
      } while ((bptr - buffer) < (int32) size);

      return (bptr - buffer);
    }

    char *getDeviceName() const
    {
      return device_name;
    }

  protected:
    static const uint32 CD_BUFFER_SIZE = 1024;

    FiFo<uint8> _in_buffer;
    FiFo<uint8> _out_buffer;

    char *device_name;

    void processInBuffer()
    {
      _in_buffer.get();
    }

    void processOutBuffer()
    {
      _out_buffer.get();
    }

};

#endif

