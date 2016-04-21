#include <iostream>
#include <unistd.h>
#include <vector>

#include "VirtualMachine.h"
#include "Machine.h"

extern "C" {

  using namespace std;

  class TCB {

    unsigned int TVMMemorySize, *TVMMemorySizeRef;
    unsigned int TVMStatus, *TVMStatusRef;
    unsigned int TVMTick, *TVMTickRef;
    unsigned int TVMThreadID, *TVMThreadIDRef;
    unsigned int TVMMutexID, *TVMMutexIDRef;
    unsigned int TVMThreadPriority, *TVMThreadPriorityRef;
    unsigned int TVMThreadState, *TVMThreadStateRef;

    void (*TVMMainEntry)(int, char*[]);
    void (*TVMThreadEntry)(void *);
    
    void AlarmCall(void *param);

    TMachineSignalStateRef sigstate;

  };


  volatile unsigned int counter = 0;
  static vector<TCB*> threads;


  TVMMainEntry VMLoadModule(const char *module);

  TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
  {

    TCB *thread = new TCB;
    threads.push_back(thread); 
    //MachineSuspendSignals(sigstate);

    

  }

  void AlarmCall(void *param)
  {

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
