// XEH_postInit.sqf: clients only.
if (!hasInterface) exitWith {};

helmetfx_lastSent = "<none>";
helmetfx_lastSendTime = -100;
helmetfx_lastLogged = "<none>";

helmetfx_fnc_desiredChain = {
private _enabled = missionNamespace getVariable ["helmetfx_enabled", false];
private _isPlayerNull = isNull player;
private _lifeState = if (_isPlayerNull) then {"NULL"} else {lifeState player};
private _helmet = if (_isPlayerNull) then {""} else {headgear player};
private _mappings = missionNamespace getVariable ["helmetfx_helmetChains", ""];

diag_log format [
    "[HelmetFX DEBUG] enabled=%1 | playerNull=%2 | lifeState=%3 | helmet='%4' | mappingsLength=%5",
    _enabled,
    _isPlayerNull,
    _lifeState,
    _helmet,
    count _mappings
];

if (!_enabled) exitWith {""};
if (_isPlayerNull || {!(_lifeState in ["HEALTHY", "INJURED"])}) exitWith {""};
if (_helmet isEqualTo "") exitWith {""};
if (_mappings isEqualTo "") exitWith {""};

private _result = "";
{
    private _entry = _x;
    private _separator = _entry find "=";
    if (_separator > 0) then {
        private _class = _entry select [0, _separator];
        if (_class isEqualTo _helmet) exitWith {
            _result = _entry select [_separator + 1];
        };
    };
} forEach (_mappings splitString ";");

diag_log format ["[HelmetFX DEBUG] matchedChain=%1", _result isNotEqualTo ""];
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
    if (!(_chain isEqualTo helmetfx_lastLogged)) then {   // log changes only, not heartbeats
        helmetfx_lastLogged = _chain;
        diag_log format ["[HelmetFX] chain '%1' -> extension replied '%2'", _chain, _r];
    };
};

[{ [] call helmetfx_fnc_sync }, 0.5] call CBA_fnc_addPerFrameHandler;       // catches death, settings changes, etc.
["loadout", { [] call helmetfx_fnc_sync }] call CBA_fnc_addPlayerEventHandler;  // instant on helmet swap
["unit",    { [] call helmetfx_fnc_sync }] call CBA_fnc_addPlayerEventHandler;  // respawn / Zeus remote control
