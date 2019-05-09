
// scheduler.cc
//      Routines to choose the next thread to run, and to dispatch to
//      that thread.
//
//      These routines assume that interrupts are already disabled.
//      If interrupts are disabled, we can assume mutual exclusion
//      (since we are on a uniprocessor).
//
//      NOTE: We can't use Locks to provide mutual exclusion here, since
//      if we needed to wait for a lock, and the lock was busy, we would
//      end up calling FindNextToRun(), and that would put us in an
//      infinite loop.
//
//      Very simple implementation -- no priorities, straight FIFO.
//      Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "scheduler.h"
#include "copyright.h"
#include "system.h"
#include <stdio.h>

//----------------------------------------------------------------------
// Scheduler::Scheduler
//      Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler() {
  readyList = new List<Thread *>();
  suspendedList = new List<Thread *>();
  MultiLevelList = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
//      De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler() {
  delete readyList;
  delete suspendedList;
  delete ptimer;
  delete[] MultiLevelList;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
//      Mark a thread as ready, but not running.
//      Put it on the ready list, for later scheduling onto the CPU.
//
//      "thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread) {
  DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

  thread->setStatus(READY);
  switch (policy) {

  case SCHED_FCFS: {
    readyList->Append(thread);
    break;
  }

  case SCHED_RR: {
    // YOUR PROJECT2 CODE HERE
    // Similar to FCFS, when a thread is ready is just
    // appended to the end of the ready queue
        thread->setQuantum(4);
        readyList->Append(thread);
        break;

  }

  case SCHED_PRIO_P: {
    // YOUR PROJECT2 CODE HERE
    // MAX_PRIOTIRY is defined as 20, thus by substracting to 20
    // the priority of the thread, we are able to insert the thread
    // by defining the sorted key as thread->MAX_PRIORITY - thread->getPriority()
    readyList->SortedInsert(thread,thread->MAX_PRIORITY - thread->getPriority());
    break;

  }

  case SCHED_MLFQ: {
    // YOUR PROJECT2 CODE HERE
    // if the quantum of the running thread is used up or the thread is just arrived, then
    // appen the thread to its respective level list based on its priority
    // else, the thread is preempte
        if(thread->getQuantum() <= 0){
                if(thread->getPriority() == 2){
                        //if priority = level = 2, then set q = 4
                        thread->setQuantum(4);
                        MultiLevelList[thread->getPriority()].Append(thread);

                }
                else if(thread->getPriority()==1){
                        //if priority = level = 1, then set q = 8
                        thread->setQuantum(8);
                        MultiLevelList[thread->getPriority()].Append(thread);

                }
                else{//if priority == 0 then just do FCFS
                        thread->setQuantum(thread->getBurstTime());
                        MultiLevelList[thread->getPriority()].Append(thread);

                }
        }else{
                MultiLevelList[thread->getPriority()].Prepend(thread);
        }
        break;
  }

  default:
    readyList->Append(thread);
    break;
  }
}

//----------------------------------------------------------------------
// CHANGED
// Scheduler::FindNextToRun
//----------------------------------------------------------------------

Thread *Scheduler::FindNextToRun() {
  Thread *next_to_run;
  switch (policy) {

  case SCHED_FCFS: {
    next_to_run = readyList->Remove();
    break;
  }

  case SCHED_RR: {
    // YOUR PROJECT2 CODE HERE
    // Similar, to FCFS, we fetch the first thread
    // in the ready queue to run
        next_to_run = readyList->Remove();
        break;
  }

  case SCHED_PRIO_P: {
    // YOUR PROJECT2 CODE HERE
    // We fetch the first thread in the ready queue to run
        next_to_run = readyList->Remove();
        break;


  }

  case SCHED_MLFQ: {
    // YOUR PROJECT2 CODE HERE
        int i = 2;
        do{
                next_to_run = MultiLevelList[i].Remove();
                i--;
        }while(i>=0 && next_to_run == NULL);
        break;
  }

  default:
    next_to_run = readyList->Remove();
    break;
  }
  return (next_to_run);
}

//----------------------------------------------------------------------
// Scheduler::ShouldISwitch
//   This function uses the policy information to tell a thread::fork
// to preempt the current thread or to not.  The answer is the domain of
// the scheduler, so it is a member function call.
//----------------------------------------------------------------------
bool Scheduler::ShouldISwitch(Thread *oldThread, Thread *newThread) {
  bool doSwitch;
  switch (policy) {

  case SCHED_FCFS: {
    doSwitch = false;
    break;
  }

  case SCHED_RR: {
    // YOUR PROJECT2 CODE HERE
    // Similar to FCFS, Round robin does not preemtively
    // gives up its CPU resources, thus the value returned is false
        doSwitch = false;
        break;
  }

  case SCHED_PRIO_P: {
    // YOUR PROJECT2 CODE HERE
    // Here, we switch if the priority of the oldthread is less than
    // the priority of the new thread
        if(oldThread->getPriority() < newThread->getPriority()){
                doSwitch = true;
        }
        else
        {
                doSwitch = false;
        }
        break;

  }

  case SCHED_MLFQ: {
    // YOUR PROJECT2 CODE HERE
        if(oldThread->getPriority() < newThread->getPriority()){
                doSwitch = true;
        }
        else
        {
                doSwitch = false;
        }
        break;

  }

  default:
    doSwitch = false;
    break;
  }

  return doSwitch;
}

//----------------------------------------------------------------------
// Scheduler::Run
//      Dispatch the CPU to nextThread.  Save the state of the old thread,
//      and load the state of the new thread, by calling the machine
//      dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//      already been changed from running to blocked or ready (depending).
// Side effect:
//      The global variable currentThread becomes nextThread.
//
//      "nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread) {
  Thread *oldThread = currentThread;

#ifdef USER_PROGRAM                   // ignore until running user programs
  if (currentThread->space != NULL) { // if this thread is a user program,
    currentThread->SaveUserState();   // save the user's CPU registers
    currentThread->space->SaveState();
  }
#endif

  oldThread->CheckOverflow(); // check if the old thread
                              // had an undetected stack overflow

  currentThread = nextThread;        // switch to the next thread
  currentThread->setStatus(RUNNING); // nextThread is now running

  printf("Switching from thread \"%s\" to thread \"%s\"\n",
         oldThread->getName(), nextThread->getName());

  // This is a machine-dependent assembly language routine defined
  // in switch.s.  You may have to think
  // a bit to figure out what happens after this, both from the point
  // of view of the thread and from the perspective of the "outside world".

  SWITCH(oldThread, nextThread);

  //    printf("Now in thread \"%s\"\n", currentThread->getName());

  // If the old thread gave up the processor because it was finishing,
  // we need to delete its carcass.  Note we cannot delete the thread
  // before now (for example, in Thread::Finish()), because up to this
  // point, we were still running on the old thread's stack!
  if (threadToBeDestroyed != NULL) {
    delete threadToBeDestroyed;
    threadToBeDestroyed = NULL;
  }

#ifdef USER_PROGRAM
  if (currentThread->space != NULL) {  // if there is an address space
    currentThread->RestoreUserState(); // to restore, do it.
    currentThread->space->RestoreState();
  }
#endif
}

//---------------------------------------------------------------------
// NEW
// Suspends a thread from execution. The suspended thread is removed
// from ready list and added to suspended list. The suspended thread
// remains there until it is resumed by some other thread. Note that
// it is not an error to suspend an thread which is already in the
// suspended state.
//
// NOTE: This method assumes that interrupts have been turned off.
//---------------------------------------------------------------------
void Scheduler::Suspend(Thread *thread) {
  List<Thread *> *tmp = new List<Thread *>();
  Thread *t;

  // Remove the thread from ready list.
  while (!readyList->IsEmpty()) {
    t = readyList->Remove();
    if (t == thread)
      break;
    else
      tmp->Prepend(t);
  }

  // Add the suspended thread to the suspended list
  if (t == thread) {
    t->setStatus(SUSPENDED);
    suspendedList->Append(t);
  }

  // Now all threads before the suspended thread in the ready list
  // are in the suspended list. Add them back to the ready list.
  while (!tmp->IsEmpty()) {
    t = tmp->Remove();
    readyList->Prepend(t);
  }
}

//---------------------------------------------------------------------
// NEW
// Resumes execution of a suspended thread. The thread is removed
// from suspended list and added to ready list. Note that it is not an
// error to resume a thread which has not been suspended.
//
// NOTE: This method assumes that interrupts have been turned off.
//---------------------------------------------------------------------
void Scheduler::Resume(Thread *thread) {
  List<Thread *> *tmp = new List<Thread *>();
  Thread *t;

  // Remove the thread from suspended list.
  while (!suspendedList->IsEmpty()) {
    t = suspendedList->Remove();
    if (t == thread)
      break;
    else
      tmp->Prepend(t);
  }

  // Add the resumed thread to the ready list
  if (t == thread) {
    t->setStatus(READY);
    readyList->Append(t);
  }

  // Now all threads before the suspended thread in the ready list
  // are in the suspended list. Add them back to the ready list.
  while (!tmp->IsEmpty()) {
    t = tmp->Remove();
    suspendedList->Prepend(t);
  }
}

//----------------------------------------------------------------------
// Scheduler::Print
//      Print the scheduler state -- in other words, the contents of
//      the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print() {
  printf("Ready list contents:\n");
  readyList->Mapcar((VoidFunctionPtr)ThreadPrint);
}

//----------------------------------------------------------------------
// Scheduler::InterruptHandler
//   Handles timer interrupts for Round-robin scheduling.  Since this
//   is called while the system is an interrupt handler, use YieldOnReturn.
//   Be sure that your scheduling policy is still Round Robin.
//----------------------------------------------------------------------
void Scheduler::InterruptHandler(int dummy) {
  switch (policy) {

  case SCHED_FCFS: {
    break;
  }

  case SCHED_RR: {
    // YOUR PROJECT2 CODE HERE
    // Check again if scheduling policy is round robin
    if(policy==SCHED_RR){
        //proceed to the if the status is not IdleMode
        int remQ = currentThread->decrementQuantum();
        if(interrupt->getStatus() != IdleMode && remQ<=0){
                //preempt the thread by callling YieldOnReturn()
                interrupt->YieldOnReturn();
        }
    }
  }

  case SCHED_PRIO_P: {
    break;
  }

  case SCHED_MLFQ: {
    // YOUR PROJECT2 CODE HERE
          if(policy==SCHED_MLFQ){
                //proceed to the if the status is not IdleMode
                if(currentThread->getPriority()>=1){
                int remQ = currentThread->decrementQuantum();

                if(interrupt->getStatus() != IdleMode && remQ<=0){
                        //preempt the thread by callling YieldOnReturn()
                        int decrementPriority = currentThread->getPriority() - 1;
                        currentThread->setPriority(decrementPriority);
                        interrupt->YieldOnReturn();
                }
                }
        }

  }

  default:
    break;
  }
}

// This is needed because GCC doesn't like passing pointers to member functions.
void SchedInterruptHandler(int dummy) { scheduler->InterruptHandler(dummy); }

//----------------------------------------------------------------------
// Scheduler::SetSchedPolicy
//      Set the scheduling policy to one of SCHED_FCFS, SCHED_SJF,
//      SCHED_PRIO_NP，SCHED_PRIO_P
//----------------------------------------------------------------------
void Scheduler::SetSchedPolicy(SchedPolicy pol) {
  ptimer = new Timer(SchedInterruptHandler, 0, false);
  SchedPolicy oldPolicy = policy;
  policy = pol;
  switch (policy) {
  case SCHED_FCFS:
    printf("First-come first-served scheduling\n");
    break;
  case SCHED_SJF:
    printf("Shortest job first scheduling\n");
    break;
  case SCHED_RR:
    printf("Round robin scheduling\n");
    break;
  case SCHED_PRIO_NP:
    printf("Non-preemptive Priority scheduling\n");
    break;
  case SCHED_PRIO_P:
    printf("Preemptive Priority scheduling\n");
    break;
  case SCHED_MLQ:
    printf("Multi level queue scheduling\n");
    break;
  case SCHED_MLFQ:
    printf("Multi level feedback queue scheduling\n");
    break;
  default:
    break;
  }

  // YOUR MODIFICATIONS TO THIS FUNCTION ARE SUGGESTED.
}

//----------------------------------------------------------------------
// Scheduler::SetNumOfQueues
//      Set the number of queues for MLQ - should be called only once
//----------------------------------------------------------------------
void Scheduler::SetNumOfQueues(int level) {
  NumOfLevel = level;
  MultiLevelList = new List<Thread *>[level];
}
