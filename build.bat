@echo off

if not exist build mkdir build
pushd build

del *.pdb > NUL 2> NUL

set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4211 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -DUNICODE -D_UNICODE -FC -Z7 -I..\source\helpers -I..\source\Engine -I..\source\win32\include
set CommonLinkerFlags=-incremental:no -opt:ref

REM ---- Game DLL ----
echo WAITING_FOR_PDB > lock.tmp
cl %CommonCompilerFlags% -LD ..\source\Engine\EngineLayer.cpp -Fmengine_game.map -FeEngine_game.dll /link %CommonLinkerFlags% -PDB:engine_game_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
del lock.tmp

REM ---- Platform EXE ----
cl %CommonCompilerFlags% ..\source\win32\src\Source.cpp -FmEngine.map -FeEngine.exe /link %CommonLinkerFlags% user32.lib gdi32.lib winmm.lib dsound.lib dxguid.lib Xinput.lib

popd
