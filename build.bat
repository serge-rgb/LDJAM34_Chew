@echo off

cl /Od /MTd /Zi chew.cc glfw/glfw3dll.lib user32.lib gdi32.lib shell32.lib OpenGL32.lib glew32.lib
