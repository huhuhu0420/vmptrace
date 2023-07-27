#include "plugin.h"
#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <Windows.h>
#include <string>

using namespace std;
FILE* f;
bool beingHittedVMEntry;
uintptr_t base_addr;
HANDLE hThread;
struct vmp_ins {
    uintptr_t vip;
    string* name;
    string* etc;
};

map<uintptr_t, vmp_ins*> ins_map;

const std::vector<std::string> split(const std::string& str, const char& delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string tok;

    while (std::getline(ss, tok, delimiter)) {
        result.push_back(tok);
    }
    return result;
}

static vmp_ins* getVMPIns() {
    uintptr_t ip = Script::Register::GetCIP();
    DISASM_INSTR instr;
    DbgDisasmAt(ip,  &instr);

    if (instr.type == instr_branch && strlen(instr.instruction) == 7 ) {
        uintptr_t vip_rva = Script::Register::GetCSI() - base_addr;
        vmp_ins* ins = ins_map[vip_rva];
        if (ins) {
            cout << hex << Script::Register::GetCSI()  << " " << *ins->name << " ";
            cout << endl;
            return ins;
        }
    }

    return NULL;
}

static void cbVmpEntry() {
    beingHittedVMEntry = true;
    uintptr_t ip = Script::Register::GetCIP();
    DISASM_INSTR instr;
    DbgDisasmAt(ip, &instr);
    Script::Debug::SetBreakpoint(ip + instr.instr_size);
}



void WINAPI CMDThread() {
    static char cmd1[] = "ticnd 0";
    string inputString;
    while (getline(cin, inputString)) {
        if (!beingHittedVMEntry)
            continue;
        if (inputString == "vmpsti") 
            DbgCmdExec(cmd1);
    }
}

static bool cbVmprofiler(int argc, char** argv) {
    base_addr = DbgEval("mod.main()");
    SetBPX(base_addr + 0xbc20, UE_BREAKPOINT, cbVmpEntry);
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, NULL, 0, NULL);
    return true;
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info){
    cbVmprofiler(0, NULL);
}

PLUG_EXPORT void CBSTEPPED(CBTYPE, PLUG_CB_STEPPED* info) {
    if (!beingHittedVMEntry)
        return;
    getVMPIns();
}

PLUG_EXPORT void CBTRACEEXECUTE(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info){
    if (!beingHittedVMEntry)
        return;
    if (getVMPIns())
        info->stop = true;
}

PLUG_EXPORT void CBEXITPROCESS(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info){
    TerminateThread(hThread, 0);
}


bool pluginInit(PLUG_INITSTRUCT* initStruct){
    AllocConsole();
    freopen_s(&f, "CONIN$", "r", stdin);
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);

    ifstream inFile("C:\\Users\\ggmaple555\\Desktop\\AIS3\\vmp.txt", std::ios::in);
    if (!inFile) {
        printf("Failed to open the Ladder file.\n");
        return 0;
    }
    string s;
    //vector< vmp_ins*> ins;
   
    while (getline(inFile, s)) {
        vector<string> sp = split(s, ';');
        vmp_ins* item = (vmp_ins*)malloc(sizeof(vmp_ins));
        stringstream ss; uintptr_t rva;
        ss << std::hex << sp[0];
        ss >> rva;
        item->name = new string(sp[1]);
        //.push_back(item);
        ins_map.insert(pair<uintptr_t, vmp_ins*>(rva, item));
    }
    //cout << *instr_map[0x14000baba]->name;

    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);

    _plugin_registercommand(pluginHandle, "vmprofiler", cbVmprofiler, true);
    beingHittedVMEntry = false;
    return true;
}


void pluginStop(){
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
    FreeConsole();
    fclose(f);
    
    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
    _plugin_unregistercommand(pluginHandle, "vmprofiler");
}

void pluginSetup(){
    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
