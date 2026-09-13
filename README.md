# Procedural Hypnotic Pattern (CUDA-OpenGL Interop)

A real-time procedural pattern generator built using modern OpenGL (Core Profile) and CUDA interoperability on Windows (Win32).

---

## Overview

This project implements the dynamic visual pattern example inspired by the classic book **"CUDA by Example: An Introduction to General-Purpose GPU Programming"**.

Instead of generating pixels on the GPU, copying them back to host memory (RAM), and then sending them back to OpenGL for display, this implementation uses a **zero-copy pipeline**. Both the OpenGL rendering pipeline and the CUDA compute kernel operate directly on the exact same texture memory inside VRAM.

---

## How It Works

* **Shared Resource:** An OpenGL 2D texture is registered with the CUDA runtime via `cudaGraphicsGLRegisterImage`.
* **Zero-Copy Surface Writes:** The texture is mapped to a CUDA array, bound to a `cudaSurfaceObject_t`, and written to directly from the CUDA kernel using `surf2Dwrite`.
* **Rendering:** Once the CUDA kernel finishes generating the procedural mathematical patterns, the resource is unmapped, and OpenGL binds the texture to draw directly to a textured quad in real time.

---

## Tech Stack

* **Language:** C++ / CUDA C
* **Graphics API:** Modern OpenGL 4.6 (Core Profile), GLEW
* **Compute API:** NVIDIA CUDA Runtime & Interop APIs
* **Platform:** Win32 API

---