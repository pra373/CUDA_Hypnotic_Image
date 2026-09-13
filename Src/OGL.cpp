#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../Include/OGL.h"

#include <gl/glew.h>
#include <gl/GL.h>
#include "../Include/vmath.h"
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include "../Include/kernel.cuh"

using namespace vmath;

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#define TEXTURE_WIDTH 1024.0f
#define TEXTURE_HEIGHT 1024.0f

#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "cudart.lib")

enum
{
    AMC_ATTRIBUTE_POSITION = 0,
    AMC_ATTRIBUTE_COLOR,
    AMC_ATTRIBUTE_TEXCOORD
};

// Windows OS Handles & Window State
HWND ghwnd = NULL;
HDC ghdc = NULL;
HGLRC ghrc = NULL;
FILE* gpFile = NULL;
BOOL gbActive = FALSE;
BOOL gbFullScreen = FALSE;
DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev = { sizeof(WINDOWPLACEMENT) };

// OpenGL Shader & Program Objects
GLuint shaderProgramObject = 0;
GLuint mvpMatrixUniform = 0;
GLuint textureSamplerUniform = 0;

// OpenGL Geometry Buffers & Textures
GLuint vao = 0;
GLuint vbo_Position = 0;
GLuint vbo_TexCoord = 0;
GLuint cudaTexture = 0;

// Transformations
mat4 perspectiveProjectionMatrix;

// CUDA Interop Resources
cudaGraphicsResource* resource = nullptr;
cudaSurfaceObject_t surfaceObject = 0;
cudaError_t error;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL createCudaTexture(GLuint* texture);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
    int initialize(void);
    void uninitialize(void);
    void display(void);
    void update(void);

    WNDCLASSEX wndclass;
    HWND hwnd;
    MSG msg;
    TCHAR szAppName[] = TEXT("Prathamesh Window");
    int iResult = 0;
    BOOL bDone = FALSE;

    int top_left_X = GetSystemMetrics(SM_CXSCREEN);
    int top_left_Y = GetSystemMetrics(SM_CYSCREEN);
    int x = (top_left_X / 2) - WIN_WIDTH / 2;
    int y = (top_left_Y / 2) - WIN_HEIGHT / 2;

    if (fopen_s(&gpFile, "../logs/Log.txt", "w") != 0)
    {
        MessageBox(NULL, TEXT("Log File Cannot Be Opened"), TEXT("Error"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    fprintf(gpFile, "Program Started Successfully...\n\n\n");

    wndclass.cbSize = sizeof(WNDCLASSEX);
    wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName = NULL;
    wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

    RegisterClassEx(&wndclass);

    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        szAppName,
        TEXT("Hypnotic Image"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        x,
        y,
        WIN_WIDTH,
        WIN_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ghwnd = hwnd;

    iResult = initialize();
    if (iResult != 0)
    {
        MessageBox(hwnd, TEXT("Initialization Failed"), TEXT("Error"), MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
    }

    ShowWindow(hwnd, iCmdShow);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    while (bDone == FALSE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                bDone = TRUE;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (gbActive == TRUE)
            {
                display();
                update();
            }
        }
    }

    uninitialize();
    return ((int)msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT imsg, WPARAM wParam, LPARAM lParam)
{
    void ToggleFullScreen(void);
    void resize(int, int);

    switch (imsg)
    {
    case WM_SETFOCUS:
        gbActive = TRUE;
        break;

    case WM_KILLFOCUS:
        gbActive = FALSE;
        break;

    case WM_SIZE:
        resize(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_ERASEBKGND:
        return 0;

    case WM_KEYDOWN:
        switch (LOWORD(wParam))
        {
        case VK_ESCAPE:
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_CHAR:
        switch (LOWORD(wParam))
        {
        case 'F':
        case 'f':
            if (gbFullScreen == FALSE)
            {
                ToggleFullScreen();
                gbFullScreen = TRUE;
            }
            else
            {
                ToggleFullScreen();
                gbFullScreen = FALSE;
            }
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        break;
    }

    return DefWindowProc(hwnd, imsg, wParam, lParam);
}

void ToggleFullScreen(void)
{
    MONITORINFO mi = { sizeof(MONITORINFO) };

    if (gbFullScreen == FALSE)
    {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);

        if (dwStyle & WS_OVERLAPPEDWINDOW)
        {
            if (GetWindowPlacement(ghwnd, &wpPrev) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
            {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    }
    else
    {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

int initialize(void)
{
    void printGLInfo(void);
    void uninitialize(void);
    void resize(int, int);
    BOOL loadGLTexture(GLuint*, TCHAR[]);

    PIXELFORMATDESCRIPTOR pfd;
    int iPixelFormatIndex = 0;

    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cBlueBits = 8;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 32;

    ghdc = GetDC(ghwnd);
    if (ghdc == NULL)
    {
        fprintf(gpFile, "GetDC Failed\n");
        return -1;
    }

    iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
    if (iPixelFormatIndex == 0)
    {
        fprintf(gpFile, "ChoosePixelFormat Failed\n");
        return -2;
    }

    if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE)
    {
        fprintf(gpFile, "SetPixelFormat Failed\n");
        return -3;
    }

    ghrc = wglCreateContext(ghdc);
    if (ghrc == NULL)
    {
        fprintf(gpFile, "wglCreateContext Failed\n");
        return -4;
    }

    if (wglMakeCurrent(ghdc, ghrc) == FALSE)
    {
        fprintf(gpFile, "wglMakeCurrent Failed\n");
        return -5;
    }

    if (glewInit() != GLEW_OK)
    {
        fprintf(gpFile, "glewInit() failed, failed to initialize glew !!\n");
        return -6;
    }

    printGLInfo();

    const GLchar* vertexShaderSourceCode =
        "#version 460 core\n"
        "\n"
        "in vec4 aPosition;\n"
        "in vec2 aTexCoord;\n"
        "out vec2 oTexCoord;\n"
        "uniform mat4 uMVPMatrix;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = uMVPMatrix * aPosition;\n"
        "    oTexCoord = aTexCoord;\n"
        "}";

    GLuint vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderObject, 1, (const GLchar**)&vertexShaderSourceCode, NULL);
    glCompileShader(vertexShaderObject);

    GLint status = 0;
    GLint infoLogLength = 0;
    GLchar* szInfoLog = NULL;

    glGetShaderiv(vertexShaderObject, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        glGetShaderiv(vertexShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0)
        {
            szInfoLog = (GLchar*)malloc(infoLogLength);
            if (szInfoLog != NULL)
            {
                glGetShaderInfoLog(vertexShaderObject, infoLogLength, NULL, szInfoLog);
                fprintf(gpFile, "Vertex shader compilation error log : %s\n", szInfoLog);
                free(szInfoLog);
                szInfoLog = NULL;
            }
        }
        uninitialize();
    }

    const GLchar* fragmentShaderSourceCode =
        "#version 460 core\n"
        "\n"
        "in vec2 oTexCoord;\n"
        "uniform sampler2D uTextureSampler;\n"
        "out vec4 FragColor;\n"
        "void main(void)\n"
        "{\n"
        "    FragColor = texture(uTextureSampler, oTexCoord);\n"
        "}";

    GLuint fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderObject, 1, (const GLchar**)&fragmentShaderSourceCode, NULL);
    glCompileShader(fragmentShaderObject);

    status = 0;
    infoLogLength = 0;
    szInfoLog = NULL;

    glGetShaderiv(fragmentShaderObject, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        glGetShaderiv(fragmentShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0)
        {
            szInfoLog = (GLchar*)malloc(infoLogLength);
            if (szInfoLog != NULL)
            {
                glGetShaderInfoLog(fragmentShaderObject, infoLogLength, NULL, szInfoLog);
                fprintf(gpFile, "Fragment shader compilation error log : %s\n", szInfoLog);
                free(szInfoLog);
                szInfoLog = NULL;
            }
        }
        uninitialize();
    }

    shaderProgramObject = glCreateProgram();
    glAttachShader(shaderProgramObject, vertexShaderObject);
    glAttachShader(shaderProgramObject, fragmentShaderObject);

    glBindAttribLocation(shaderProgramObject, AMC_ATTRIBUTE_POSITION, "aPosition");
    glBindAttribLocation(shaderProgramObject, AMC_ATTRIBUTE_TEXCOORD, "aTexCoord");

    glLinkProgram(shaderProgramObject);

    status = 0;
    infoLogLength = 0;
    szInfoLog = NULL;

    glGetProgramiv(shaderProgramObject, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        glGetProgramiv(shaderProgramObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0)
        {
            szInfoLog = (GLchar*)malloc(infoLogLength);
            if (szInfoLog != NULL)
            {
                glGetProgramInfoLog(shaderProgramObject, infoLogLength, NULL, szInfoLog);
                fprintf(gpFile, "Shader program linking error log : %s\n", szInfoLog);
                free(szInfoLog);
                szInfoLog = NULL;
            }
        }
        uninitialize();
    }

    mvpMatrixUniform = glGetUniformLocation(shaderProgramObject, "uMVPMatrix");
    textureSamplerUniform = glGetUniformLocation(shaderProgramObject, "uTextureSampler");

    if (textureSamplerUniform == -1)
    {
        fprintf(gpFile, "Loading of uniform variable failed !! Exiting now\n");
        return -7;
    }

    const GLfloat rectanglePosition[] = {
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f
    };

    const GLfloat rectangleTexCoord[] = {
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo_Position);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_Position);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectanglePosition), rectanglePosition, GL_STATIC_DRAW);
    glVertexAttribPointer(AMC_ATTRIBUTE_POSITION, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(AMC_ATTRIBUTE_POSITION);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &vbo_TexCoord);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_TexCoord);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleTexCoord), rectangleTexCoord, GL_STATIC_DRAW);
    glVertexAttribPointer(AMC_ATTRIBUTE_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(AMC_ATTRIBUTE_TEXCOORD);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    bool result = createCudaTexture(&cudaTexture);
    if (result == false)
    {
        MessageBox(ghwnd, TEXT("Creating texture for OpenGL-CUDA Interop failed !"), TEXT("ERROR"), MB_OK);
        exit(EXIT_FAILURE);
    }

    error = cudaGraphicsGLRegisterImage(&resource, cudaTexture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
    if (error != cudaSuccess)
    {
        MessageBox(ghwnd, TEXT("Failed to register OpenGL texture with CUDA!"), TEXT("CUDA Error"), MB_OK);
        exit(EXIT_FAILURE);
    }

    error = cudaGraphicsMapResources(1, &resource, 0);
    if (error != cudaSuccess)
    {
        MessageBox(ghwnd, TEXT("Failed to map OpenGL texture to CUDA!"), TEXT("CUDA Error"), MB_OK);
        exit(EXIT_FAILURE);
    }

    cudaArray* cudaArray = nullptr;
    error = cudaGraphicsSubResourceGetMappedArray(&cudaArray, resource, 0, 0);
    if (error != cudaSuccess)
    {
        MessageBox(ghwnd, TEXT("Failed to get mapped CUDA array!"), TEXT("CUDA Error"), MB_OK);
        exit(EXIT_FAILURE);
    }

    cudaResourceDesc resourceDesc = {};
    resourceDesc.resType = cudaResourceTypeArray;
    resourceDesc.res.array.array = cudaArray;

    error = cudaCreateSurfaceObject(&surfaceObject, &resourceDesc);
    if (error != cudaSuccess)
    {
        MessageBox(ghwnd, TEXT("Failed to create CUDA surface object!"), TEXT("CUDA Error"), MB_OK);
        exit(EXIT_FAILURE);
    }

    runCudaKernel(surfaceObject, TEXTURE_WIDTH, TEXTURE_HEIGHT);

    error = cudaGraphicsUnmapResources(1, &resource, 0);
    if (error != cudaSuccess)
    {
        MessageBox(ghwnd, TEXT("Failed to unmap OpenGL texture from CUDA!"), TEXT("CUDA Error"), MB_OK);
        exit(EXIT_FAILURE);
    }

    perspectiveProjectionMatrix = vmath::mat4::identity();

    glEnable(GL_TEXTURE_2D);
    resize(WIN_WIDTH, WIN_HEIGHT);

    return 0;
}

void resize(int width, int height)
{
    if (height <= 0)
        height = 1;

    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
}

void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgramObject);

    mat4 modelViewMatrix = vmath::translate(0.0f, 0.0f, -3.5f);
    mat4 modelViewProjectionMatrix = perspectiveProjectionMatrix * modelViewMatrix;

    glUniformMatrix4fv(mvpMatrixUniform, 1, GL_FALSE, modelViewProjectionMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cudaTexture);
    glUniform1i(textureSamplerUniform, 0);

    glBindVertexArray(vao);
    glDrawArrays(GL_QUADS, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    SwapBuffers(ghdc);
}

void update(void)
{
}

void uninitialize(void)
{
    void ToggleFullScreen(void);

    if (shaderProgramObject)
    {
        glUseProgram(shaderProgramObject);

        GLint numShaders = 0;
        glGetProgramiv(shaderProgramObject, GL_ATTACHED_SHADERS, &numShaders);

        if (numShaders > 0)
        {
            GLuint* pShaders = (GLuint*)malloc(numShaders * sizeof(GLuint));
            if (pShaders != NULL)
            {
                glGetAttachedShaders(shaderProgramObject, numShaders, NULL, pShaders);
                for (GLint i = 0; i < numShaders; i++)
                {
                    glDetachShader(shaderProgramObject, pShaders[i]);
                    glDeleteShader(pShaders[i]);
                    pShaders[i] = 0;
                }
                free(pShaders);
                pShaders = NULL;
            }
        }

        glUseProgram(0);
        glDeleteProgram(shaderProgramObject);
        shaderProgramObject = 0;
    }

    if (vbo_TexCoord)
    {
        glDeleteBuffers(1, &vbo_TexCoord);
        vbo_TexCoord = 0;
    }

    if (vbo_Position)
    {
        glDeleteBuffers(1, &vbo_Position);
        vbo_Position = 0;
    }

    if (cudaTexture)
    {
        glDeleteTextures(1, &cudaTexture);
        cudaTexture = 0;
    }

    if (vao)
    {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    if (gbFullScreen == TRUE)
    {
        ToggleFullScreen();
        gbFullScreen = FALSE;
    }

    if (wglGetCurrentContext() == ghrc)
    {
        wglMakeCurrent(NULL, NULL);
    }

    if (ghrc)
    {
        wglDeleteContext(ghrc);
        ghrc = NULL;
    }

    if (ghdc)
    {
        ReleaseDC(ghwnd, ghdc);
        ghdc = NULL;
    }

    if (gpFile)
    {
        fprintf(gpFile, "Program Ended Sucessfully...\n");
        fclose(gpFile);
        gpFile = NULL;
    }

    if (ghwnd)
    {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }
}

void printGLInfo(void)
{
    GLint numExtensions;
    GLint i;

    fprintf(gpFile, "OpenGL Vendor   : %s\n\n", glGetString(GL_VENDOR));
    fprintf(gpFile, "OpenGL Renderer : %s\n\n", glGetString(GL_RENDERER));
    fprintf(gpFile, "OpenGL Version  : %s\n\n", glGetString(GL_VERSION));
    fprintf(gpFile, "GLSL Version    : %s\n\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (i = 0; i < numExtensions; i++)
    {
        fprintf(gpFile, "%s\n", glGetStringi(GL_EXTENSIONS, i));
    }
}

BOOL createCudaTexture(GLuint* texture)
{
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        TEXTURE_WIDTH,
        TEXTURE_HEIGHT,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL
    );

    glBindTexture(GL_TEXTURE_2D, 0);
    return TRUE;
}