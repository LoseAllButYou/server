@echo off
for /r %%i in (*.proto) do (          
    echo %%~ni.proto
    protoc.exe  --cpp_out=.  ./%%~ni.proto
)
pause