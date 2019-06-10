@ECHO OFF && SETLOCAL ENABLEDELAYEDEXPANSION

SET STARTED=%time%
GOTO START
################################################################
#                                                              #
#  Author: AndWalu                                             #
#  Date: 2018-09-12                                            #
#                                                              #
################################################################

:START
REM cmake generator. 
REM Use "cmake -G" to list generators and chose this installed on your system. 
SET GENERATOR=Visual Studio 16 2019

REM Microsoft Visual Studio IDE directories 
REM SET MSVSIDE=c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE
SET MSVSIDE=c:\Program Files\Microsoft Visual Studio\2019\Community\Common7\IDE

REM Path to git command if any. Comment this line if you do not have git installed.
SET GITPATH=%MSVSIDE%\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\mingw32\bin

REM ###################################################################################################################

REM Add the git and cmake directories to PATH environment variable
PATH=%MSVSIDE%\CommonExtensions\Microsoft\CMake\CMake\bin;%MSVSIDE%\..\..\MSBuild\Current\Bin;%GITPATH%;%PATH%;

ECHO.
CALL :MK_LIB_DIRS Win32
CALL :MK_LIB_DIRS x64

cd lib

REM goto LINK

:AES
ECHO.
IF EXIST tiny-AES-c (
    ECHO Updating tiny-AES-c
	git  -C tiny-AES-c pull
) ELSE (
    ECHO Downloading tiny-AES-c
	git  clone https://github.com/kokke/tiny-AES-c.git
)
IF %ERRORLEVEL% NEQ 0 GOTO AESERROR

:ZLIB
ECHO.
IF EXIST zlib (
    ECHO Updating zlib
	git  -C zlib pull
) ELSE (
    ECHO Downloading zlib
	git  clone https://github.com/madler/zlib.git
)
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

IF NOT EXIST zlib\build ( md zlib\build )
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

ECHO.
ECHO Building zlib x32
pushd zlib\build
cmake -G"%GENERATOR%" -A win32 -DCMAKE_INSTALL_PREFIX="..\..\win32" ..

for /F "tokens=1,2 delims=<>	 " %%a IN ( zlibstatic.vcxproj ) DO (
	IF "WindowsTargetPlatformVersion"=="%%a" SET  WinTargetPVer=%%b
)
IF DEFINED WinTargetPVer SET WinTargetPVer=/p:WindowsTargetPlatformVersion=%WinTargetPVer%

xcopy zconf.h ..\..\Win32\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy ..\zlib.h ..\..\Win32\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Debug zlibstatic.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy Debug ..\..\Win32\lib\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Release zlibstatic.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy Release ..\..\Win32\lib\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
popd

IF NOT EXIST zlib\build_x64 ( md zlib\build_x64 )

ECHO.
ECHO Building zlib x64
pushd zlib\build_x64
cmake  -G"%GENERATOR%" -A x64 -DCMAKE_INSTALL_PREFIX="..\..\lib\x64" ..

xcopy zconf.h ..\..\x64\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy ..\zlib.h ..\..\x64\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
MSBuild.exe  /p:Configuration=Debug zlibstatic.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy Debug ..\..\x64\lib\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Release zlibstatic.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy Release ..\..\x64\lib\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
popd


:LIBZIP
ECHO.
IF EXIST libzip (
    ECHO Updating libzip
	git  -C libzip pull
) ELSE (
    ECHO Downloading libzip
	git  clone https://github.com/nih-at/libzip.git
)
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR
git -C libzip apply ..\..\libzip.diff

ECHO.
ECHO Building libzip x32
IF NOT EXIST libzip\build ( md libzip\build )
pushd libzip\build

cmake -G"%GENERATOR%" -A win32 -DCMAKE_PREFIX_PATH="..\Win32" -DZLIB_LIBRARY="zlibstatic.lib;zlibstaticd.lib" -DENABLE_GNUTLS="OFF" -DENABLE_OPENSSL="OFF" -DENABLE_COMMONCRYPTO="OFF" -DENABLE_BZIP2="OFF" -DBUILD_REGRESS="OFF" -DBUILD_EXAMPLES="OFF" -DBUILD_DOC="OFF" -DBUILD_SHARED_LIBS="OFF" ..
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR

xcopy zipconf.h  ..\..\Win32\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy ..\lib\zip.h ..\..\Win32\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Debug lib\zip.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR
copy lib\Debug\zip.lib ..\..\win32\lib\zipstaticd.lib  /V /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Release lib\zip.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR
copy lib\Release\zip.lib ..\..\win32\lib\zipstatic.lib  /V /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

popd

IF NOT EXIST libzip\build_x64 ( md libzip\build_x64 )

ECHO.
ECHO Building libzip x64
pushd libzip\build_x64

cmake -G"%GENERATOR%" -A x64 -DCMAKE_PREFIX_PATH="..\x64" -DZLIB_LIBRARY="zlibstatic.lib;zlibstaticd.lib" -DENABLE_GNUTLS="OFF" -DENABLE_OPENSSL="OFF" -DENABLE_COMMONCRYPTO="OFF" -DENABLE_BZIP2="OFF" -DBUILD_REGRESS="OFF" -DBUILD_EXAMPLES="OFF" -DBUILD_DOC="OFF" -DBUILD_SHARED_LIBS="OFF" ..
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR

xcopy zipconf.h  ..\..\x64\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR
xcopy ..\lib\zip.h ..\..\x64\include\ /S /V /F /R /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Debug lib\zip.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR
copy lib\Debug\zip.lib ..\..\x64\lib\zipstaticd.lib  /V /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

MSBuild.exe  /p:Configuration=Release lib\zip.vcxproj
IF %ERRORLEVEL% NEQ 0 GOTO LIBZIPERROR
copy lib\Release\zip.lib ..\..\x64\lib\zipstatic.lib  /V /Y
IF %ERRORLEVEL% NEQ 0 GOTO ZLIBERROR

popd

:LINK
echo.
CALL :MK_LINK Win32
CALL :MK_LINK x64

ECHO.
ECHO Building dV3wConverter
:DV3WCONVERTER
cd ..\
MSBuild.exe  /p:Configuration=Debug   /p:Platform=x64   %WinTargetPVer% dV3wConverter.sln
IF %ERRORLEVEL% NEQ 0 GOTO DV3WERROR
MSBuild.exe  /p:Configuration=Debug   /p:Platform=Win32 %WinTargetPVer% dV3wConverter.sln
IF %ERRORLEVEL% NEQ 0 GOTO DV3WERROR
MSBuild.exe  /p:Configuration=Release /p:Platform=x64   %WinTargetPVer% dV3wConverter.sln
IF %ERRORLEVEL% NEQ 0 GOTO DV3WERROR
MSBuild.exe  /p:Configuration=Release /p:Platform=Win32 %WinTargetPVer% dV3wConverter.sln
IF %ERRORLEVEL% NEQ 0 GOTO DV3WERROR

ECHO.
ECHO All Ok. Thanks.
GOTO END

REM ###################################################################################################################
REM Error messages.

:DIRERROR
	ECHO Can't create library output directory.
	ECHO Sorry!
GOTO END

:AESERROR
	ECHO.
	ECHO Something go wrong when we try to get tiny-AES-c to work.
	ECHO https://github.com/kokke/tiny-AES-c.git
	ECHO Sorry!
GOTO END

:ZLIBERROR
	ECHO.
	ECHO Something go wrong when we try to get zlib to work.
	ECHO https://github.com/madler/zlib.git
	ECHO Sorry!
GOTO END

:LIBZIPERROR
	ECHO.
	ECHO Something go wrong when we try to get libzip to work.
	ECHO https://github.com/nih-at/libzip.git
	ECHO Sorry!
GOTO END

:LINKERROR
	ECHO Can't create link to the library file.
	ECHO Sorry!
GOTO END

:DV3WERROR
	ECHO Can't  build the dV3wConverter project.
	ECHO Sorry!
GOTO END

REM ###################################################################################################################
REM Functions
REM ###################################################################################################################

:MK_LIB_DIRS
	IF [%~1] == []  GOTO DIRERROR
	SET DIRS=lib\%~1\lib;lib\%~1\include;lib\%~1\Debug;lib\%~1\Release
	
	SET TK=1
	:LOOP_MK_LIB_DIRS
	FOR /f "tokens=%TK%* delims=; eol=," %%a IN ("%DIRS%") DO (
	   CALL :MK_DIR %%a
	   SET N=%%b
	   SET /A TK +=1
	)
	IF DEFINED N GOTO :LOOP_MK_LIB_DIRS

EXIT /B 0

REM ###################################################################################################################

:MK_DIR
	IF [%~1] == []  GOTO DIRERROR
	IF NOT EXIST %~1 ( 
		ECHO Creating directory: %~1
		md %~1 
	)
    IF %ERRORLEVEL% NEQ 0 GOTO DIRERROR
EXIT /B 0

REM ###################################################################################################################

:MK_LINK
	IF [%~1] == []  GOTO LINKERROR
	FOR %%F IN ( .\%~1\lib\*.lib ) DO (
		SET DBG=0
		SET STD=0
		SET FNAME=%%~nF
		SET SLIB=!FNAME:~-6!
		SET DLIB=!FNAME:~-7!
		IF "!SLIB!"=="static"  SET STD=1
		IF "!DLIB!"=="staticd" SET DBG=1
		
		IF !STD! EQU 1	(
			SET LNAME=!FNAME:static=.lib!
			IF EXIST .\%~1\Release\!LNAME! del /Q /F .\%~1\Release\!LNAME!
			mklink /H .\%~1\Release\!LNAME! %%F
			IF %ERRORLEVEL% NEQ 0 GOTO LINKERROR
		)
		IF !DBG! EQU 1	(
			SET LNAME=!FNAME:staticd=.lib!
			IF EXIST .\%~1\Debug\!LNAME!   del /Q /F  .\%~1\Debug\!LNAME!
			mklink /H  .\%~1\Debug\!LNAME! %%F
			IF %ERRORLEVEL% NEQ 0 GOTO LINKERROR
		)
	)
	IF %ERRORLEVEL% NEQ 0 GOTO LINKERROR
EXIT /B 0

REM ###################################################################################################################

:ELPASED

	IF "%~1" == "" EXIT /B 1
	REM Leading zero and decimal part restore
	SET EL_STARTED=%~1
	IF NOT "%~2" == "" SET EL_STARTED=%EL_STARTED%,%~2
	SET EL_STARTED=0%EL_STARTED: =0%
	SET EL_STARTED=%EL_STARTED:~-11%

	SET EL_ENDED=%time: =0%
	
	FOR /f "tokens=1-4 delims=:," %%a IN ("%EL_STARTED%") DO (
		SET EL_ST_H=%%a
		SET EL_ST_M=%%b
		SET EL_ST_S=%%c
		SET EL_ST_D=%%d
	)
	REM Removing leading zero because it make number ambiguous (0xx - is an octal sign)
	if %EL_ST_H:~0,1% EQU 0 SET EL_ST_H=%EL_ST_H:~1,1%
	if %EL_ST_M:~0,1% EQU 0 SET EL_ST_M=%EL_ST_M:~1,1%
	if %EL_ST_S:~0,1% EQU 0 SET EL_ST_S=%EL_ST_S:~1,1%
	if %EL_ST_D:~0,1% EQU 0 SET EL_ST_D=%EL_ST_D:~1,1%

	FOR /f "tokens=1-4 delims=:," %%a IN ("%EL_ENDED%") DO (
		SET EL_EN_H=%%a
		SET EL_EN_M=%%b
		SET EL_EN_S=%%c
		SET EL_EN_D=%%d
	)
	REM Removing leading zero because it make number ambiguous (0xx - zero is an octal sign)
	if %EL_EN_H:~0,1% EQU 0 SET EL_EN_H=%EL_EN_H:~1,1%
	if %EL_EN_M:~0,1% EQU 0 SET EL_EN_M=%EL_EN_M:~1,1%
	if %EL_EN_S:~0,1% EQU 0 SET EL_EN_S=%EL_EN_S:~1,1%
	if %EL_EN_D:~0,1% EQU 0 SET EL_EN_D=%EL_EN_D:~1,1%

	IF %EL_ST_D% GTR %EL_EN_D% (
		SET /A EL_ST_S+=1
		SET /A EL_EN_D+=100
	)
	SET /A EL_D=%EL_EN_D%-%EL_ST_D%

	IF %EL_ST_S% GTR %EL_EN_S% (
		SET /A EL_ST_M+=1
		SET /A EL_EN_S+=60
	)
	SET /A EL_S=%EL_EN_S%-%EL_ST_S%

	IF %EL_ST_M% GTR %EL_EN_M% (
		SET /A EL_ST_H+=1
		SET /A EL_EN_M+=60
	)
	SET /A EL_M=%EL_EN_M%-%EL_ST_M%

	IF %EL_ST_H% GTR %EL_EN_H% (
		SET /A EL_EN_H+=24
	)
	SET /A EL_H=%EL_EN_H%-%EL_ST_H%
	
	REM Leading zero restore
	SET EL_H=0%EL_H%
	SET EL_M=0%EL_M%
	SET EL_S=0%EL_S%
	SET EL_D=0%EL_D%
	
	ECHO.
	ECHO.
	ECHO Start time:   %EL_STARTED%
	ECHO End time:     %EL_ENDED%
	ECHO Time elpased: %EL_H:~-2%:%EL_M:~-2%:%EL_S:~-2%,%EL_D:~-2%
	
EXIT /B 0

REM ###################################################################################################################
REM ###################################################################################################################

:END

CALL :ELPASED %STARTED%

popd
ENDLOCAL

REM ###################################################################################################################
