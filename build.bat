@echo off

::cl /EHsc /Od /MTd /Zi -I GL -I portaudio\include chew.cc glfw/glfw3dll.lib user32.lib gdi32.lib shell32.lib OpenGL32.lib glew32.lib portaudio_x64.lib

:: RELEASE
cl /EHsc /DRELEASE_CHEW /O2 /MT -I GL -I portaudio\include chew.cc glfw/glfw3.lib user32.lib gdi32.lib shell32.lib OpenGL32.lib glew32.lib portaudio_x64.lib
