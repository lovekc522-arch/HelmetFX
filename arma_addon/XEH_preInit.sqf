
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
    ["Enable Helmet FX", "Master switch."],
    ["HelmetFX", "General"], true, 0
] call CBA_fnc_addSetting;

{
    _x params ["_slot", "_getter", "_root", "_title"];

    [
        format ["helmetfx_%1Enabled", _slot], "CHECKBOX",
        [format ["Enable %1 FX", _title], format ["Allow %1 items to apply an effect chain. Off = this slot is skipped entirely.", toLower _title]],
        ["HelmetFX", "Slots"], true, 1
    ] call CBA_fnc_addSetting;

    [
        format ["helmetfx_%1Chains", _slot], "EDITBOX",
        [format ["%1 Chains", _title], "classname=stage:key=val|stage:key=val;classname,classname2=stage|... Empty chain = no effect for that item. Overrides the item's own HelmetFXChain. Search order: facewear, helmet, uniform, NVG, backpack, vest; the first item with a chain wins."],
        ["HelmetFX", "Chains"], "", 1
    ] call CBA_fnc_addSetting;
} forEach helmetfx_slots;