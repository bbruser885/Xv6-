#ifdef CS333_P5
#include "types.h"
#include "user.h"
#include "stat.h"

int
uatoo(const char*s){
 int n, sign;
 
 n= 0;
 while(*s == ' ') s++;
 sign = (*s == '-')? -1 : 1;
 if( *s == '+' || *s == '-')
  s++;
 while('0' <= *s && *s <= '7')
    n = n*8 + *s++ - '0';
 return sign*n;
}

int
main(int argc, char ** argv)
{
  char * usrMode = argv[1];
  char * pathname = argv[2];
  int octMode = uatoo(usrMode);
 
  if(argc != 3) 
  {
    printf(1, "Invalid Arguments -- Usage: chmod [MODE] [TARGET]\n");
    exit();
  }
 
  if(strlen(usrMode) != 4)
  {
    printf(1, "Invalid Mode: mode must contain 4 digits\n");
    exit();
  }
  if(usrMode[0] < '0' || usrMode[0] > '1') 
  {
    printf(1, "Invalid Mode: first mode value must be '1' or '0'\n");
    exit();
  } 
  for(int i = 1; i < 4; i++)
  {
    if(usrMode[i] < '0' || usrMode[i] > '7')
    { 
      printf(1, "Invalid Mode: mode values after 0th location must be between '0' and '7'\n");
      exit();
    }
  }

  if(chmod(pathname, octMode) < 0)
  {
    printf (1, "Chmod failed to set mode\n");
    exit();
  }
  exit();
}

#endif
