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

REM Detect VCPKG installation folder
SET VCPKG=""
IF NOT EXIST %LOCALAPPDATA%/vcpkg/vcpkg.path.txt (
	ECHO vcpkg not installed. Abort.
	ECHO.
	EXIT /B 99
) ELSE (
	SET /P VCPKG=<%LOCALAPPDATA%/vcpkg/vcpkg.path.txt
)


SET "BUILD_CFGS=Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer UBSanitizer"

FOR %%c IN (%BUILD_CFGS%) DO (
	CALL cleanup.bat clean_build_files
	ECHO Building %%c ...
	cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG%/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=%%c .
	cmake --build .
)

EXIT /B 0
