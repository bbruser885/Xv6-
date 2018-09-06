#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char ** argv)
{
  char * uidStr = argv[1];
  char * filepath = argv[2];
  int uid = atoi(uidStr);
  
  if(argc != 3) {
    printf(1, "Invalid arguments! -- Usage: chown [UID] [TARGET]\n");
    exit();
  }
 
  if(uidStr[0] == '-')
  {
    printf(1, "Invalid UID -- Must be a positive integer.\n");
    exit();
  }   
  
  if(uid > 32767 || uid < 0)
  {
    printf(1, "Invalid UID -- Out of range.\n");
    exit();
  }
  
  if(chown(filepath, uid) < 0)
  {
     printf(1, "Chown failed to set UID.\n");
  }

  exit();
}

#endif
