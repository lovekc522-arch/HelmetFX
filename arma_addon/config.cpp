// PBO prefix (set by tools/pack_pbo.py): x\helmetfx\addons\main

class CfgPatches {
    class helmetfx_main {
        name = "HelmetFX";
        author = "kes";
        units[] = {};
        weapons[] = {};
        requiredVersion = 2.0;
        requiredAddons[] = {"cba_main"};
    };
};

class Extended_PreInit_EventHandlers {
    class helmetfx_main {
        init = "call compile preprocessFileLineNumbers '\x\helmetfx\addons\main\XEH_preInit.sqf'";
    };
};
class Extended_PostInit_EventHandlers {
    class helmetfx_main {
        init = "call compile preprocessFileLineNumbers '\x\helmetfx\addons\main\XEH_postInit.sqf'";
    };
};
