#include <iostream>
#include <unistd.h>

#include "VirtualMachine.h"
#include "Machine.h"

using namespace std;

volatile unsigned int counter = 0;

extern "C" {

  TVMMainEntry VMLoadModule(const char *module);

  void AlarmCall(void *param)
  {

    cout << "tick\n";

    if(counter > 0)
    {

      --counter;

    } 

  }

  TVMStatus VMThreadSleep(TVMTick tick)
  {

    counter = tick;

    while(counter)
    {

       cout << counter << "\n";
       cout << "Sleeping\n"; 

    }

  }

  TVMStatus VMTickMS(int *tickmsref)
  {

    

  }

  TVMStatus VMStart(int tickms, int argc, char *argv[])
  {
    string module_name(argv[0]);
    TVMMainEntry main_entry = VMLoadModule(module_name.c_str());

    MachineInitialize();
    MachineRequestAlarm(tickms*1000, AlarmCall, NULL);
    MachineEnableSignals();

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

}
