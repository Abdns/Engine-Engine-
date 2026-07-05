@echo off

set VSLANG=1033

if not exist build mkdir build
pushd build

del *.pdb > NUL 2> NUL

REM ---- Shaders: каждый <имя>.vert/.frag -> CompiledShaders\<имя>.vert.spv/.frag.spv ----
if not exist CompiledShaders mkdir CompiledShaders
for %%f in (..\source\Graphics\Vulkan\shaders\*.vert) do "%VULKAN_SDK%\Bin\glslc.exe" "%%f" -o "CompiledShaders\%%~nf.vert.spv"
for %%f in (..\source\Graphics\Vulkan\shaders\*.frag) do "%VULKAN_SDK%\Bin\glslc.exe" "%%f" -o "CompiledShaders\%%~nf.frag.spv"

REM ---- Assets: копируем модели/ресурсы рядом с exe (build\assets\models\) ----
if not exist assets\models mkdir assets\models
xcopy /Y /D "..\assets\models\*.*" "assets\models\" > NUL 2> NUL

set CommonCompilerFlags=-MTd^
 -nologo^
 -Gm-^
 -GR-^
 -EHa-^
 -Od^
 -Oi^
 -WX^
 -W4^
 -wd4201^
 -wd4100^
 -wd4189^
 -wd4505^
 -wd4211^
 -DHANDMADE_INTERNAL=1^
 -DHANDMADE_SLOW=1^
 -DHANDMADE_WIN32=1^
 -DVK_USE_PLATFORM_WIN32_KHR^
 -DUNICODE^
 -D_UNICODE^
 -FC^
 -Z7^
 -I..\source^
 -I..\source\helpers^
 -I..\source\Game^
 -I..\source\PlatformApi^
 -I..\source\win32\include^
 -I..\source\win32\src^
 -I..\source\Graphics\Vulkan^
 -I..\source\Graphics\SoftwareRender^
 -I"%VULKAN_SDK%\Include"

set CommonLinkerFlags=-incremental:no^
 -opt:ref^
 -LIBPATH:"%VULKAN_SDK%\Lib"

REM ---- Game DLL ----
echo WAITING_FOR_PDB > lock.tmp
cl %CommonCompilerFlags%^
 -LD ..\source\Game\Game.cpp^
 -FmGame.map^
 -FeGame.dll^
 /link %CommonLinkerFlags%^
 -PDB:game_%random%.pdb^
 -EXPORT:GameUpdateAndRender^
 -EXPORT:GameGetSoundSamples
del lock.tmp

REM ---- Platform EXE ----
cl %CommonCompilerFlags%^
 ..\source\win32\Program.cpp^
 -FmEngine.map^
 -FeEngine.exe^
 /link %CommonLinkerFlags%^
 /SUBSYSTEM:CONSOLE^
 /ENTRY:WinMainCRTStartup^
 user32.lib^
 gdi32.lib^
 winmm.lib^
 dsound.lib^
 dxguid.lib^
 Xinput.lib^
 vulkan-1.lib

popd
