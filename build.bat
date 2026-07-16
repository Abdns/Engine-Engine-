@echo off

set VSLANG=1033

if not exist build mkdir build
pushd build

del *.pdb > NUL 2> NUL

REM ---- Shaders (HLSL): каждый <имя>.vert.hlsl/.frag.hlsl -> CompiledShaders\<имя>.vert.spv/.frag.spv ----
if not exist CompiledShaders mkdir CompiledShaders
for %%f in (..\engine\source\Graphics\Vulkan\shaders\*.vert.hlsl) do "%VULKAN_SDK%\Bin\dxc.exe" -spirv -T vs_6_0 -E main "%%f" -Fo "CompiledShaders\%%~nf.spv"
for %%f in (..\engine\source\Graphics\Vulkan\shaders\*.frag.hlsl) do "%VULKAN_SDK%\Bin\dxc.exe" -spirv -T ps_6_0 -E main "%%f" -Fo "CompiledShaders\%%~nf.spv"

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
 -DENGINE_INTERNAL=1^
 -DENGINE_SLOW=1^
 -DENGINE_WIN32=1^
 -DVK_USE_PLATFORM_WIN32_KHR^
 -DUNICODE^
 -D_UNICODE^
 -FC^
 -Z7^
 -I..\engine\source^
 -I..\engine\source\Helpers^
 -I..\engine\source\Assets^
 -I..\engine\source\Game\include^
 -I..\engine\source\Game\src^
 -I..\engine\source\PlatformApi^
 -I..\engine\source\Platform\win32\include^
 -I..\engine\source\Platform\win32\src^
 -I..\engine\source\Graphics\Vulkan^
 -I"%VULKAN_SDK%\Include"

set CommonLinkerFlags=-incremental:no^
 -opt:ref^
 -LIBPATH:"%VULKAN_SDK%\Lib"

REM ---- Asset builder: пакует ..\engine\assets в build\assets.kbn ----
cl %CommonCompilerFlags%^
 ..\engine\tools\AssetBuilder\AssetBuilder.cpp^
 -FeAssetBuilder.exe^
 /link %CommonLinkerFlags%
.\AssetBuilder.exe

REM ---- Game DLL ----
echo WAITING_FOR_PDB > lock.tmp
cl %CommonCompilerFlags%^
 -LD ..\engine\source\Game\src\Game.cpp^
 -FmGame.map^
 -FeGame.dll^
 /link %CommonLinkerFlags%^
 -PDB:game_%random%.pdb^
 -EXPORT:GameUpdateAndRender^
 -EXPORT:GameGetSoundSamples
del lock.tmp

REM ---- Platform EXE ----
cl %CommonCompilerFlags%^
 ..\engine\source\Platform\win32\Program.cpp^
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
