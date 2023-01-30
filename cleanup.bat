@ECHO OFF

REM ===========================================================================
REM Check MSVC environment
REM ===========================================================================
IF [%VCToolsVersion%] == [] (
	ECHO ================================================================================
	ECHO Visual Studio env not detected. Abort.
	ECHO ================================================================================
	ECHO.
	EXIT /B 99
) ELSE (
	ECHO ================================================================================
	ECHO Visual Studio CMD ver: %VSCMD_VER%
	ECHO Visual Studio Tools ver: %VCToolsVersion%
	ECHO ================================================================================
	ECHO.
)

SET ROOT=%~dp0
SET ARG=%~1

SET MATCH=""
IF [%ARG%]==[] SET MATCH=Y
IF [%ARG%]==[clean_artifacts] SET MATCH=Y
IF [%MATCH%]==[Y] (
	CALL :clean_artifacts
)

SET MATCH=""
IF [%ARG%]==[] SET MATCH=Y
IF [%ARG%]==[clean_build_files] SET MATCH=Y
IF [%MATCH%]==[Y] (
	CALL :clean_build_files
)
ECHO clean

EXIT /B 0


REM ===========================================================================
REM Functions declaration
REM ===========================================================================

:clean_artifacts
RMDIR /S /Q "%ROOT%bin/*" 2>NUL
RMDIR /S /Q "%ROOT%lib/*" 2>NUL
RMDIR /S /Q "%ROOT%doc/Doxygen/*" 2>NUL
EXIT /B 0

:clean_build_files
FOR /F "Delims=" %%A In ('"DIR /B /S /AD-L "%ROOT%" | FINDSTR /E "CMakeFiles CMakeScripts .build .xcodeproj .dir .tlog""') DO IF EXIST "%%~A" RMDIR /S /Q "%%~A"
FOR /F "Delims=" %%A In ('"DIR /B /S /A-D "%ROOT%" | FINDSTR /E "CMakeCache.txt cmake_install.cmake .pyc .xctestrun .sln .vcxproj .vcxproj.filters .recipe""') DO IF EXIST "%%~A" DEL /F "%%~A"
IF EXIST "Makefile" DEL /F Makefile
FOR /F "Delims=" %%A In ('"DIR /B /S /A-D "%ROOT%src" | FINDSTR /E "Makefile""') DO IF EXIST "%%~A" DEL /F "%%~A"
EXIT /B 0
