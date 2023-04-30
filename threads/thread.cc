// thread.cc 
//	Routines to manage threads.  These are the main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Begin -- called when the forked procedure starts up, to turn
//		interrupts on and clean up after last thread
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly 
//		put back on the ready queue.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "sysdep.h"

// this is put at the top of the execution stack, for detecting stack overflows
const int STACK_FENCEPOST = 0xdedbeef;

//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//----------------------------------------------------------------------



Thread::Thread(char* threadName, int threadID)
{
	ID = threadID;
    name = threadName;
    stackTop = NULL;
    stack = NULL;
    status = JUST_CREATED;
    for (int i = 0; i < MachineStateSize; i++) {
	machineState[i] = NULL;		// not strictly necessary, since
					// new thread ignores contents 
					// of machine registers
    }
    space = NULL;

    tsb = new ThreadSchedulingBlock;
    tsb->thread = this;
    tsb->t_pred = 0;
    tsb->t_key = 0;
    tsb->T = 0;
}

//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread()
{
    DEBUG(dbgThread, "Deleting thread: " << name);
    ASSERT(this != kernel->currentThread);
    if (stack != NULL)
	DeallocBoundedArray((char *) stack, StackSize * sizeof(int));
    
    delete tsb;
}

//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute 
//	concurrently.
//
//	NOTE: although our definition allows only a single argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		cause it to run the procedure
//		3. Put the thread on the ready queue
// 	
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------

void 
Thread::Fork(VoidFunctionPtr func, void *arg)
{
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
    
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int) func << " " << arg);
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts 
					// are disabled!
    (void) interrupt->SetLevel(oldLevel);
}    

//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------

void
Thread::CheckOverflow()
{
    if (stack != NULL) {
#ifdef HPUX			// Stacks grow upward on the Snakes
	ASSERT(stack[StackSize - 1] ==\
                     STACK_FENCEPOST);
#else
	ASSERT(*stack == STACK_FENCEPOST);
#endif
   }
}

//----------------------------------------------------------------------
// Thread::Begin
// 	Called by ThreadRoot when a thread is about to begin
//	executing the forked procedure.
//
// 	It's main responsibilities are:
//	1. deallocate the previously running thread if it finished 
//		(see Thread::Finish())
//	2. enable interrupts (so we can get time-sliced)
//----------------------------------------------------------------------

void
Thread::Begin ()
{
    ASSERT(this == kernel->currentThread);    
    kernel->scheduler->CheckToBeDestroyed();
    kernel->interrupt->Enable();
}



void
Thread::Finish ()
{
    (void) kernel->interrupt->SetLevel(IntOff);		
    ASSERT(this == kernel->currentThread);
    Sleep(TRUE);				// invokes SWITCH
}



// void
// Thread::Yield()
// {
//     Thread *nextThread;
//     IntStatus oldLevel = \
//             kernel->interrupt->SetLevel(IntOff);
//     ASSERT(this == kernel->currentThread);
//     nextThread = \
//             kernel->scheduler->FindNextToRun();
//     if (nextThread != NULL) {
//         kernel->scheduler->ReadyToRun(this);
//         kernel->scheduler->Run(nextThread, FALSE);
//     }
//     (void) kernel->interrupt->SetLevel(oldLevel);
// }


void
Thread::Yield() // preemption
{
    
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    ASSERT(this == kernel->currentThread);


    // update tsb for running -> ready.
    double t_cur = (double)kernel->stats->totalTicks;
    ThreadSchedulingBlock *tsb = this->tsb;
    tsb->T += t_cur - tsb->t_start;

    tsb->t_key = tsb->t_pred - tsb->T;
    if(tsb->t_key < 0){
        tsb->t_key = 0;
    }

    
    kernel->scheduler->ReadyToRun(this);

    Thread *nextThread = kernel->scheduler->FindNextToRun();


    DEBUG('z', "[E] Tick [" <<  kernel->stats->totalTicks
                            << "]: Thread [" 
                            << nextThread->getName() << ", " << nextThread->getID()
                            << "] is now selected for execution, thread ["
                            << this->getName() << ", " << this->getID()
                            << "] is preempted, and it has executed ["
                            << tsb->T
                            << "] ticks"
                            );


    // `nextThread` can be the same as `kernel->currentThread`.
    kernel->scheduler->Run(nextThread, FALSE);

    (void) kernel->interrupt->SetLevel(oldLevel);
}


// void
// Thread::Sleep(bool finishing)
// {
//     Thread *nextThread;
    
//     ASSERT(this == kernel->currentThread);
//     ASSERT(kernel->interrupt->getLevel() == IntOff);
    

//     status = BLOCKED;
//     while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {
// 		    kernel->interrupt->Idle();	
// 	   }    

//     kernel->scheduler-> Run(nextThread, finishing); 
// }


void
Thread::Sleep(bool finishing)
{
    ASSERT(this == kernel->currentThread);
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    double t_accu;
    double t_pred_prev;

    if(!finishing){

        double t_cur = (double)kernel->stats->totalTicks;
        ThreadSchedulingBlock *tsb = this->tsb;

        tsb->T += t_cur - tsb->t_start;

        t_accu = tsb->T;
        t_pred_prev = tsb->t_pred;

        tsb->t_pred = 0.5 * tsb->T + 0.5 * tsb->t_pred;
        tsb->t_key = tsb->t_pred;
        tsb->T = 0;


        DEBUG('z', "[C] Tick [" <<  kernel->stats->totalTicks
                                << "]: Thread [" 
                                << this->getName() << ", " << this->getID()
                                << "] update approximate burst time, from: ["
                                << t_pred_prev
                                << "], add ["
                                << t_accu
                                << "], to ["
                                << tsb->t_pred
                                << "]"
                                );


    }


    Thread *nextThread;
    status = BLOCKED;
    while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {
		kernel->interrupt->Idle();	
	}    
    

    if(!finishing){
        DEBUG('z', "[D] Tick [" <<  kernel->stats->totalTicks
                                << "]: Thread [" 
                                << nextThread->getName() << ", " << nextThread->getID()
                                << "] is now selected for execution, thread ["
                                << this->getName() << ", " << this->getID()
                                << "] starts IO, and it has executed ["
                                << t_accu
                                << "] ticks"
                                );
    }

    kernel->scheduler->Run(nextThread, finishing); 
}




//----------------------------------------------------------------------
// ThreadBegin, ThreadFinish,  ThreadPrint
//	Dummy functions because C++ does not (easily) allow pointers to member
//	functions.  So we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the 
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish()    { kernel->currentThread->Finish(); }
static void ThreadBegin() { 
    kernel->currentThread->Begin(); 
}
void ThreadPrint(Thread *t) { t->Print(); }

#ifdef PARISC

//----------------------------------------------------------------------
// PLabelToAddr
//	On HPUX, function pointers don't always directly point to code,
//	so we need to do the conversion.
//----------------------------------------------------------------------

static void *
PLabelToAddr(void *plabel)
{
    int funcPtr = (int) plabel;

    if (funcPtr & 0x02) {
        // L-Field is set.  This is a PLT pointer
        funcPtr -= 2;	// Get rid of the L bit
        return (*(void **)funcPtr);
    } else {
        // L-field not set.
        return plabel;
    }
}
#endif

//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------

void
Thread::StackAllocate (VoidFunctionPtr func, void *arg)
{
    stack = (int *) AllocBoundedArray(StackSize * sizeof(int));

#ifdef PARISC
    // HP stack works from low addresses to high addresses
    // everyone else works the other way: from high addresses to low addresses
    stackTop = stack + 16;	// HP requires 64-byte frame marker
    stack[StackSize - 1] = STACK_FENCEPOST;
#endif

#ifdef SPARC
    stackTop = stack + StackSize - 96; 	// SPARC stack must contains at 
					// least 1 activation record 
					// to start with.
    *stack = STACK_FENCEPOST;
#endif 

#ifdef PowerPC // RS6000
    stackTop = stack + StackSize - 16; 	// RS6000 requires 64-byte frame marker
    *stack = STACK_FENCEPOST;
#endif 

#ifdef DECMIPS
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif

#ifdef ALPHA
    stackTop = stack + StackSize - 8;	// -8 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif


#ifdef x86
    // the x86 passes the return address on the stack.  In order for SWITCH() 
    // to go to ThreadRoot when we switch to this thread, the return addres 
    // used in SWITCH() must be the starting address of ThreadRoot.
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
    *(--stackTop) = (int) ThreadRoot;
    *stack = STACK_FENCEPOST;
#endif
    
#ifdef PARISC
    machineState[PCState] = PLabelToAddr(ThreadRoot);
    machineState[StartupPCState] = PLabelToAddr(ThreadBegin);
    machineState[InitialPCState] = PLabelToAddr(func);
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = PLabelToAddr(ThreadFinish);
#else
    machineState[PCState] = (void*)ThreadRoot;
    machineState[StartupPCState] =\
                         (void*)ThreadBegin;
    machineState[InitialPCState] =\
                         (void*)func;
    machineState[InitialArgState] =\
                         (void*)arg;
    machineState[WhenDonePCState] =\
                         (void*)ThreadFinish;
#endif
}

#include "machine.h"

//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------

void
Thread::SaveUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	userRegisters[i] = kernel->machine->ReadRegister(i);
}

//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------

void
Thread::RestoreUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	kernel->machine->WriteRegister(i, userRegisters[i]);
}


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

static void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	cout << "*** thread " << which << " looped " << num << " times\n";
        kernel->currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// Thread::SelfTest
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
Thread::SelfTest()
{
    DEBUG(dbgThread, "Entering Thread::SelfTest");

    Thread *t = new Thread("forked thread", 1);

    t->Fork((VoidFunctionPtr) SimpleThread, (void *) 1);
    kernel->currentThread->Yield();
    SimpleThread(0);
}
