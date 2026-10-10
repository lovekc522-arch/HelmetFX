// XEH_preInit.sqf
helmetfx_slots = [
    ["facewear", {goggles player},  "CfgGlasses",  "Facewear"],
    ["helmet",   {headgear player}, "CfgWeapons",   "Helmet"],
    ["uniform",  {uniform player},  "CfgWeapons",   "Uniform"],
    ["nvg",      {hmd player},      "CfgWeapons",   "NVG"],
    ["backpack", {backpack player}, "CfgVehicles",  "Backpack"],
    ["vest",     {vest player},     "CfgWeapons",   "Vest"]
];

["helmetfx_enabled", "CHECKBOX",
    ["Enable Helmet FX", "Master switch. Off = No Processing."],
    ["HelmetFX", "General"], true, 0
] call CBA_fnc_addSetting;

["helmetfx_autoInstall", "CHECKBOX",
    ["Auto-install TeamSpeak plugin", "Checks for and installs TS3 plugin update on game join."],
    ["HelmetFX", "General"], true, 0
] call CBA_fnc_addSetting;

{
    _x params ["_slot", "_getter", "_root", "_title"];

    [
        format ["helmetfx_%1Enabled", _slot], "CHECKBOX",
        [format ["Enable %1 FX", _title], format ["Off = this slot is skipped entirely.", toLower _title]],
        ["HelmetFX", "Slots"], true, 1
    ] call CBA_fnc_addSetting;

    [
        format ["helmetfx_%1Chains", _slot], "EDITBOX",
        [format ["%1 Chains", _title], "classname=stage:key=val|stage:key=val;classname2=stage|... Search order: facewear, helmet, uniform, NVG, backpack, vest;"],
        ["HelmetFX", "Chains"], "", 1
    ] call CBA_fnc_addSetting;
} forEach helmetfx_slots;