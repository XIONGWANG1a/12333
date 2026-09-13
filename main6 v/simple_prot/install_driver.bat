@echo off
:: install
sc create SimpleProt type= kernel start= demand binPath= "%~dp0bin\SimpleProt.sys"
sc start SimpleProt