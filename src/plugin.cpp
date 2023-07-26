#include "plugin.h"
#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

using namespace std;
FILE* f;

// Examples: https://github.com/x64dbg/x64dbg/wiki/Plugins
// References:
// - https://help.x64dbg.com/en/latest/developers/plugins/index.html
// - https://x64dbg.com/blog/2016/10/04/architecture-of-x64dbg.html
// - https://x64dbg.com/blog/2016/10/20/threading-model.html
// - https://x64dbg.com/blog/2016/07/30/x64dbg-plugin-sdk.html

struct vmp_instr {
    uintptr_t vip;
    string* name;
    string* etc;
};

const std::vector<std::string> split(const std::string& str, const char& delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string tok;

    while (std::getline(ss, tok, delimiter)) {
        result.push_back(tok);
    }
    return result;
}

static void cbVmpEntry() {
    DbgCmdExecDirect("sti");
    uintptr_t aa = Script::Register::GetCIP();
    cout << hex << aa << endl;;
    DbgCmdExecDirect("sti");
    aa = Script::Register::GetCIP();
    cout << hex<< aa;
    
}

static bool cbVmprofiler(int argc, char** argv) {

    uintptr_t base_addr = DbgEval("mod.main()");

    SetBPX(base_addr + 0xbc20, UE_SINGLESHOOT, cbVmpEntry);
   
    return true;
}

/*callback each step*/
PLUG_EXPORT void CBSTEPPED(CBTYPE, PLUG_CB_STEPPED* info)
{
	
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info)
{
 
    cbVmprofiler(0, NULL);
}




// Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT* initStruct)
{
   
    _plugin_registercommand(pluginHandle, "vmprofiler", cbVmprofiler, true);
   
    AllocConsole();
    
    freopen_s(&f, "CONIN$", "r", stdin);
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    ifstream inFile(VMPPATH, std::ios::in);
    if (!inFile) {
        printf("Failed to open the Ladder file.\n");
        return 0;
    }
    string s;
    vector< vmp_instr*> ins;
    map<uintptr_t, vmp_instr*> instr_map;
    while (getline(inFile, s)) {
        vector<string> sp = split(s, ';');
        vmp_instr* item = (vmp_instr*)malloc(sizeof(vmp_instr));
        stringstream ss;
        ss << std::hex << sp[0];
        ss >> item->vip;
        item->name = new string(sp[1]);
        ins.push_back(item);
        instr_map.insert(pair<uintptr_t, vmp_instr*>(item->vip, item));
        cout << s << endl;
    }
    //cout << *instr_map[0x14000baba]->name;
    dprintf("pluginInit(pluginHandle: %d)\n", pluginHandle);

    // Use a while loop together with the getline() function to read the file line by line

    // Prefix of the functions to call here: _plugin_register
    

    // Return false to cancel loading the plugin.
    return true;
}

// Deinitialize your plugin data here.
// NOTE: you are responsible for gracefully closing your GUI
// This function is not executed on the GUI thread, so you might need
// to use WaitForSingleObject or similar to wait for everything to close.
void pluginStop()
{
    // Prefix of the functions to call here: _plugin_unregister

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
    FreeConsole();
    fclose(f);

    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
}

// Do GUI/Menu related things here.
// This code runs on the GUI thread: GetCurrentThreadId() == GuiGetMainThreadId()
// You can get the HWND using GuiGetWindowHandle()
void pluginSetup()
{
    // Prefix of the functions to call here: _plugin_menu

    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
