#include <iostream>
#include <unistd.h>

#include "VirtualMachine.h"

using namespace std;

extern "C" {

  TVMMainEntry VMLoadModule(const char *module);

  TVMStatus VMStart(int tickms, int argc, char *argv[])
  {
    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    string s = "module: " + module_name + "\n";
    VMPrint(s.c_str());
    VMFilePrint(1, s.c_str());

    main_entry(argc, argv);

    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
  { 
    if (*length == NULL)
      return VM_STATUS_ERROR_INVALID_PARAMETER;

    write(filedescriptor, data, *length);
    return VM_STATUS_SUCCESS;
  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {
  }

}
