#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char ** argv)
{
  char * userStr = argv[1];
  char * pathname = argv[2];
  int gid = atoi(userStr);

  if(argc != 3) { 
     printf(2, "Invalid Arguments -- Usage: chgrp [GROUP] [TARGET]\n");
     exit();
  }

  if(userStr[0] == '-') {
     printf(2, "Invalid GID --  Must be a positive integer\n");
     exit();
  }
  if(gid > 32767 || gid < 0)
  {
     printf(1, "Invalid GID -- Out of range.\n");
     exit();
  }
  if(chgrp(pathname, gid) < 0)
  {
     printf(2, "Chgrp failed to set GID\n");
  }
  exit();
  
}

#endif
