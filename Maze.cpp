
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <stack>
#include <random>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Constants
constexpr unsigned int SCR_WIDTH = 1600;
constexpr unsigned int SCR_HEIGHT = 1200;
constexpr int N = 10;

// Flatten 2D to 1D
inline int idx(int x, int y) { return y * N + x; }

// Maze cell
struct Cell {
    bool walls[4] = { true, true, true, true }; // L, B, R, T
    bool visited = false;
};
static std::vector<Cell> maze;

// Camera
static glm::vec3 cameraPos, cameraFront, cameraUp{ 0,1,0 };
static float yaw = -90, pitch = 0;
static float lastX = SCR_WIDTH / 2, lastY = SCR_HEIGHT / 2;
static bool firstMouse = true;
static float deltaTime = 0, lastFrame = 0;

// Mesh & minimap buffers
static unsigned int VAO = 0, VBO = 0, EBO = 0;
static std::vector<float> verts;
static std::vector<unsigned int> inds;

static unsigned int miniVAO = 0, miniVBO = 0;
static std::vector<float> miniVerts;

// Textures & shaders
unsigned int wallTex = 0, floorTex = 0;
unsigned int shader3D = 0, shaderLine = 0;

// Prototypes
void generateMaze(int sx, int sy);
void buildMesh();
void buildMinimap();
unsigned int compileShader(GLenum type, const char* src);
unsigned int create3DProgram();
unsigned int createLineProgram();
void mouse_callback(GLFWwindow*, double, double);
void processInput(GLFWwindow*);
bool AABBvsAABB(const glm::vec3& minA, const glm::vec3& maxA,
    const glm::vec3& minB, const glm::vec3& maxB);
bool collidesWithMaze(const glm::vec3& pos);
unsigned int loadTexture(const char* path, bool flip = true);
unsigned int createDefaultTexture();

int main() {
    // Init GLFW/GLAD
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Maze", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create window\n"; return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);




    // Camera start
    cameraPos = { 1.5f,1.0f,1.5f };
    cameraFront = glm::normalize(glm::vec3(1, 0, 0));

    // Maze + mesh
    generateMaze(0, 0);
    buildMesh();
    buildMinimap();


    // Textures
    wallTex = loadTexture("wall.jpg");
    floorTex = loadTexture("floor.jpg");
    if (!wallTex || !floorTex) {
        std::cout << "Using fallback texture\n";
        wallTex = floorTex = createDefaultTexture();
    }

    // Shaders
    shader3D = create3DProgram();
    shaderLine = createLineProgram();

    // Input
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    std::cout << "Controls: WASD, mouse look, SPACE regen, ESC exit\n";

    // Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 3D render
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
            float(SCR_WIDTH) / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model(1.0f);

        glUseProgram(shader3D);
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "projection"), 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "model"), 1, GL_FALSE, &model[0][0]);




        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, wallTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, floorTex);
        glUniform1i(glGetUniformLocation(shader3D, "wallTexture"), 0);
        glUniform1i(glGetUniformLocation(shader3D, "floorTexture"), 1);
        glBindVertexArray(VAO);
        size_t total = inds.size(), exitOff = total - 36;
        glUniform1i(glGetUniformLocation(shader3D, "isExit"), GL_FALSE);
        glDrawElements(GL_TRIANGLES, GLsizei(total - 36), GL_UNSIGNED_INT, nullptr);
        glUniform1i(glGetUniformLocation(shader3D, "isExit"), GL_TRUE);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(exitOff * sizeof(unsigned)));

        // Mini-map
        glDisable(GL_DEPTH_TEST);
        glViewport(10, 10, SCR_WIDTH / 4, SCR_HEIGHT / 4);
        glUseProgram(shaderLine);
        glm::mat4 ortho = glm::ortho(0.0f, float(N), 0.0f, float(N));
        glUniformMatrix4fv(glGetUniformLocation(shaderLine, "uProj"), 1, GL_FALSE, &ortho[0][0]);
        glBindVertexArray(miniVAO);
        glDrawArrays(GL_LINES, 0, GLsizei(miniVerts.size() / 2));
        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

// 1) Maze gen
void generateMaze(int sx, int sy) {
    maze.assign(N * N, Cell());
    std::stack<std::pair<int, int>> st;
    std::mt19937 rng{ std::random_device{}() };
    maze[idx(sx, sy)].visited = true;
    st.push({ sx,sy });
    while (!st.empty()) {
        auto [x, y] = st.top();
        std::vector<int> dirs;
        if (x > 0 && !maze[idx(x - 1, y)].visited) dirs.push_back(0);
        if (y > 0 && !maze[idx(x, y - 1)].visited) dirs.push_back(1);
        if (x < N - 1 && !maze[idx(x + 1, y)].visited) dirs.push_back(2);
        if (y < N - 1 && !maze[idx(x, y + 1)].visited) dirs.push_back(3);

        if (!dirs.empty()) {
            int d = dirs[rng() % dirs.size()];
            int nx = x + (d == 2) - (d == 0), ny = y + (d == 3) - (d == 1);
            maze[idx(x, y)].walls[d] = false;
            maze[idx(nx, ny)].walls[(d + 2) % 4] = false;
            maze[idx(nx, ny)].visited = true;
            st.push({ nx,ny });
        }
        else {
            st.pop();
        }
    }
}

// 2) 3D mesh
void buildMesh() {
    verts.clear(); inds.clear();
    unsigned off = 0;
    auto q = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        glm::vec3 p[4] = { a,b,c,d };
        for (int i = 0;i < 4;i++) {
            verts.insert(verts.end(), {
                p[i].x,p[i].y,p[i].z,
                (i == 0 || i == 3) ? 0.0f : 1.0f,
                (i < 2) ? 0.0f : 1.0f
                });
        }
        inds.insert(inds.end(), { off,off + 1,off + 2,off,off + 2,off + 3 });
        off += 4;
        };

    for (int y = 0;y < N;y++)for (int x = 0;x < N;x++) {
        auto& c = maze[idx(x, y)];
        glm::vec3 base(x, 0, y);
        if (c.walls[0]) q(base, base + glm::vec3(0, 1, 0),
            base + glm::vec3(0, 1, 1), base + glm::vec3(0, 0, 1));
        if (c.walls[1]) q(base, base + glm::vec3(1, 0, 0),
            base + glm::vec3(1, 1, 0), base + glm::vec3(0, 1, 0));
        if (c.walls[2]) q(base + glm::vec3(1, 0, 1), base + glm::vec3(1, 1, 1),
            base + glm::vec3(1, 1, 0), base + glm::vec3(1, 0, 0));
        if (c.walls[3]) q(base + glm::vec3(0, 0, 1), base + glm::vec3(0, 1, 1),
            base + glm::vec3(1, 1, 1), base + glm::vec3(1, 0, 1));
    }

    // floor
    q({ 0,0,0 }, { N,0,0 }, { N,0,N }, { 0,0,N });
    // exit cube
    glm::vec3 e0(N - 1, 0, N - 1), e[8] = { e0,e0 + glm::vec3(1,0,0),e0 + glm::vec3(1,1,0),
                                  e0 + glm::vec3(0,1,0),e0 + glm::vec3(0,0,1),
                                  e0 + glm::vec3(1,0,1),e0 + glm::vec3(1,1,1),
                                  e0 + glm::vec3(0,1,1) };
    unsigned iidx[36] = { 0,1,2,0,2,3,1,5,6,1,6,2,5,4,7,5,7,6,
                       4,0,3,4,3,7,3,2,6,3,6,7,4,5,1,4,1,0 };
    for (unsigned i = 0;i < 36;i++) inds.push_back(off + iidx[i]);

    // upload
    if (!VAO) glGenVertexArrays(1, &VAO);
    if (!VBO) glGenBuffers(1, &VBO);
    if (!EBO) glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// 3) Minimap
void buildMinimap() {
    miniVerts.clear();
    for (int y = 0;y < N;y++)for (int x = 0;x < N;x++) {
        auto& c = maze[idx(x, y)];
        float fx = x, fy = y;
        // Left wall
        if (c.walls[0]) {
            miniVerts.insert(miniVerts.end(), { fx,fy,fx,fy + 1 });
        }
        // Bottom wall
        if (c.walls[1]) {
            miniVerts.insert(miniVerts.end(), { fx,fy,fx + 1,fy });
        }

        // Right
        if (c.walls[2]) {
            miniVerts.insert(miniVerts.end(), { fx + 1,fy,fx + 1,fy + 1 });
        }
        // Top
        if (c.walls[3]) {
            miniVerts.insert(miniVerts.end(), { fx,fy + 1,fx + 1,fy + 1 });
        }
    }
    if (!miniVAO) glGenVertexArrays(1, &miniVAO);
    if (!miniVBO) glGenBuffers(1, &miniVBO);
    glBindVertexArray(miniVAO);
    glBindBuffer(GL_ARRAY_BUFFER, miniVBO);
    glBufferData(GL_ARRAY_BUFFER, miniVerts.size() * sizeof(float), miniVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// 4) Shader helper
unsigned int compileShader(GLenum type, const char* src) {
    unsigned int sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(sh, 512, nullptr, buf);
        std::cerr << "Shader error:\n" << buf << "\n";
    }
    return sh;
}









// 5) 3D shader
unsigned int create3DProgram() {
    const char* vs = R"glsl(
    #version 330 core
    layout(location=0) in vec3 aPos;
    layout(location=1) in vec2 aTexCoord;
    uniform mat4 model,view,projection;
    out vec2 TexCoord; out vec3 WorldPos;
    void main(){
        vec4 worldPos = model * vec4(aPos,1.0);
        WorldPos = worldPos.xyz;
        TexCoord = aTexCoord;
        gl_Position = projection * view * worldPos;
    })glsl";
    const char* fs = R"glsl(
    #version 330 core
    in vec2 TexCoord; in vec3 WorldPos;
    uniform bool isExit;
    uniform sampler2D wallTexture,floorTexture;
    out vec4 FragColor;
    void main(){
        if(isExit){
            FragColor = vec4(1,0,0,1);
        } else if(WorldPos.y < 0.01){
            FragColor = texture(floorTexture, TexCoord * 10.0);
        } else {
            FragColor = texture(wallTexture, TexCoord * vec2(2.0,1.0));
        }
    })glsl";

    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    unsigned int p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// 6) Line shader
unsigned int createLineProgram() {
    const char* vs = R"glsl(
    #version 330 core
    layout(location=0) in vec2 aPos;
    uniform mat4 uProj;
    void main(){
        gl_Position = uProj * vec4(aPos,0,1);
    })glsl";
    const char* fs = R"glsl(
    #version 330 core
    out vec4 FragColor;
    void main(){ FragColor = vec4(1); }
    )glsl";

    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    unsigned int p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}
// 7) Mouse look
void mouse_callback(GLFWwindow* win, double xpos, double ypos) {
    if (firstMouse) {
        lastX = float(xpos);
        lastY = float(ypos);
        firstMouse = false;
    }
    float xoff = float(xpos) - lastX;
    float yoff = lastY - float(ypos);
    lastX = float(xpos);
    lastY = float(ypos);
    const float sens = 0.1f;
    xoff *= sens; yoff *= sens;
    yaw += xoff;
    pitch = glm::clamp(pitch + yoff, -89.0f, 89.0f);
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(dir);
}


// 8) Input + collision + regen
void processInput(GLFWwindow* win) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, true);

    float speed = 2.5f * deltaTime;
    glm::vec3 oldPos = cameraPos;
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));

    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * flatFront;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * flatFront;
    glm::vec3 right = normalize(cross(cameraFront, cameraUp));
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += speed * right;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= speed * right;

    // per-wall collision
    if (collidesWithMaze(cameraPos)) cameraPos = oldPos;

    if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) {
        generateMaze(0, 0);
        buildMesh();
        buildMinimap();
    }
}

// 9) AABB overlap
bool AABBvsAABB(const glm::vec3& minA, const glm::vec3& maxA,
    const glm::vec3& minB, const glm::vec3& maxB) {
    return (minA.x <= maxB.x && maxA.x >= minB.x) &&
        (minA.y <= maxB.y && maxA.y >= minB.y) &&
        (minA.z <= maxB.z && maxA.z >= minB.z);
}

// 10) Per-wall collision
bool collidesWithMaze(const glm::vec3& pos) {
    const float r = 0.2f;
    glm::vec3 minC(pos.x - r, pos.y - 0.5f, pos.z - r);
    glm::vec3 maxC(pos.x + r, pos.y + 1.5f, pos.z + r);

    int cx = int(pos.x), cy = int(pos.z);
    for (int dy = -1;dy <= 1;dy++)for (int dx = -1;dx <= 1;dx++) {
        int x = cx + dx, y = cy + dy;
        if (x < 0 || y < 0 || x >= N || y >= N) continue;
        Cell& c = maze[idx(x, y)];
        glm::vec3 base(x, 0, y);
        // LEFT
        if (c.walls[0]) {
            glm::vec3 minW(base.x, 0, base.z);
            glm::vec3 maxW(base.x + 0.01f, 1, base.z + 1);
            if (AABBvsAABB(minC, maxC, minW, maxW)) return true;
        }
        // BOTTOM
        if (c.walls[1]) {
            glm::vec3 minW(base.x, 0, base.z);
            glm::vec3 maxW(base.x + 1, 1, base.z + 0.01f);
            if (AABBvsAABB(minC, maxC, minW, maxW)) return true;
        }
        // RIGHT
        if (c.walls[2]) {
            glm::vec3 minW(base.x + 1 - 0.01f, 0, base.z);
            glm::vec3 maxW(base.x + 1, 1, base.z + 1);
            if (AABBvsAABB(minC, maxC, minW, maxW)) return true;
        }
        // TOP
        if (c.walls[3]) {
            glm::vec3 minW(base.x, 0, base.z + 1 - 0.01f);
            glm::vec3 maxW(base.x + 1, 1, base.z + 1);
            if (AABBvsAABB(minC, maxC, minW, maxW)) return true;
        }
    }
    return false;
}

// 11) Texture load
unsigned int loadTexture(const char* path, bool flip) {
    stbi_set_flip_vertically_on_load(flip);
    unsigned int tex; glGenTextures(1, &tex);
    int w, h, nc;
    unsigned char* data = stbi_load(path, &w, &h, &nc, 0);
    if (data) {
        GLenum fmt = (nc == 1 ? GL_RED : (nc == 4 ? GL_RGBA : GL_RGB));
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);

    }
    else {
        std::cerr << "Failed to load " << path << "\n";
        return 0;
    }
    return tex;
}

// 12) Fallback texture
unsigned int createDefaultTexture() {
    const int S = 64;
    std::vector<unsigned char> pix(S * S * 3);
    for (int y = 0;y < S;y++)for (int x = 0;x < S;x++) {
        unsigned char c = ((x / 8 + y / 8) & 1) ? 255 : 0;
        int off = (y * S + x) * 3;
        pix[off + 0] = c; pix[off + 1] = c; pix[off + 2] = c;
    }
    unsigned int tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, S, S, 0, GL_RGB, GL_UNSIGNED_BYTE, pix.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}
