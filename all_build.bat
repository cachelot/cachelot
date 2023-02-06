@ECHO OFF

SET "BUILD_CFGS=Debug Release RelWithDebugInfo MinSizeRel AddressSanitizer UBSanitizer"

FOR %%c IN (%BUILD_CFGS%) DO (
	CALL cleanup.bat clean_build_files
	CALL build.bat %%c
)

EXIT /B 0
