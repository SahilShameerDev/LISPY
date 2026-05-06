#include<stdio.h>

#if defined(_WIN32)
void print(){
  puts("This is a windows system");
}

#elif defined(__linux__)
void print(){
  puts("This is linux");
}

#else 
void print(){
  puts("This is mac");
}

#endif

int main(int argc, char *argv[])
{
  print();
  return 1;
}

