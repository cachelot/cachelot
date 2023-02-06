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

REM ===========================================================================
REM Detect VCPKG installation folder
REM ===========================================================================
SET VCPKG=""
IF NOT EXIST %LOCALAPPDATA%/vcpkg/vcpkg.path.txt (
	ECHO ================================================================================
	ECHO vcpkg not installed. Abort.
	ECHO ================================================================================
	ECHO.
	EXIT /B 99
) ELSE (
	SET /P VCPKG=<%LOCALAPPDATA%/vcpkg/vcpkg.path.txt
)


SET CONF=%~1
IF [%CONF%]==[] (
	ECHO No configuration specified. Abort.
	ECHO.
	EXIT /B 99
)

ECHO Building %CONF% ...
cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG%/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=%CONF% .
cmake --build . --config %CONF%

EXIT /B 0
