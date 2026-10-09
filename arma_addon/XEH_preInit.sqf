// XEH_preInit.sqf: CBA settings must be registered in preInit.

["helmetfx_enabled", "CHECKBOX",
    ["Enable Helmet FX", "Apply your helmet's effect chain to your outgoing voice."],
    ["HelmetFX", "General"], true, 0
] call CBA_fnc_addSetting;

// Separate helmet entries with semicolons. The chain itself may contain pipes and commas.
[
    "helmetfx_helmetChains", "EDITBOX",
    ["Helmet Chains", "classname=stage:key=val|stage:key=val;classname=stage:key=val|... Empty = no effect."],
    ["HelmetFX", "HelmetFX Chains"],
    "",
    1
] call CBA_fnc_addSetting;
