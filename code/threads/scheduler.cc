// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

static int 
PriorityCompare(Thread* a, Thread* b)
{
	if(a->getPriority() > b->getPriority()) return -1;
	if(a->getPriority() < b->getPriority()) return 1;
	if(a->getID() < b->getID()) return -1;
	if(a->getID() > b->getID()) return 1;
	return 0;
}
	
static int 
BurstTimeCompare(Thread* a, Thread* b)
{
	if(a->getBurstTime() < b->getBurstTime()) return -1;
	if(a->getBurstTime() > b->getBurstTime()) return 1;
	if(a->getID() < b->getID()) return -1;
	if(a->getID() > b->getID()) return 1;
	return 0;
}
//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
  L1Queue = new SortedList<Thread *>(BurstTimeCompare);
	L2Queue = new SortedList<Thread *>(PriorityCompare);
  L3Queue = new List<Thread *>; 
  toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
  delete L1Queue;
	delete L2Queue;
  delete L3Queue;
}  

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
  ASSERT(kernel->interrupt->getLevel() == IntOff);
  DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
  thread->setStatus(READY);
  thread->setWaitTime(0);
	Statistics *stats = kernel->stats;
	Thread *current = kernel->currentThread;
	Interrupt *interrupt = kernel->interrupt;
 
  if(thread-> getPriority() >= 100)
	{
    if(thread->getID() != -1) //ignore postal worker
		{
      cout << "Tick [" << stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[1]"<< endl;
		}
    L1Queue->Insert(thread);
	}
	else if(thread->getPriority() >= 50)
	{
    if(thread->getID() != -1) //ignore postal worker
		{
		  cout << "Tick [" << stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[2]"<< endl;
		}
    L2Queue->Insert(thread);
	}
	else
	{
    if(thread->getID() != -1) //ignore postal worker
		{
		  cout << "Tick [" << stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[3]"<< endl;
		}
    L3Queue->Append(thread);
	}
  
  
  if(current->getStatus()==RUNNING && current->getID() != 0) //if the current is not main, then do rescheduling
  {
    interrupt->YieldOnReturn();
  }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
  ASSERT(kernel->interrupt->getLevel() == IntOff);
	Thread *thread = kernel ->currentThread;
	Statistics *stats  = kernel ->stats;
	if(!L1Queue->IsEmpty())
	{
    if(L1Queue->Front()->getID() != -1) //ignore postal worker
		{
      cout << "Tick [" << stats->totalTicks << "]: Thread [" << L1Queue->Front()->getID() << "] is removed from queue L[1]"<< endl;
		}
    return L1Queue->RemoveFront();
	}
	else if(!L2Queue ->IsEmpty())
	{
    if(L2Queue->Front()->getID() != -1) //ignore postal worker
		{
		  cout << "Tick [" << stats->totalTicks << "]: Thread [" << L2Queue->Front()->getID() << "] is removed from queue L[2]"<< endl;
		}
    return L2Queue->RemoveFront();
	}
	else if(!L3Queue ->IsEmpty())
	{
    if(L3Queue->Front()->getID() != -1) //ignore postal worker
		{
		  cout << "Tick [" << stats->totalTicks << "]: Thread [" << L3Queue->Front()->getID() << "] is removed from queue L[3]"<< endl;
		}
    return L3Queue->RemoveFront();
	}
	else
  {
		//cout<<"return Null"<<endl;
		return NULL;
	}
	
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}
void 
Scheduler::AgingMechanism()
{
	ListIterator<Thread *> *iterator1 = new ListIterator<Thread *>(L1Queue);
	ListIterator<Thread *> *iterator2 = new ListIterator<Thread *>(L2Queue);
	ListIterator<Thread *> *iterator3 = new ListIterator<Thread *>(L3Queue);
	
	Statistics *stats = kernel->stats;

	for(; !iterator1->IsDone(); iterator1->Next())
	{
		iterator1->Item()->setWaitTime(iterator1->Item()->getWaitTime() + 1);
    
		if(iterator1->Item()->getWaitTime() >= WaitForIncreasePriority) // WaitForIncreasePriority = 1500
		{
      int ID = iterator1->Item()->getID();
			int prev = iterator1->Item()->getPriority();
      
			iterator1->Item()->setPriority(prev + IncreasePriority); // IncreasePriority  = 10
      if(ID != -1) //ignore postal worker
      {
			  cout << "Tick [" << stats->totalTicks << "]: Thread [" << ID << "] changes its priority from [" << prev << "] to [" << prev + IncreasePriority << "]" << endl;
		  }
      iterator1->Item()->setWaitTime(0);
    }
	}
	
	for(; !iterator2->IsDone(); iterator2->Next())
	{
		iterator2->Item()->setWaitTime(iterator2->Item()->getWaitTime() + 1);
    
		if(iterator2->Item()->getWaitTime() >= WaitForIncreasePriority) // WaitForIncreasePriority = 1500
		{
      int ID = iterator2->Item()->getID();
			int prev = iterator2->Item()->getPriority();
      
			iterator2->Item()->setPriority(prev + IncreasePriority); // IncreasePriority  = 10
			if(ID != -1) //ignore postal worker
      {
        cout << "Tick [" << stats->totalTicks << "]: Thread [" << ID << "] changes its priority from [" << prev << "] to [" << prev + IncreasePriority << "]" << endl;
			}
      iterator2->Item()->setWaitTime(0);
      
      if(prev + IncreasePriority >= 100)
      {
        L2Queue->Remove(iterator2->Item());
        if(ID != -1) //ignore postal worker
  			{
          cout << "Tick [" << stats->totalTicks << "]: Thread [" << ID << "] is removed from queue L[2]"<< endl;
  			}
        ReadyToRun(iterator2->Item());
      }
		}
	}
	for(; !iterator3 ->IsDone(); iterator3 ->Next())
	{
		iterator3->Item()->setWaitTime(iterator3->Item()->getWaitTime() + 1);
   
		if(iterator3->Item()->getWaitTime() >= WaitForIncreasePriority) // WaitForIncreasePriority = 1500
		{
      int ID = iterator3->Item()->getID();
			int prev = iterator3->Item()->getPriority();
      
			iterator3->Item()->setPriority(prev + IncreasePriority); // IncreasePriority  = 10
      if(ID != -1) //ignore postal worker
			{
        cout << "Tick [" << stats->totalTicks << "]: Thread [" << ID << "] changes its priority from [" << prev << "] to [" << prev + IncreasePriority << "]" << endl;
			}
      iterator3->Item()->setWaitTime(0);
      
      if(prev + IncreasePriority >= 50)
      {
        L3Queue->Remove(iterator3->Item());
        if(ID != -1) //ignore postal worker
        {
  			  cout << "Tick [" << stats->totalTicks << "]: Thread [" << ID << "] is removed from queue L[3]"<< endl;
  			}
        ReadyToRun(iterator3->Item());
      }
		}
	}
}