// XEH_postInit.sqf
if (!hasInterface) exitWith {};

helmetfx_lastSent = "<none>";
helmetfx_lastSendTime = -100;
helmetfx_lastLogged = "<none>";
helmetfx_lastSource = "none";
helmetfx_boxCache = createHashMap;
helmetfx_cfgCache = createHashMap;

helmetfx_fnc_boxMap = {
    params ["_slot"];
    private _src = missionNamespace getVariable [format ["helmetfx_%1Chains", _slot], ""];
    private _cached = helmetfx_boxCache getOrDefault [_slot, []];
    if (_cached isNotEqualTo [] && {(_cached select 0) isEqualTo _src}) exitWith { _cached select 1 };

    private _map = createHashMap;
    {
        private _sep = _x find "=";
        if (_sep > 0) then {
            private _chain = trim (_x select [_sep + 1]);
            private _classes = (_x select [0, _sep]) splitString ",";
            {
                private _cls = toLower (trim _x);
                if (_cls isNotEqualTo "" && {(_map getOrDefault [_cls, "<none>"]) isEqualTo "<none>"}) then {
                    _map set [_cls, _chain];
                };
            } forEach _classes;
        };
    } forEach (_src splitString ";");

    helmetfx_boxCache set [_slot, [_src, _map]];
    _map
};

helmetfx_fnc_slotChain = {
    params ["_slot", "_getter", "_root"];
    if !(missionNamespace getVariable [format ["helmetfx_%1Enabled", _slot], true]) exitWith {""};

    private _class = call _getter;
    if (_class isEqualTo "") exitWith {""};

    private _chain = ([_slot] call helmetfx_fnc_boxMap) getOrDefault [toLower _class, "<none>"];
    if (_chain isEqualTo "<none>") then {
        private _key = format ["%1|%2", _root, toLower _class];
        _chain = helmetfx_cfgCache getOrDefault [_key, "<none>"];
        if (_chain isEqualTo "<none>") then {
            private _cfg = configFile >> _root >> _class >> "HelmetFXChain";
            _chain = if (isText _cfg) then {trim (getText _cfg)} else {""};
            helmetfx_cfgCache set [_key, _chain];
        };
    };
    _chain
};

helmetfx_fnc_desiredChain = {
    helmetfx_lastSource = "none";
    if !(missionNamespace getVariable ["helmetfx_enabled", false]) exitWith {""};
    if (isNull player || {!(lifeState player in ["HEALTHY", "INJURED", "INCAPACITATED"])}) exitWith {""};

    private _result = "";
    {
        _x params ["_slot", "_getter", "_root"];
        private _chain = [_slot, _getter, _root] call helmetfx_fnc_slotChain;
        if (_chain isNotEqualTo "") exitWith {
            _result = _chain;
            helmetfx_lastSource = format ["%1 (%2)", _slot, call _getter];
        };
    } forEach helmetfx_slots;
    _result
};

// Sends on change, and re-sends every 2 s as a heartbeat (plugin clears after 8 s of silence).
helmetfx_fnc_sync = {
    private _chain = [] call helmetfx_fnc_desiredChain;
    if (count _chain > 1800) then { _chain = "" };              // datagram size guard
    private _due = (diag_tickTime - helmetfx_lastSendTime) > 2;
    if (_chain isEqualTo helmetfx_lastSent && {!_due}) exitWith {};

    helmetfx_lastSent = _chain;
    helmetfx_lastSendTime = diag_tickTime;
    private _r = "helmetfx" callExtension (if (_chain isEqualTo "") then {"clear"} else {"chain:" + _chain});
    if (_chain isNotEqualTo helmetfx_lastLogged) then {
        helmetfx_lastLogged = _chain;
        diag_log format ["[HelmetFX] source: %1 | chain '%2' | extension replied '%3'", helmetfx_lastSource, _chain, _r];
    };
};

[{ [] call helmetfx_fnc_sync }, 0.5] call CBA_fnc_addPerFrameHandler;       // catches death, settings changes, etc.
["loadout", { [] call helmetfx_fnc_sync }] call CBA_fnc_addPlayerEventHandler;  // instant on helmet swap
["unit",    { [] call helmetfx_fnc_sync }] call CBA_fnc_addPlayerEventHandler;  // respawn / Zeus remote control