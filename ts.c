// Test program for CS333 scheduler, project 4.
#include "types.h"
#include "user.h"
// Must match NPRIO in proc.h
#define PrioCount 7
#define numChildren 10

void
countForever(int i)
{
  int j, p, rc;
  unsigned long count = 0;
  j = 1;
  p = 0;
  rc = setpriority(j, p);
  if (rc == 0)
  { 
    printf(1, "%d: start prio %d\n", j, p);
  }
  else {
    printf(1, "setpriority failed. file %s at %d\n", __FILE__, __LINE__);
    exit();
  }

  while (1) {
    count++;
    if ((count & (0x1FFFFFFF)) == 0) {
      p = (p+1) % PrioCount;
      rc = setpriority(j, p);
      if (rc == 0) 
	printf(1, "%d: new prio %d\n", j, p);
      else {
	printf(1, "setpriority failed. file %s at %d\n", __FILE__, __LINE__);
	exit();
      }
    }
  }
}
void
schedTest()
{
  int n, ppid;
  ppid = getpid();

  for(n = 0; n < 6; n++)
  {
    if(getpid() == ppid)
	 fork();
    else
	for(;;);
  }

  sleep(200);
  if(ppid == getpid()){
  
  for(int i = ppid + 1; i < ppid + 7; i++)
  {
    setpriority(i, 0);
  }
 }
}

void 
invalidTest()
{
  int rc;
  rc = setpriority(1, 400);
  if(rc == -1)
   printf(1, "400 is an Invalid priority!\n");
  
  rc = setpriority(-1, 2);
  if(rc == -1)
   printf(1, "-1 is an Invalid PID!\n");

}
int
main(void)
{
    int i, rc;
//    invalidTest();
//  countForever(1);

  for (i=0; i<numChildren; i++) {
     rc = fork();
    if (!rc) { // child
      countForever(i);
    }
    }
    //what the heck, let's have the parent waste time as well!
  //countForever(1);
 // schedTest();
  exit();

}
