#include "types.h" 
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "uproc.h"

#ifdef CS333_P3P4
struct StateList {
  struct proc* ready[MAX+1];
  struct proc* free;
  struct proc* sleep;
  struct proc* zombie;
  struct proc* running;
  struct proc* embryo;
};
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3P4
  struct StateList pLists;
  uint PromoteAtTime;
#endif
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
 
#ifndef CS333_P3P4 
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
#else   
  acquire(&ptable.lock);
  p = removeFromStateListHead(&ptable.pLists.free, UNUSED);
  if(p)
     goto found;
  release(&ptable.lock);
#endif
  return 0;

found:
  p->state = EMBRYO;
#ifdef CS333_P3P4  
  addToStateEnd(&ptable.pLists.embryo, p);
#endif
  p->pid = nextpid++;
  p->prio = 0;
  p->budget = BUDGETVAL;
  release(&ptable.lock);
   
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);
#ifdef CS333_P3P4  
    removeFromStateList(&ptable.pLists.embryo, p, EMBRYO);
    p->state = UNUSED;
    addToStateHead(&ptable.pLists.free, p);
#else
    p->state = UNUSED;
#endif
    release(&ptable.lock);
    
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
	
  //initialize start ticks to the current ticks counter
  p->start_ticks = ticks;
  
#ifdef CS333_P2  
  //initialize cpu_-ticks_total and cpu_ticks_in to zero
  p->cpu_ticks_in = 0;
  p->cpu_ticks_total = 0;

#endif
  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
#ifdef CS333_P3P4 
 ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;  
  
  acquire(&ptable.lock); 
  for(int i = 0; i < MAX+1; i++)
  {
    ptable.pLists.ready[i] = 0;
  }
  ptable.pLists.sleep = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.running = 0;
  ptable.pLists.embryo = 0;
  ptable.pLists.free = &ptable.proc[0];
  for(int i = 0; i < NPROC-1; i++)
  {
    ptable.proc[i].next = &ptable.proc[i+1];
  }

  ptable.proc[NPROC-1].next = 0;
  release(&ptable.lock);
#endif
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  acquire(&ptable.lock);

#ifdef CS333_P3P4  
  removeFromStateList(&ptable.pLists.embryo, p, EMBRYO);
  p->state = RUNNABLE;
  addToStateEnd(&ptable.pLists.ready[0], p);
#else
  p->state = RUNNABLE;
#endif
  release (&ptable.lock);
  p->gid = 0;
  p->uid = 0;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    acquire(&ptable.lock);
#ifdef CS333_P3P4
    removeFromStateList(&ptable.pLists.embryo, np, EMBRYO);
    np->state = UNUSED;
    addToStateHead(&ptable.pLists.free, np);
#else
    np->state = UNUSED;
#endif
    release(&ptable.lock);
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

#ifdef CS333_P2
  //IDs are copied
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.

  acquire(&ptable.lock);
#ifdef CS333_P3P4
  removeFromStateList(&ptable.pLists.embryo, np, EMBRYO);
  np->state = RUNNABLE;
  addToStateEnd(&ptable.pLists.ready[0], np);
#else
  np->state = RUNNABLE;
#endif
  release(&ptable.lock);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);
  
  for(int headNumber = 1; headNumber < 7; headNumber++)
  {
     // Pass abandoned children to init.
    for(p = headFinder(headNumber); p; p = p->next){
      if(p->parent == proc){
        p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
      }
    }
  }

  // Jump into the scheduler, never to return.
  removeFromStateList(&ptable.pLists.running, proc, RUNNING);
  proc->state = ZOMBIE;
  addToStateHead(&ptable.pLists.zombie, proc);
  

  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    
    for(int headNumber = 2; headNumber < 7; headNumber++)
    { 
      for(p = headFinder(headNumber); p; p = p->next){
        if(p->parent != proc)
          continue;
        havekids = 1;
        if(p->state == ZOMBIE){
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
        
          removeFromStateList(&ptable.pLists.zombie, p, ZOMBIE);
          p->state = UNUSED;
          addToStateHead(&ptable.pLists.free, p);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          release(&ptable.lock);
          return pid;
          }
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;

#ifdef CS333_P2 
      p->cpu_ticks_in = ticks;
#endif
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}
#else
void
scheduler(void)
{
 struct proc *p;
  int idle;  // for checking if processor is idle
 
  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    
    if(ticks >= ptable.PromoteAtTime)
    {
	ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
        promoteReady();
        promoteProcess();
    }
     for(int i = 0; i < MAX+1; i++)
     {
       if((p = removeFromStateListHead(&ptable.pLists.ready[i], RUNNABLE)) != 0)
       {
         break;
       }
     }
     if(p)
     {  
       
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p; 
#ifdef CS333_P2 
      p->cpu_ticks_in = ticks;
#endif
      switchuvm(p);
      p->state = RUNNING;
      addToStateHead(&ptable.pLists.running, p);
      swtch(&cpu->scheduler, proc->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
      }
      release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
      }
    }
  
}
#endif 

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
#ifndef CS333_P3P4
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  
#ifdef CS333_P2
  proc->cpu_ticks_total += (ticks - proc->cpu_ticks_in);
#endif
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}
#else
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  
  proc->cpu_ticks_total += (ticks - proc->cpu_ticks_in);
  proc->budget = proc->budget - (ticks - proc->cpu_ticks_in);
  
  if(proc->budget <= 0 && proc->prio != MAX)
  {
    if(proc->state == RUNNABLE)
    {
      removeFromStateList(&ptable.pLists.ready[proc->prio], proc, RUNNABLE);
      proc->prio++;
      addToStateEnd(&ptable.pLists.ready[proc->prio], proc);
    }
    else
    {
      proc->prio++;
    }
    proc->budget = BUDGETVAL;
  }

  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;  
}
#endif

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
#ifdef CS333_P3P4
  removeFromStateList(&ptable.pLists.running, proc, RUNNING);
  proc->state = RUNNABLE;
  addToStateEnd(&ptable.pLists.ready[proc->prio], proc);
#else
  proc->state = RUNNABLE;
#endif
  
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
#ifdef CS333_P3P4
  removeFromStateList(&ptable.pLists.running, proc, RUNNING);
  proc->state = SLEEPING;
  addToStateHead(&ptable.pLists.sleep, proc);
#else
  proc->state = SLEEPING;
#endif

  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){ 
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
    }
}
#else
static void
wakeup1(void *chan)
{
 struct proc *p;

  for(p = ptable.pLists.sleep; p; p = p->next)
    if(p->state == SLEEPING && p->chan == chan)
    {
      removeFromStateList(&ptable.pLists.sleep, p, SLEEPING);
      p->state = RUNNABLE;
      addToStateEnd(&ptable.pLists.ready[p->prio], p);
    }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;
  
  acquire(&ptable.lock);
  for (int headNumber = 2; headNumber < 7; headNumber++)
  {
    for(p = headFinder(headNumber); p; p = p->next){
      if(p->pid == pid){
        p->killed = 1;
        // Wake process from sleep if necessary.
        if(p->state == SLEEPING)
        {
          removeFromStateList(&ptable.pLists.sleep, p, SLEEPING);
          p->state = RUNNABLE;
          addToStateEnd(&ptable.pLists.ready[p->prio], p);
        }
        release(&ptable.lock);
        return 0;
      }
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

#ifdef CS333_P2
int
setuid(uint * uid)
{
	acquire(&ptable.lock);
	proc->uid = *uid;
	release(&ptable.lock);
	return 0;
}

int
setgid(uint * gid)
{
	acquire(&ptable.lock);
	proc->gid = *gid;
	release(&ptable.lock);
	return 0;
}

static void
printTimeHelper(uint ticks)
{
	uint timeSeconds = ticks / 1000;
	uint decTime = ticks % 1000;
	cprintf("%d.%d\t", timeSeconds, decTime);
}

int
fillTable(uint max, struct uproc* table)
{
	struct proc *p;
	int counter = 0;

	acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[NPROC] && p <  &ptable.proc[max]; p++)
	{
		if(p->state == UNUSED || p->state == EMBRYO)
		continue;

		table->pid = p->pid;
		table->uid = p->uid;
		table->gid = p->gid;
		table->prio = p->prio;
                if(p->pid == 1)
			table->ppid = 1;
 		else
			table->ppid = p->parent->pid;
		table->elapsed_ticks = ticks - p->start_ticks;
		table->CPU_total_ticks = p->cpu_ticks_total;
		safestrcpy(table->state, states[p->state], sizeof(table->state));
		table->size = p->sz;
		safestrcpy(table->name, p->name, sizeof(p->name));
                
		table++;
		counter++;
	}
	release(&ptable.lock);
	return counter;
}
#endif
#ifdef CS333_P3P4
/*===================================================
Removes a process from a state list
===================================================*/
int
removeFromStateList(struct proc** sList, struct proc* p, enum procstate state)
{ 
 if(p->state != state)
 {
   panic("ERROR: Invalid state for list item");
 }

 struct proc * current;
 struct proc * follow;
 current = (*sList);

 if(current != 0 && current == p)
 {
   (*sList) = current->next;
   return 0;
 }
 
 while(current != 0 && current != p)
 {
   follow = current;
   current = current->next;
 }
 
 if(current == 0)
 {
   panic("ERROR: Process was not in the list");
 }
 
 follow->next = current->next;
 return 0;
}
/*================================================
Removes a process from the beginning of a state
list and returns the process removed
================================================*/
struct proc*
removeFromStateListHead(struct proc ** sList, enum procstate state)
{
  struct proc * current = (*sList);
 
  if(!current)
  {
	return current;
  }

  if((*sList)->state != state)
  {
    panic("ERROR: Invalid state");
  }
  
  (*sList) = (*sList)->next;
  return current;
}
/*================================================
Add proccess to the end of the State list
===============================================*/
int 
addToStateEnd(struct proc** sList, struct proc *p)
{
  while((*sList))
  {
    (*sList) = (*sList)->next;
  }
  (*sList) = p;
  p->next = 0;
  return 0;
}
/*=================================================
Add proccess to the start of the State list
=================================================*/
int
addToStateHead(struct proc** sList, struct proc * p)
{ 
  p->next = (*sList);
  (*sList) = p;

  return 0;
}

/*==================================================
print free list
================================================*/
int 
printFree()
{
  struct proc * current = ptable.pLists.free;
  int counter = 1;
 
  if(!current)
  {
    cprintf("Free List Size: 0 \n");
    return 0;
  }
  while(current->next)
  {
    counter++;
    current = current->next;
  }
  cprintf("Free List Size: %d \n", counter);

  return 0;
}
/*=======================================
print ready list
=======================================*/
int 
printReady()
{

  cprintf("Ready List Processes: \n");
  for(int i = 0; i < MAX; i++)
  {
  struct proc * current = ptable.pLists.ready[i];
  cprintf("%d: ", i);
  if(!current)
  {
    cprintf("\n", i);
    continue;
  }
  while(current->next)
  {
    cprintf("(%d,%d)  -> ", i, current->pid, current->budget);
    current = current->next;
  }
  
  cprintf("(%d,%d)\n", current->pid, current->budget);
  }
  return 0;
}
/*=======================================
print zombie list
=======================================*/
int
printZombie()
{
  struct proc * current = ptable.pLists.zombie;

  if(!current)
  {
    cprintf("Zombie List Processes: \n");
    return 0;
  }
  cprintf("Zombie List Processes: \n"); 
  while(current->next)
  {
    cprintf("(%d, %d) -> ", current->pid, current->parent->pid);
    current = current->next;
  }
  
 cprintf("(%d, %d)\n", current->pid, current->parent->pid);
 return 0;
}
/*=====================================
print sleep list
=====================================*/
int
printSleep()
{
  struct proc * current = ptable.pLists.sleep;
 
  if(!current)
  {
    cprintf("Sleep List Processes: \n");
    return 0;
  }
  cprintf("Sleep List Processes: \n");
  while(current->next)
  {
    cprintf("%d -> ", current->pid);
    current = current->next;
  }
  cprintf("%d\n", current->pid);
  return 0;
}
/*====================================
Head finder
====================================*/
struct proc*
headFinder(int headNumber)
{
  switch(headNumber)
  {
    case 1:
	return ptable.pLists.free;
    case 2:
	return ptable.pLists.embryo;
    case 3:
	return ptable.pLists.ready[0];
    case 4:
	return ptable.pLists.running;
    case 5:
	return ptable.pLists.sleep;
    case 6:
	return ptable.pLists.zombie;
    default:
      panic("");
   }
}
/*===================================
Priority setter
===================================*/
int
setprio(int pid, int prio)
{
  struct proc * p;

  if(pid < 0)
  {
    cprintf("Invalid PID\n");
    return -1;
  }
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
	if(p->pid == pid)
        {
	  if((prio > MAX+1) || prio < 0)
           {
            cprintf("Invalid priority");
            return -1;
           }
          else
           {
            p->prio = prio;
            return 0;
           }
        }
  }
  cprintf("Could not find PID\n");
  return -1;

}
/*===================================
Promote Process on Ready List
===================================*/
int
promoteReady()
{
  struct proc * p;
  
  for(int i = 1; i < (MAX+1); i++)
  {
    while(ptable.pLists.ready[i])
    {
      p = removeFromStateListHead(&ptable.pLists.ready[i], RUNNABLE);
      p->prio--;
      p->budget = BUDGETVAL;
      addToStateEnd(&ptable.pLists.ready[i - 1], p);
    }
  }
 
  struct proc * current = ptable.pLists.ready[0];
  while(current)
  {
    current->budget = BUDGETVAL;
    current = current->next;
  }
  return 0;
}
/*===================================
Promote Processes
===================================*/
int
promoteProcess()
{  
  struct proc * curr;
  
  for(int headNumber = 4; headNumber < 6; headNumber++)
  {
    for(curr = headFinder(headNumber); curr; curr=curr->next)
    {
      if(curr->prio > 0)
      {
        curr->prio--;
        curr->budget = BUDGETVAL;
      }
    }
  }
  return 0;
}
#endif
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{

  int i;
  int ppid = 1; 
  struct proc *p;
  char *state;
  uint pc[10];

  cprintf("\nPID\tName\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\tPCs\n");

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
	state = "???";

    if(p->pid != 1)
      ppid = p->parent->pid;
	
    cprintf("%d\t%s\t%d\t%d\t%d\t", p->pid, p->name, p->uid, p->gid, ppid);
    cprintf("%d\t", p->prio);
#ifdef CS333_P2
    printTimeHelper((ticks - p->start_ticks));
    printTimeHelper(p->cpu_ticks_total);
#endif
    cprintf("%s\t%d", state, p->sz);   
	
    if(p->state == SLEEPING){
     getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf("   %p", pc[i]);
    }
    cprintf("\n");
  }
}
