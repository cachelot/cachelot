@ECHO OFF

REM ===========================================================================
REM Detect Python
REM ===========================================================================
python --version > NUL 2>&1
IF ERRORLEVEL 1 (
	ECHO Python not installed. Abort.
	ECHO.
	EXIT /B 99
)


SET MYDIR=%~dp0

SET "BUILD_CFGS=Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer UBSanitizer"

FOR %%c IN (%BUILD_CFGS%) DO (
    CALL :run_tests %%c
	IF NOT ERRORLEVEL 0 (
		EXIT /B %ERRORLEVEL%
	)
)

ECHO.
ECHO *** All tests completed successfully

EXIT /B 0


REM ===========================================================================
REM Functions declaration
REM ===========================================================================

:server_test
SET buildCfg=%~1
START %MYDIR%bin/%buildCfg%/cachelotd
SET "PID="
FOR /F "tokens=2" %%A IN ('"TASKLIST /NH /FI "IMAGENAME eq cachelotd*" | FINDSTR /i cachelotd"') DO SET pid=%%A
REM ensure listen socket is up
TIMEOUT /T 1 /NOBREAK > NUL 2>&1
python %MYDIR%test/server_test.py
SET ret=%ERRORLEVEL%
TASKKILL /PID %pid% > NUL 2>&1
REM ensure process is down
TIMEOUT /T 1 /NOBREAK > NUL 2>&1
EXIT /B %ret%


:run_tests
SET buildCfg=%~1
ECHO *** [%buildCfg%]
SET bindir=%MYDIR%bin/%buildCfg%
IF NOT EXIST "%bindir%" (
	ECHO *** [- skipped -]
	EXIT /B 0
)
REM Run basic smoke tests
CALL :server_test %buildCfg%
IF NOT ERRORLEVEL 0 (
	EXIT /B ERRORLEVEL
)

REM Run other tests/benchmarks depending on build type
2>NUL CALL :CASE_%buildCfg%
IF ERRORLEVEL 1 CALL :DEFAULT_CASE
:CASE_Debug
	%bindir%/unit_tests || EXIT /B 1
	%bindir%/test_c_api || EXIT /B 1
	EXIT /B 0
:CASE_Release
	%bindir%/unit_tests || EXIT /B 1
	%bindir%/test_c_api || EXIT /B 1
	%bindir%/benchmark_cache || EXIT /B 1
	%bindir%/benchmark_memalloc || EXIT /B 1
	EXIT /B 0
:CASE_RelWithDebugInfo
	%bindir%/unit_tests || EXIT /B 1
	%bindir%/test_c_api || EXIT /B 1
	%bindir%/benchmark_cache || EXIT /B 1
	%bindir%/benchmark_memalloc || EXIT /B 1
	EXIT /B 0
:CASE_MinSizeRel
	%bindir%/unit_tests || EXIT /B 1
	%bindir%/test_c_api || EXIT /B 1
	%bindir%/benchmark_cache || EXIT /B 1
	%bindir%/benchmark_memalloc || EXIT /B 1
	EXIT /B 0
:CASE_AddressSanitizer
	%bindir%/unit_tests || EXIT /B 1
	%bindir%/test_c_api || EXIT /B 1
	%bindir%/benchmark_cache || EXIT /B 1
	EXIT /B 0
:CASE_UBSanitizer
	%bindir%/unit_tests || exit 1
	%bindir%/test_c_api || exit 1
	%bindir%/benchmark_cache || exit 1
	EXIT /B 0
:DEFAULT_CASE
	ECHO Unknown build configuration
	EXIT /B 1

EXIT /B 0
