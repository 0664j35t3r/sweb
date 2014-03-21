/**
 * @file SpinLock.h
 */

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "types.h"
#include <ustl/ulist.h>

class Thread;


/**
 * @class SpinLock
 *
 * WikiPedia about SpinLocks:
 * @url http://en.wikipedia.org/wiki/Spinlock
 * In software engineering, a spinlock is a lock where the thread simply waits in a loop ("spins")
 * repeatedly checking until the lock becomes available. This is also known as "busy waiting"
 * because the thread remains active but isn't performing a useful task.
 *
 * In Sweb we use SpinLocks in situations where we cannot allocate any kernel memory
 * i.e. because we are locking the KMM itself.
 * The SpinLock however is not meant to be used in a context where the InterruptFlag is not set,
 * because as with any lock, no taskswitch can happen and deadlock would occur if acquiring the SpinLock should
 * not succeed in an IF==0 context.
 * Also, in sweb, the Spinlock uses yield()s instead of a simple do-nothing-loop
 */
class SpinLock
{
  public:

    /**
     * Constructor
     * @return SpinLock instance
     */
    SpinLock(const char* name);

    /**
     * acquires the SpinLock.
     */
    void acquire(const char* debug_info = 0);

    /**
     * acquires the SpinLock non blocking.
     */
    bool acquireNonBlocking(const char* debug_info = 0);

    /**
     * releases the SpinLock.
     */
    void release(const char* debug_info = 0);

    /**
     * allows you to check if the SpinLock is set or not.
     * trust the return value only if the SpinLock can't be acquired or releases
     * when you're not locking. (= only use in atomic state)
     */
    bool isFree();

    /**
     * Returns the thread holding the lock
     *
     * @return the thread holding the lock
     */
    Thread* heldBy()
    {
      return held_by_;
    }

    const char* name_;

  private:

    /**
      * @note dont't use any stuff that needs memory allocation here
      * since the spinlock is used by the KernelMemoryManager
      */
    size_t nosleep_mutex_;
    Thread *held_by_;

    /**
      * Copy Constructor - you _really_ don't want to copy a SpinLock!
      */
    SpinLock ( SpinLock const & );

    /**
      * operator = - you _really_ don't want to copy a SpinLock!
      */
    SpinLock &operator= ( SpinLock const& );

    /**
     * verifies that interrupts are enabled
     * @param method in which the check is done
     * @param debug_info additional debug info
     */
    void checkInterrupts(const char* method, const char* debug_info);

};

#endif
