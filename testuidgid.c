#ifdef CS333_P2
#include "types.h"
#include "user.h"

//Allows user interaction with the system calls that
//both set and get UIDs and GIDs
int 
main(void)
{
	uint ppid;
	int validTest = 32768;
	int invalidTest = -10;
	
	//UID testing
	printf(2, "\n\nreturn code for setuid(%d) is: %d\n",validTest,  setuid(validTest));
	if(setuid(validTest) == -1)
	{
		printf(2, "setuid(%d) failed: Out of bounds\n", validTest);
	}
	else
	{
		printf(2, "setuid(%d) successful!, UID is now: %d\n", validTest, getuid());
	}
	
	printf(2, "return code for setuid(%d) is: %d\n",invalidTest,  setuid(invalidTest));
	if(setuid(invalidTest) == -1)
	{
		printf(2, "setuid(%d) failed: Out of bounds\n", invalidTest);
	}
	else
	{
		printf(2, "setuid(%d) successful!, UID is now: %d\n", invalidTest, getuid());
	}

	//GID testing	
	printf(2, "return code for setgid(%d) is: %d\n",validTest,  setgid(validTest));
	if(setgid(validTest) == -1)
	{
		printf(2, "setgid(%d) failed: Out of bounds\n", validTest);
	}
	else
	{
		printf(2, "setgid(%d) successful!, GID is now: %d\n", validTest, getgid());
	}
	
	printf(2, "return code for setgid(%d) is: %d\n",invalidTest,  setgid(invalidTest));
	if(setgid(invalidTest) == -1)
	{
		printf(2, "setgid(%d) failed: Out of bounds\n", invalidTest);
	}
	else
	{
		printf(2, "setgid(%d) successful!, GID is now: %d\n", invalidTest, getgid());
	}

	//PPID testing
	ppid = getppid();
	printf(2, "My parent process is: %d\n", ppid);
	printf(2, "Done!\n");

	exit();
}
#endif

