#include "plugin.h"
#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <Windows.h>
#include <string>
#include <string_view>
#include <format>
#include <vector>
using namespace std;
FILE* f;
bool beingHittedVMEntry;
uintptr_t base_addr;
HANDLE hThread;
struct vmp_ins {
    uintptr_t vip_rva;
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

template <typename... Args>
std::string dyna_print(std::string_view rt_fmt_str, Args&&... args) {
    return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

void tokenize(std::string const& str, const char delim,
    std::vector<std::string>& out)
{
    // construct a stream from the string 
    std::stringstream ss(str);

    std::string s;
    while (std::getline(ss, s, delim)) {
        out.push_back(s);
    }
}

template<class  T>
int getIndex(vector<T> v, int K){
    auto it = find(v.begin(), v.end(), K);

    if (it != v.end()){
        int index = it - v.begin();
        return index;
    }
    else {
        return -1;
    }
}

void show_vmp_ins(vmp_ins ins) {
    string rdi_fm = "[rdi + {}]";
    cout << "0x" << hex << Script::Register::GetCSI() << " " << *ins.name << " " << endl;
    cout << "VSP: 0x" << Script::Register::GetCBP() << endl;
    for (int i = 0; i <= 23; i++) {
        cout << "VREG" << dec << i << ": 0x" << hex << DbgEval(dyna_print(rdi_fm, i).c_str()) << " ";
    }
    cout << endl;
}


static vmp_ins* getVMPIns() {
    uintptr_t ip = Script::Register::GetCIP();
    DISASM_INSTR instr;
    DbgDisasmAt(ip,  &instr);

    if (instr.type == instr_branch && strlen(instr.instruction) == 7 ) {
        uintptr_t vip_rva = Script::Register::GetCSI() - base_addr;
        vmp_ins* ins = ins_map[vip_rva];
        if (ins) {
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


static bool bThread;
int command_state;
vector<uintptr_t> bp_vip_rva;
void WINAPI CMDThread() {
    const char cmd1[] = "ticnd 0";
    string inputString;
    while (bThread) {
        getline(cin, inputString);
        if (!beingHittedVMEntry)
            continue;
        if (inputString == "si") {
            command_state = 0;
            DbgCmdExec(cmd1);
        }
        else if (inputString == "c") {
            command_state = 1;
            DbgCmdExec(cmd1);
        }
        else {
            std::vector<std::string> out;
            tokenize(inputString, ' ', out);
            if (out.size() == 2) 
                if (out[0] == "brva") {
                    stringstream ss;
                    uintptr_t rva;
                    ss << std::hex << out[1];
                    ss >> rva;
                    bp_vip_rva.push_back(rva);
                }
        }
    }
}

static bool cbVmprofiler(int argc, char** argv) {
    base_addr = DbgEval("mod.main()");
    SetBPX(base_addr + 0xbc20, UE_BREAKPOINT, cbVmpEntry);
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, NULL, 0, NULL);
    bThread = true;
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

    if (command_state == 0) {
        vmp_ins* ins = getVMPIns();
        if (ins) {
            info->stop = true;
            show_vmp_ins(*ins);
        }
    }
    else if (command_state == 1) {
        if (bp_vip_rva.size()) {
            vmp_ins* ins = getVMPIns();
            if (ins) {
                if (getIndex<uintptr_t>(bp_vip_rva, ins->vip_rva) >= 0) {
                    info->stop = true;
                    show_vmp_ins(*ins);
                }
            }
        }
    }
  
}

PLUG_EXPORT void CBEXITPROCESS(CBTYPE cbType, PLUG_CB_TRACEEXECUTE* info){
    bThread = false;
    DWORD dwTmp;
    INPUT_RECORD ir[2];
    ir[0].EventType = KEY_EVENT;
    ir[0].Event.KeyEvent.bKeyDown = TRUE;
    ir[0].Event.KeyEvent.dwControlKeyState = 0;
    ir[0].Event.KeyEvent.uChar.UnicodeChar = VK_RETURN;
    ir[0].Event.KeyEvent.wRepeatCount = 1;
    ir[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    ir[0].Event.KeyEvent.wVirtualScanCode = MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC);
    ir[1] = ir[0];
    ir[1].Event.KeyEvent.bKeyDown = FALSE;
    WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), ir, 2, &dwTmp);
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
    while (getline(inFile, s)) {
        vector<string> sp = split(s, ';');
        vmp_ins* item = (vmp_ins*)malloc(sizeof(vmp_ins));
        stringstream ss; uintptr_t rva;
        ss << std::hex << sp[0];
        ss >> rva;
        item->name = new string(sp[1]);
        item->vip_rva = rva;
        ins_map.insert(pair<uintptr_t, vmp_ins*>(rva, item));
    }

    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);

    _plugin_registercommand(pluginHandle, "vmprofiler", cbVmprofiler, true);
    beingHittedVMEntry = false;
    command_state = -1;
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
