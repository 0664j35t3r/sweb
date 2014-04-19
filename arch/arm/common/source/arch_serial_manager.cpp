/**
 * @file arch_serial_manager.cpp
 *
 */

#include "arch_serial.h"
#include "serial.h"
#include "string.h"

#include "debug_bochs.h"
#include "kprintf.h"

SerialManager * SerialManager::instance_ = 0;

SerialManager::SerialManager() : num_ports( 0 )
{
  assert(false);
};

SerialManager::~SerialManager()
{
  assert(false);
};

uint32 SerialManager::get_num_ports()
{
  assert(false);
  return num_ports;
};

uint32 SerialManager::do_detection( uint32 is_paging_set_up )
{
  assert(false);
  return num_ports;
}

void SerialManager::service_irq( uint32 irq_num )
{
  assert(false);
}
