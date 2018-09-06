#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

void printHeader();
void printData(int cont, struct uproc *procTable);
void timeHandle(uint ticks);

int
main(int argc, char *argv[])
{
	uint max = 16;
	struct uproc *procTable = malloc(max * sizeof(struct uproc));
	if(procTable == 0)
	{
		printf(1, "Allocation of memory failed\n");
		exit();
	}

	int count = getprocs(max, procTable);
	printHeader();
	printData(count, procTable);
 	
}
void
printHeader()
{
	printf(1, "\nPID\tName\tUID\tGID\tPPID\tPrio\tElapsed\t CPU\tState\t Size\n");
}
void
printData(int count, struct uproc *procTable)
{
	for(int i = 0; i < count; i++)
	{
	printf(1, "%d\t %s\t %d\t %d\t %d\t %d\t ", procTable[i].pid, procTable[i].name, procTable[i].uid, procTable[i].gid, procTable[i].ppid, procTable[i].prio);
	timeHandle(procTable[i].elapsed_ticks);
	timeHandle(procTable[i].CPU_total_ticks);
	printf(1, "%s\t %d\n", procTable[i].state, procTable[i].size); 
	}
}
void
timeHandle(uint ticks)
{
	uint sec = (ticks/100);
	uint decSec = ((ticks%100/10));
	uint milSec = (ticks % 10);
	printf(1,"%d.%d%d\t", sec, decSec, milSec);
}
#endif
