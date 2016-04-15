#include <iostream>

#include "VirtualMachine.h"

using namespace std;

TVMStatus VMStart(int tickms, int argc, char *argv[])
{
  return VM_STATUS_SUCCESS;
}

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{ 
  if (*length == NULL)
    return VM_STATUS_ERROR_INVALID_PARAMETER;
  return VM_STATUS_SUCCESS;
}

