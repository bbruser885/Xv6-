#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "date.h"

int
main(int argc, char *argv[])
{
	uint start_ticks;
	uint end_ticks;
	
	start_ticks = uptime();
	int pid = fork();
	
	if(pid < 0)
	{
		printf(2, "invalid pid");
		exit();
	}
	if(pid > 0)
	{
		wait();
	}
	if(pid == 0)
	{
		if(exec(argv[1],argv+1))
		{
			printf(2, "ran in 0.00 second\n");
			exit();
		}
	}
	if(argc > 1)
	{
		end_ticks = (uptime() - start_ticks);
		printf(1, "%s ran in %d.%d seconds\n", argv[1], end_ticks / 100, end_ticks % 100);
		exit();
	}
	 
	exit();
}

#endif
