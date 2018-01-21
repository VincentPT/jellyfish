setlocal
@echo off

SET CURR_DIR=%cd%
SET THIS_SCRIPT_PATH=%~dp0
cd %THIS_SCRIPT_PATH%

cmake -G "Visual Studio 14 2015 Win64" ../src ^
        -DCINDER_INCLUDE_DIR=E:/Projects/externals/cinder/include -DCINDER_LIB_DIR=E:/Projects/externals/cinder/lib/vc14/x64 ^
        -DCPPRESTSDK_INCLUDE_DIR=E:/Projects/externals/cpprestsdk.v140.windesktop.msvcstl.dyn.rt-dyn.2.9.1/build/native/include ^
        -DCPPRESTSDK_LIB_DIR=E:/Projects/externals/cpprestsdk.v140.windesktop.msvcstl.dyn.rt-dyn.2.9.1/lib/native/v140/windesktop/msvcstl/dyn/rt-dyn/x64 ^
        -DCPPRESTSDK_BIN_DIR=E:/Projects/externals/cpprestsdk.v140.windesktop.msvcstl.dyn.rt-dyn.2.9.1/lib/native/v140/windesktop/msvcstl/dyn/rt-dyn/x64
cd %CURR_DIR%

endlocal