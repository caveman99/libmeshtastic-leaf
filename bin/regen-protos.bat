@ECHO OFF
SETLOCAL

cd protobufs
..\nanopb-0.4.9.1\generator-bin\protoc.exe --experimental_allow_proto3_optional "--nanopb_out=-S.cpp -v:..\src\generated" -I=..\protobufs\ ..\protobufs\meshtastic\*.proto
GOTO eof

:eof
ENDLOCAL
EXIT /B %ERRORLEVEL%
