@echo off
setlocal

rem === EDIT THIS PATH ===
rem Set OUTDIR to your Call of Duty 2 bin directory
set OUTDIR=CHANGE_ME

set SRCDIR=%~dp0
set CODDIR=%SRCDIR%cod2src
set ZLIBDIR=%CODDIR%\libs\zlib
set MZIPDIR=%CODDIR%\libs\minizip
set BUILDDIR=%SRCDIR%build
if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"

rem Auto-detect Visual Studio
for /f "tokens=*" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath') do set VSDIR=%%i
if not defined VSDIR ( echo ERROR: Visual Studio not found. Install VS 2022 or later. && exit /b 1 )

echo === COMPILING (32-bit) ===
cd /d "%BUILDDIR%"

call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat"

cl /nologo /O2 /W0 /c /I"%ZLIBDIR%" /I"%SRCDIR%" ^
    "%ZLIBDIR%\adler32.c" "%ZLIBDIR%\compress.c" "%ZLIBDIR%\crc32.c" ^
    "%ZLIBDIR%\deflate.c" "%ZLIBDIR%\inflate.c" "%ZLIBDIR%\inffast.c" ^
    "%ZLIBDIR%\inftrees.c" "%ZLIBDIR%\trees.c" "%ZLIBDIR%\uncompr.c" ^
    "%ZLIBDIR%\zutil.c" ^
    "%MZIPDIR%\ioapi.c" "%MZIPDIR%\unzip.c"
if errorlevel 1 ( echo === ZLIB 32-BIT BUILD FAILED === && exit /b 1 )

cl /nologo /O2 /arch:IA32 /fp:precise /W0 /DDETERMINISTIC_SORT /d1trimfile:%CODDIR%\ ^
   /I"%ZLIBDIR%" /I"%MZIPDIR%" /I"%CODDIR%" /I"%SRCDIR%" ^
   "%CODDIR%\aabbtree.c" ^
   "%CODDIR%\brush.c" ^
   "%CODDIR%\bsp.c" ^
   "%CODDIR%\collision.c" ^
   "%CODDIR%\facebsp.c" ^
   "%CODDIR%\leakfile.c" ^
   "%CODDIR%\lightmaps.c" ^
   "%CODDIR%\map.c" ^
   "%CODDIR%\materials.c" ^
   "%CODDIR%\mesh.c" ^
   "%CODDIR%\patch.c" ^
   "%CODDIR%\portals.c" ^
   "%CODDIR%\shadows.c" ^
   "%CODDIR%\shadows_midpoint.c" ^
   "%CODDIR%\surface.c" ^
   "%CODDIR%\threads.c" ^
   "%CODDIR%\tree.c" ^
   "%CODDIR%\tris.c" ^
   "%CODDIR%\tris_coalesce.c" ^
   "%CODDIR%\tris_gridtree.c" ^
   "%CODDIR%\tris_mergeconcave.c" ^
   "%CODDIR%\tris_mergeverts.c" ^
   "%CODDIR%\tris_tesselate.c" ^
   "%CODDIR%\tris_tjunc.c" ^
   "%CODDIR%\vis.c" ^
   "%CODDIR%\win_shared.c" ^
   "%CODDIR%\writebsp.c" ^
   "%CODDIR%\common\bspfile.c" ^
   "%CODDIR%\common\polylib.c" ^
   "%CODDIR%\common\targetplatform.c" ^
   "%CODDIR%\common\texturevecs.c" ^
   "%CODDIR%\universal\assertive.c" ^
   "%CODDIR%\universal\com_files.c" ^
   "%CODDIR%\universal\com_math.c" ^
   "%CODDIR%\universal\com_memory.c" ^
   "%CODDIR%\universal\com_shared.c" ^
   "%CODDIR%\universal\dvar.c" ^
   "%CODDIR%\universal\q_parse.c" ^
   "%CODDIR%\universal\q_shared.c" ^
   "%CODDIR%\universal\tangentspace.c" ^
   "%CODDIR%\libs\cmdlib.c" ^
   adler32.obj compress.obj crc32.obj deflate.obj inflate.obj inffast.obj ^
   inftrees.obj trees.obj uncompr.obj zutil.obj ioapi.obj unzip.obj ^
   /Fe"%OUTDIR%\cod2map.exe" ^
   user32.lib gdi32.lib kernel32.lib ^
   /link /SUBSYSTEM:CONSOLE /STACK:8388608 /DYNAMICBASE:NO
if errorlevel 1 ( echo === 32-BIT BUILD FAILED === && exit /b 1 )

echo.
echo === COMPILING (64-bit) ===

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat"

cl /nologo /O2 /W0 /c /I"%ZLIBDIR%" /I"%SRCDIR%" ^
    "%ZLIBDIR%\adler32.c" "%ZLIBDIR%\compress.c" "%ZLIBDIR%\crc32.c" ^
    "%ZLIBDIR%\deflate.c" "%ZLIBDIR%\inflate.c" "%ZLIBDIR%\inffast.c" ^
    "%ZLIBDIR%\inftrees.c" "%ZLIBDIR%\trees.c" "%ZLIBDIR%\uncompr.c" ^
    "%ZLIBDIR%\zutil.c" ^
    "%MZIPDIR%\ioapi.c" "%MZIPDIR%\unzip.c"
if errorlevel 1 ( echo === ZLIB 64-BIT BUILD FAILED === && exit /b 1 )

ml64 /nologo /c "%CODDIR%\asm\x87_trig.asm"
if errorlevel 1 ( echo === MASM x87_trig FAILED === && exit /b 1 )

cl /nologo /O2 /fp:precise /W0 /DDETERMINISTIC_SORT /d1trimfile:%CODDIR%\ ^
   /I"%ZLIBDIR%" /I"%MZIPDIR%" /I"%CODDIR%" /I"%SRCDIR%" ^
   "%CODDIR%\aabbtree.c" ^
   "%CODDIR%\brush.c" ^
   "%CODDIR%\bsp.c" ^
   "%CODDIR%\collision.c" ^
   "%CODDIR%\facebsp.c" ^
   "%CODDIR%\leakfile.c" ^
   "%CODDIR%\lightmaps.c" ^
   "%CODDIR%\map.c" ^
   "%CODDIR%\materials.c" ^
   "%CODDIR%\mesh.c" ^
   "%CODDIR%\patch.c" ^
   "%CODDIR%\portals.c" ^
   "%CODDIR%\shadows.c" ^
   "%CODDIR%\shadows_midpoint.c" ^
   "%CODDIR%\surface.c" ^
   "%CODDIR%\threads.c" ^
   "%CODDIR%\tree.c" ^
   "%CODDIR%\tris.c" ^
   "%CODDIR%\tris_coalesce.c" ^
   "%CODDIR%\tris_gridtree.c" ^
   "%CODDIR%\tris_mergeconcave.c" ^
   "%CODDIR%\tris_mergeverts.c" ^
   "%CODDIR%\tris_tesselate.c" ^
   "%CODDIR%\tris_tjunc.c" ^
   "%CODDIR%\vis.c" ^
   "%CODDIR%\win_shared.c" ^
   "%CODDIR%\writebsp.c" ^
   "%CODDIR%\common\bspfile.c" ^
   "%CODDIR%\common\polylib.c" ^
   "%CODDIR%\common\targetplatform.c" ^
   "%CODDIR%\common\texturevecs.c" ^
   "%CODDIR%\universal\assertive.c" ^
   "%CODDIR%\universal\com_files.c" ^
   "%CODDIR%\universal\com_math.c" ^
   "%CODDIR%\universal\com_memory.c" ^
   "%CODDIR%\universal\com_shared.c" ^
   "%CODDIR%\universal\dvar.c" ^
   "%CODDIR%\universal\q_parse.c" ^
   "%CODDIR%\universal\q_shared.c" ^
   "%CODDIR%\universal\tangentspace.c" ^
   "%CODDIR%\libs\cmdlib.c" ^
   adler32.obj compress.obj crc32.obj deflate.obj inflate.obj inffast.obj ^
   inftrees.obj trees.obj uncompr.obj zutil.obj ioapi.obj unzip.obj ^
   x87_trig.obj ^
   /Fe"%OUTDIR%\cod2map64.exe" ^
   user32.lib gdi32.lib kernel32.lib ^
   /link /SUBSYSTEM:CONSOLE /STACK:8388608
if errorlevel 1 ( echo === 64-BIT BUILD FAILED === && exit /b 1 )

echo.
echo === BUILD COMPLETE ===
echo x86: %OUTDIR%\cod2map.exe
echo x64: %OUTDIR%\cod2map64.exe
