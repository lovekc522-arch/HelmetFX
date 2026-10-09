@echo off
setlocal
cd /d "%~dp0.."
set "ROOT=%CD%\"

set "SDKROOT="
if exist "%ROOT%ts3_plugin\sdk\include\ts3_functions.h" set "SDKROOT=%ROOT%ts3_plugin\sdk"
if exist "%ROOT%ts3_plugin\sdk\ts3client-pluginsdk-26\include\ts3_functions.h" set "SDKROOT=%ROOT%ts3_plugin\sdk\ts3client-pluginsdk-26"
if not defined SDKROOT (
  echo.
  echo TS3 SDK NOT FOUND. Extract ts3client-pluginsdk-26.zip into ts3_plugin\sdk\
  echo so that ts3_plugin\sdk\include\ts3_functions.h exists.
  goto :end
)

set TOOL=
where cl >nul 2>nul
if not errorlevel 1 set TOOL=msvc
if not defined TOOL (
  where g++ >nul 2>nul
  if not errorlevel 1 set TOOL=mingw
)
if not defined TOOL (
  where zig >nul 2>nul
  if not errorlevel 1 set TOOL=zig
)
if not defined TOOL (
  echo.
  echo NO C++ COMPILER FOUND. Easiest fix:  winget install -e --id zig.zig
  echo then close and reopen this window and run build_all.bat again.
  goto :end
)
echo Using %TOOL%, SDK at %SDKROOT%

if "%TOOL%"=="msvc" (
  set PLUGIN_CMD=cl /nologo /std:c++17 /O2 /EHsc /LD /I"%SDKROOT%\include" /I"%SDKROOT%\src" helmetfx_plugin.cpp /Fe:helmetfx_win64.dll
  set EXT_CMD=cl /nologo /O2 /EHsc /LD helmetfx_ext.cpp /Fe:helmetfx_x64.dll ws2_32.lib
)
if "%TOOL%"=="mingw" (
  set PLUGIN_CMD=g++ -std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ -I"%SDKROOT%\include" -I"%SDKROOT%\src" helmetfx_plugin.cpp -o helmetfx_win64.dll -lws2_32
  set EXT_CMD=g++ -O2 -shared -static -static-libgcc helmetfx_ext.cpp -o helmetfx_x64.dll -lws2_32
)
if "%TOOL%"=="zig" (
  set PLUGIN_CMD=zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -shared -I"%SDKROOT%\include" -I"%SDKROOT%\src" helmetfx_plugin.cpp -o helmetfx_win64.dll -lws2_32
  set EXT_CMD=zig c++ -target x86_64-windows-gnu -O2 -shared helmetfx_ext.cpp -o helmetfx_x64.dll -lws2_32
)

pushd ts3_plugin
if exist helmetfx_win64.dll del helmetfx_win64.dll
%PLUGIN_CMD%
if errorlevel 1 ( popd & echo. & echo PLUGIN BUILD FAILED & goto :end )
if not exist helmetfx_win64.dll ( popd & echo PLUGIN DLL NOT PRODUCED & goto :end )

if exist pkg rmdir /s /q pkg
mkdir pkg\plugins
copy /y helmetfx_win64.dll pkg\plugins\ >nul
copy /y package.ini pkg\ >nul
if exist helmetfx.zip del helmetfx.zip
if exist helmetfx.ts3_plugin del helmetfx.ts3_plugin
tar -a -c -f helmetfx.zip -C pkg package.ini plugins
if errorlevel 1 ( popd & echo. & echo COULD NOT CREATE .ts3_plugin ^(tar missing^). Copy helmetfx_win64.dll to %%APPDATA%%\TS3Client\plugins manually. & goto :end )
move /y helmetfx.zip helmetfx.ts3_plugin >nul
rmdir /s /q pkg
popd

pushd arma_dll
if exist helmetfx_x64.dll del helmetfx_x64.dll
%EXT_CMD%
if errorlevel 1 ( popd & echo. & echo EXTENSION BUILD FAILED & goto :end )
if not exist helmetfx_x64.dll ( popd & echo EXTENSION DLL NOT PRODUCED & goto :end )
popd

echo.
echo DONE.
echo   TS plugin DLL       : ts3_plugin\helmetfx_win64.dll
echo   TS plugin installer : ts3_plugin\helmetfx.ts3_plugin
echo   Arma extension DLL  : arma_dll\helmetfx_x64.dll

:end
if not defined CI pause
