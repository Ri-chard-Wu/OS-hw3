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

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    readyList = new SortedList<ThreadSchedulingBlock *>\
                (ThreadSchedulingBlock::Compare); 
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
} 





void Scheduler::CheckPreempt(Thread *thread){


    
    MachineStatus status = kernel->interrupt->getStatus();
    // if status == IdelMode: no thread is running right now -> no need to preempt.    
    if (status != IdleMode) { 
        

                                    
        double t_cur = (double)kernel->stats->totalTicks;
        double T_cur = t_cur - kernel->currentThread->tsb->t_start + \
                                        kernel->currentThread->tsb->T;
        double t_key_cur = kernel->currentThread->tsb->t_pred - T_cur;

        if(thread->tsb->t_key < t_key_cur){
            DEBUG('z', "[G] Tick [" <<  kernel->stats->totalTicks
                                << "]: Thread [" 
                                << thread->getName() << ", " << thread->getID()
                                << "] can preempt cur thrad. cur thread remaining time ["
                                << t_key_cur
                                << "], new ready thread pred time ["
                                << thread->tsb->t_key
                                << "]"
                                
                                );            
            kernel->interrupt->Preempt();
        }
    }
}



void
Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if(thread->getStatus() == BLOCKED){
        CheckPreempt(thread);
    }

    DEBUG('z', "[A] Tick [" <<  kernel->stats->totalTicks
                            << "]: Thread [" 
                            << thread->getName() << ", " << thread->getID()
                            << "] is inserted into queue");

    thread->setStatus(READY);
    readyList->Insert(thread->tsb);
}



Thread *
Scheduler::FindNextToRun()
{

    static int counter = 0;
    counter++;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
        // cout << counter << ": nextThread: NULL\n";
		return NULL;
    } else {

        Thread *nextThread =  readyList->RemoveFront()->thread;
        nextThread->tsb->t_start = (double)kernel->stats->totalTicks;


        DEBUG('z', "[B] Tick [" <<  kernel->stats->totalTicks
                                << "]: Thread [" 
                                << nextThread->getName() << ", " << nextThread->getID()
                                << "] is removed from queue");
                                
        // cout << counter << ": nextThread: " << nextThread->getName() << "\n";

    	return nextThread;
    }
}



void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    // previous running thread still has shorted estimated burst time.
    if(nextThread == kernel->currentThread){ 
        ASSERT(kernel->interrupt->getLevel() == IntOff);
        nextThread->setStatus(RUNNING);
        nextThread->CheckOverflow();	

    }
    else{

        Thread *oldThread = kernel->currentThread;
        ASSERT(kernel->interrupt->getLevel() == IntOff);
        if (finishing) {	
            ASSERT(toBeDestroyed == NULL);
            toBeDestroyed = oldThread;
        }

        if (oldThread->space != NULL) {	
            oldThread->SaveUserState(); 
            oldThread->space->SaveState();
        }
        
        oldThread->CheckOverflow();		    
        kernel->currentThread = nextThread; 
        nextThread->setStatus(RUNNING);     

        SWITCH(oldThread, nextThread);

        ASSERT(kernel->interrupt->getLevel()== IntOff);

        CheckToBeDestroyed();		
        if (oldThread->space != NULL) {	    
            oldThread->RestoreUserState();  
            oldThread->space->RestoreState();
        }
        
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
    // cout << "Ready list contents:\n";
    // readyList->Apply(ThreadPrint);
    cout << "[Scheduler::Print()] Not implemented\n";
}
