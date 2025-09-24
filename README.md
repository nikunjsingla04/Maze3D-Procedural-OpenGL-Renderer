# Maze3D — Real-time OpenGL Renderer

**Short description**  
Maze3D is a C++ OpenGL application (GLFW + GLAD + GLM) that procedurally generates an N×N maze and renders it in first-person. Features include indexed mesh rendering (VAO/VBO/EBO), texture mapping, GLSL vertex/fragment shaders, AABB collision detection, an orthographic minimap, and simple performance profiling.

---

## Repo contents (important files)
- `src/main.cpp` — main source file (procedural maze, mesh build, rendering, input, collision).  
- `assets/wall.jpg`, `assets/floor.jpg` — textures used at runtime.  
- `screenshots/` — sample images demonstrating the app.  

---

## Features
- Procedural maze generation (DFS backtracker).  
- Efficient indexed mesh (VAO / VBO / EBO) with texture coordinates.  
- GLSL shaders for textured walls and floor; exit is highlighted.  
- First-person camera with mouse-look and WASD controls.  
- Per-wall AABB collision preventing walking through walls.  
- Minimap rendered in an orthographic viewport.  
- FPS measurement via `glfwGetTime()`.

---

## Build & Run (Windows — Visual Studio)
1. Install prerequisites: Visual Studio 2019/2022 (Desktop C++), GLFW, GLAD (or include GLAD source).  
2. Open `MazeFinal.sln` (if included) and build, or use `CMake` (recommended).  
3. Ensure `assets/` is in working directory so `wall.jpg` and `floor.jpg` can be loaded.  
4. Run the executable. Controls: `WASD` move, mouse look, `SPACE` regenerate, `ESC` exit.

---
