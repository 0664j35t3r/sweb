/**
 * @file Mutex.cpp
 */

#include "Mutex.h"
#include "console/kprintf.h"
#include "ArchThreads.h"
#include "ArchInterrupts.h"
#include "debug_bochs.h"
#include "Scheduler.h"
#include "Thread.h"
#include "panic.h"

Mutex::Mutex() : 
  mutex_(0),
  sleepers_(),
  held_by_(0),
  spinlock_()
{
}

bool Mutex::acquireNonBlocking()
{
  if(!spinlock_.acquireNonBlocking())
      return false;

  while ( ArchThreads::testSetLock ( mutex_,1 ) )
  {
    if(threadOnList(currentThread))
    {
      kprintfd ( "Mutex::acquire: thread %s going to sleep is already on sleepers-list\n"
                 "you shouldn't use Scheduler::wake() with a thread sleeping on a mutex\n", currentThread->getName() );
      assert(false);
    }

    spinlock_.release();
    return false;
  }
  spinlock_.release();
  held_by_=currentThread;
  return true;
}

void Mutex::acquire()
{
  spinlock_.acquire();
  while ( ArchThreads::testSetLock ( mutex_,1 ) )
  {
    if(threadOnList(currentThread))
    {
      kprintfd ( "Mutex::acquire: thread %s going to sleep is already on sleepers-list\n"
                 "you shouldn't use Scheduler::wake() with a thread sleeping on a mutex\n", currentThread->getName() );
      assert(false);
    }

    sleepers_.push_back ( currentThread );
    Scheduler::instance()->sleepAndRelease ( spinlock_ );
    spinlock_.acquire();
  }
  spinlock_.release();
  held_by_=currentThread;
}

void Mutex::release()
{
  spinlock_.acquire();
  mutex_ = 0;
  held_by_=0;
  if ( ! sleepers_.empty() )
  {
    Thread *thread = sleepers_.front();
    sleepers_.pop_front();
    Scheduler::instance()->wake ( thread );
  }
  spinlock_.release();
}

bool Mutex::isFree()
{
  if ( unlikely ( ArchInterrupts::testIFSet() && Scheduler::instance()->isSchedulingEnabled() ) )
    kpanict ( ( uint8* ) ( "Mutex::isFree: ERROR: Should not be used with IF=1 AND enabled Scheduler, use acquire instead\n" ) );

  if ( !spinlock_.isFree() || mutex_ > 0 )
    return false;
  else
    return true;
}

bool Mutex::threadOnList(Thread *thread)
{
  bool return_value = false;
  for (uint32 i=0; i < sleepers_.size(); i++  )
  {
    if(thread == sleepers_.front())
      return_value = true;
    sleepers_.push_back(sleepers_.front());
    sleepers_.pop_front();
  }
  return return_value;
}
