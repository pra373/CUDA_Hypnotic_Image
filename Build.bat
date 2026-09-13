cls

cl.exe /c /EHsc /Fo"Build/" /I "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5\include" /I "C:\glew-2.1.0-win32\glew-2.1.0\include" Src/OGL.cpp

nvcc.exe -c -o Build/kernel.obj Src/kernel.cu

rc.exe Resources/OGL.rc 			   

link.exe Build/OGL.obj Build/kernel.obj Build/OGL.res user32.lib gdi32.lib /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5\lib\x64" /LIBPATH:"C:\glew-2.1.0-win32\glew-2.1.0\lib\Release\x64" /SUBSYSTEM:WINDOWS /OUT:Build\OGL.exe

