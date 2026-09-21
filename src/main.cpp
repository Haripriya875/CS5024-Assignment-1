#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------------------------------------------------
// WINDOW / GRID SIZE SETTINGS
// GRID_LINES = 21 gives a 20x20x20 grid of cells.
// GRID_MAX is CALCULATED from GRID_LINES so it can never go out of sync
// with the number of lines actually drawn.
// ---------------------------------------------------------------------
constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 900;

constexpr int GRID_LINES = 21;
constexpr float GRID_MIN = -10.0f;
constexpr float GRID_MAX = GRID_MIN + static_cast<float>(GRID_LINES - 1);
constexpr int CELL_COUNT = GRID_LINES - 1;

// ---------------------------------------------------------------------
// HUD (on-screen legend) SETTINGS
// Each letter is drawn as a 5-wide x 7-tall grid of small squares.
// FONT_PIXEL_SIZE is how big (in screen pixels) each of those squares is.
// ---------------------------------------------------------------------
constexpr int FONT_COLS = 5;
constexpr int FONT_ROWS = 7;
constexpr float FONT_PIXEL_SIZE = 3.0f;
constexpr float CHAR_ADVANCE = static_cast<float>(FONT_COLS + 1) * FONT_PIXEL_SIZE; // space between chars
constexpr float LINE_ADVANCE = static_cast<float>(FONT_ROWS + 2) * FONT_PIXEL_SIZE; // space between lines
constexpr float HUD_MARGIN = 10.0f;
const glm::vec3 HUD_TEXT_COLOR(0.10f, 0.10f, 0.15f);
const glm::vec3 HUD_PANEL_COLOR(0.85f, 0.85f, 0.88f);
constexpr float HUD_PANEL_ALPHA = 0.6f; // 0 = fully see-through, 1 = fully solid
constexpr GLsizei HUD_PANEL_VERTEX_COUNT = 6; // the panel is 1 quad = 2 triangles = 6 vertices

// The legend text shown in the top-left corner of the window.
const std::vector<std::string> HUD_LINES = {
    "CONTROLS",
    "ARROWS : MOVE X/Y",
    "U / B : MOVE Z",
    "C : CHANGE COLOR",
    "F : FILL CELL",
    "W : CLEAR CELL",
    "L R T D : ROTATE",
    "1 2 3 : SHAPES",
    "0 : CLEAR ALL",
    "H : HIDE GRID",
    "ESC : QUIT"
};

// ---------------------------------------------------------------------
// GLOBAL STATE
// ---------------------------------------------------------------------
GLFWwindow* g_window = nullptr;

GLuint g_shaderProgram = 0;
GLint  g_modelLoc = -1;
GLint  g_viewLoc = -1;
GLint  g_projectionLoc = -1;
GLint  g_overrideColorLoc = -1;
GLint  g_useOverrideLoc = -1;

GLuint g_cubeVAO = 0;
GLuint g_gridVAO = 0;
GLsizei g_gridVertexCount = 0;

GLuint g_hudVAO = 0;
GLsizei g_hudVertexCount = 0;

glm::vec3 g_cubePosition(0.0f);
glm::vec3 g_cubeColor(0.9f, 0.2f, 0.2f);
glm::mat4 g_rotation(1.0f);
bool      g_showGrid = true; // toggled with the H key

// Each filled box remembers its OWN color, instead of borrowing whatever
// color the cursor cube currently has.
bool      g_filled[CELL_COUNT][CELL_COUNT][CELL_COUNT] = {};
glm::vec3 g_filledColor[CELL_COUNT][CELL_COUNT][CELL_COUNT] = {};

// The rasterized shapes live in their own layer so toggling a shape never
// touches cells you filled by hand with F.
bool      g_modelFilled[CELL_COUNT][CELL_COUNT][CELL_COUNT] = {};
glm::vec3 g_modelColor[CELL_COUNT][CELL_COUNT][CELL_COUNT] = {};
bool g_showLine = false; // 1 toggles the line
bool g_showRing = false; // 2 toggles the ring
bool g_showBall = false; // 3 toggles the ball

std::array<int, GLFW_KEY_LAST + 1> g_previousKeys = {};

// ---------------------------------------------------------------------
// SMALL MATH / FILE HELPERS
// ---------------------------------------------------------------------

float cellCenter(int index)
{
    return GRID_MIN + static_cast<float>(index) + 0.5f;
}

int positionToCell(float position)
{
    return static_cast<int>(position - GRID_MIN);
}

std::string loadShaderSource(const char* filepath)
{
    std::ifstream file(filepath);
    if (!file)
    {
        std::cerr << "Failed to open shader file: " << filepath << '\n';
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

glm::vec3 readColor()
{
    std::cout << "\nEnter cube RGB values (0-255, separated by spaces): ";
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (!(std::cin >> r >> g >> b))
    {
        std::cin.clear();
        std::string discarded;
        std::getline(std::cin, discarded);
        std::cerr << "Invalid color. Keeping the previous color.\n";
        return g_cubeColor;
    }
    if (std::max({r, g, b}) > 1.0f)
    {
        r /= 255.0f;
        g /= 255.0f;
        b /= 255.0f;
    }
    return glm::clamp(glm::vec3(r, g, b), glm::vec3(0.0f), glm::vec3(1.0f));
}

// ---------------------------------------------------------------------
// 3D MESH BUILDING HELPERS (cube + grid lines)
// ---------------------------------------------------------------------

void addVertex(std::vector<float>& vertices, const glm::vec3& position,
               const glm::vec3& color)
{
    vertices.insert(vertices.end(),
                    {position.x, position.y, position.z,
                     color.r, color.g, color.b});
}

void addCube(std::vector<float>& vertices, const glm::vec3& center,
             const glm::vec3& color)
{
    const std::array<glm::vec3, 8> corners = {
        center + glm::vec3(-0.5f, -0.5f, -0.5f),
        center + glm::vec3( 0.5f, -0.5f, -0.5f),
        center + glm::vec3( 0.5f,  0.5f, -0.5f),
        center + glm::vec3(-0.5f,  0.5f, -0.5f),
        center + glm::vec3(-0.5f, -0.5f,  0.5f),
        center + glm::vec3( 0.5f, -0.5f,  0.5f),
        center + glm::vec3( 0.5f,  0.5f,  0.5f),
        center + glm::vec3(-0.5f,  0.5f,  0.5f)
    };
    constexpr std::array<unsigned, 36> indices = {
        4, 5, 6, 4, 6, 7, 1, 0, 3, 1, 3, 2,
        0, 4, 7, 0, 7, 3, 5, 1, 2, 5, 2, 6,
        3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4
    };
    for (unsigned index : indices)
        addVertex(vertices, corners[index], color);
}

std::vector<float> buildGridVertices()
{
    const glm::vec3 gridColor(0.35f);
    std::vector<float> vertices;

    for (int y = 0; y < GRID_LINES; ++y)
        for (int z = 0; z < GRID_LINES; ++z)
        {
            const float yy = GRID_MIN + y;
            const float zz = GRID_MIN + z;
            addVertex(vertices, {GRID_MIN, yy, zz}, gridColor);
            addVertex(vertices, {GRID_MAX, yy, zz}, gridColor);
        }
    for (int x = 0; x < GRID_LINES; ++x)
        for (int z = 0; z < GRID_LINES; ++z)
        {
            const float xx = GRID_MIN + x;
            const float zz = GRID_MIN + z;
            addVertex(vertices, {xx, GRID_MIN, zz}, gridColor);
            addVertex(vertices, {xx, GRID_MAX, zz}, gridColor);
        }
    for (int x = 0; x < GRID_LINES; ++x)
        for (int y = 0; y < GRID_LINES; ++y)
        {
            const float xx = GRID_MIN + x;
            const float yy = GRID_MIN + y;
            addVertex(vertices, {xx, yy, GRID_MIN}, gridColor);
            addVertex(vertices, {xx, yy, GRID_MAX}, gridColor);
        }
    return vertices;
}

GLuint makeMesh(const std::vector<float>& vertices)
{
    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo); // safe: the VAO keeps the buffer alive until unbound
    return vao;
}

// ---------------------------------------------------------------------
// HUD TEXT HELPERS (on-screen controls legend)
// Each glyph is a 5-column x 7-row grid of dots. One row is stored as
// a 5-bit number: bit 4 = leftmost column ... bit 0 = rightmost column.
// Unknown characters are simply skipped (drawn blank).
// ---------------------------------------------------------------------

// Returns the 7 row-bytes for a glyph, or nullptr if we don't have that
// character defined.
const uint8_t* getGlyphRows(char c)
{
    static const uint8_t A[7] = {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    static const uint8_t B[7] = {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
    static const uint8_t C[7] = {0b01111,0b10000,0b10000,0b10000,0b10000,0b10000,0b01111};
    static const uint8_t D[7] = {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
    static const uint8_t E[7] = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
    static const uint8_t F[7] = {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000};
    static const uint8_t G[7] = {0b01111,0b10000,0b10000,0b10011,0b10001,0b10001,0b01111};
    static const uint8_t H[7] = {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    static const uint8_t I[7] = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111};
    static const uint8_t L[7] = {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
    static const uint8_t M[7] = {0b10001,0b11011,0b10101,0b10001,0b10001,0b10001,0b10001};
    static const uint8_t N[7] = {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
    static const uint8_t O[7] = {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static const uint8_t Q[7] = {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
    static const uint8_t R[7] = {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
    static const uint8_t S[7] = {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110};
    static const uint8_t T[7] = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
    static const uint8_t U[7] = {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    static const uint8_t V[7] = {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100};
    static const uint8_t W[7] = {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001};
    static const uint8_t X[7] = {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001};
    static const uint8_t Y[7] = {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100};
    static const uint8_t Z[7] = {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111};
    static const uint8_t D0[7] = {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110};
    static const uint8_t D1[7] = {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
    static const uint8_t D2[7] = {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111};
    static const uint8_t D3[7] = {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110};
    static const uint8_t COLON[7] = {0b00000,0b00100,0b00000,0b00000,0b00000,0b00100,0b00000};
    static const uint8_t SLASH[7] = {0b00001,0b00010,0b00100,0b00100,0b01000,0b10000,0b00000};
    static const uint8_t SPACE[7] = {0,0,0,0,0,0,0};

    switch (c)
    {
        case 'A': return A;  case 'B': return B;  case 'C': return C;
        case 'D': return D;  case 'E': return E;  case 'F': return F;
        case 'G': return G;  case 'H': return H;  case 'I': return I;
        case 'L': return L;  case 'M': return M;  case 'N': return N;
        case 'O': return O;  case 'Q': return Q;  case 'R': return R;
        case 'S': return S;  case 'T': return T;  case 'U': return U;
        case 'V': return V;  case 'W': return W;  case 'X': return X;
        case 'Y': return Y;  case 'Z': return Z;
        case '0': return D0;  case '1': return D1;
        case '2': return D2;  case '3': return D3;
        case ':': return COLON;
        case '/': return SLASH;
        case ' ': return SPACE;
        default:  return nullptr; // unknown character: draw nothing
    }
}

// Adds one filled rectangle (two triangles) to the vertex list.
void appendQuad(std::vector<float>& vertices, float x0, float y0,
                 float x1, float y1, const glm::vec3& color)
{
    addVertex(vertices, {x0, y0, 0.0f}, color);
    addVertex(vertices, {x1, y0, 0.0f}, color);
    addVertex(vertices, {x1, y1, 0.0f}, color);
    addVertex(vertices, {x0, y0, 0.0f}, color);
    addVertex(vertices, {x1, y1, 0.0f}, color);
    addVertex(vertices, {x0, y1, 0.0f}, color);
}

// Draws one character by turning each "on" dot of its glyph into a quad.
// originX/originY is the pixel position of the glyph's top-left corner.
void appendGlyph(std::vector<float>& vertices, char c, float originX,
                  float originY, const glm::vec3& color)
{
    const uint8_t* rows = getGlyphRows(c);
    if (rows == nullptr)
        return;

    for (int row = 0; row < FONT_ROWS; ++row)
        for (int col = 0; col < FONT_COLS; ++col)
        {
            const int bit = FONT_COLS - 1 - col; // bit 4 = leftmost column
            const bool on = (rows[row] & (1 << bit)) != 0;
            if (!on)
                continue;

            const float x0 = originX + static_cast<float>(col) * FONT_PIXEL_SIZE;
            const float y0 = originY + static_cast<float>(row) * FONT_PIXEL_SIZE;
            appendQuad(vertices, x0, y0, x0 + FONT_PIXEL_SIZE, y0 + FONT_PIXEL_SIZE, color);
        }
}

// Draws a whole line of text, one character at a time, left to right.
void appendText(std::vector<float>& vertices, const std::string& text,
                 float originX, float originY, const glm::vec3& color)
{
    float cursorX = originX;
    for (char c : text)
    {
        appendGlyph(vertices, c, cursorX, originY, color);
        cursorX += CHAR_ADVANCE;
    }
}

// Builds the full HUD mesh: a background panel plus every legend line.
std::vector<float> buildHudVertices()
{
    std::vector<float> vertices;

    const float panelWidth = 20.0f * CHAR_ADVANCE + 2.0f * HUD_MARGIN;
    const float panelHeight = static_cast<float>(HUD_LINES.size()) * LINE_ADVANCE + 2.0f * HUD_MARGIN;
    appendQuad(vertices, 0.0f, 0.0f, panelWidth, panelHeight, HUD_PANEL_COLOR);

    for (std::size_t line = 0; line < HUD_LINES.size(); ++line)
    {
        const float originY = HUD_MARGIN + static_cast<float>(line) * LINE_ADVANCE;
        appendText(vertices, HUD_LINES[line], HUD_MARGIN, originY, HUD_TEXT_COLOR);
    }
    return vertices;
}

// ---------------------------------------------------------------------
// RASTERIZED MODEL: "Ringed Planet"
// Every shape is rasterized by deciding which grid cells to fill.
//   - Ball   : sphere shell, distance test per cell
//   - Circle : midpoint circle algorithm (ring around the ball)
//   - Line   : 3D Bresenham (slanted axis through the planet)
// ---------------------------------------------------------------------

// Fills one cell with a color (ignores cells outside the grid).
void setVoxel(int x, int y, int z, const glm::vec3& c)
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= CELL_COUNT || y >= CELL_COUNT || z >= CELL_COUNT)
        return;
    g_modelFilled[x][y][z] = true;
    g_modelColor[x][y][z] = c;
}

// Hue t (any float, wraps around 0..1) -> rainbow RGB.
glm::vec3 rainbow(float t)
{
    t -= std::floor(t);
    return glm::clamp(glm::vec3(std::abs(t * 6.0f - 3.0f) - 1.0f,
                                2.0f - std::abs(t * 6.0f - 2.0f),
                                2.0f - std::abs(t * 6.0f - 4.0f)),
                      0.0f, 1.0f);
}

// 3D Bresenham line. The hue fades from h0 to h1 along the line.
void drawLine3D(glm::ivec3 a, glm::ivec3 b, float h0, float h1)
{
    const int dx = std::abs(b.x - a.x);
    const int dy = std::abs(b.y - a.y);
    const int dz = std::abs(b.z - a.z);
    const int sx = a.x < b.x ? 1 : -1;
    const int sy = a.y < b.y ? 1 : -1;
    const int sz = a.z < b.z ? 1 : -1;
    const int dm = std::max({dx, dy, dz}); // dominant axis length

    int ex = dm / 2, ey = dm / 2, ez = dm / 2;
    int x = a.x, y = a.y, z = a.z;
    for (int i = 0; i <= dm; ++i)
    {
        const float t = dm ? static_cast<float>(i) / static_cast<float>(dm) : 0.0f;
        setVoxel(x, y, z, rainbow(h0 + (h1 - h0) * t));
        ex -= dx; if (ex < 0) { ex += dm; x += sx; }
        ey -= dy; if (ey < 0) { ey += dm; y += sy; }
        ez -= dz; if (ez < 0) { ez += dm; z += sz; }
    }
}

// Midpoint circle in the XZ plane at height cy. Hue follows the angle.
void drawCircle(int cx, int cy, int cz, int r)
{
    auto plot = [&](int dx, int dz) {
        const float hue = std::atan2(static_cast<float>(dz), static_cast<float>(dx))
                              / 6.2831853f + 0.5f;
        setVoxel(cx + dx, cy, cz + dz, rainbow(hue));
    };

    int x = r, z = 0, err = 1 - r;
    while (x >= z)
    {
        plot( x,  z); plot( z,  x); plot(-z,  x); plot(-x,  z);
        plot(-x, -z); plot(-z, -x); plot( z, -x); plot( x, -z);
        ++z;
        if (err < 0)
            err += 2 * z + 1;
        else
        {
            --x;
            err += 2 * (z - x) + 1;
        }
    }
}

// Hollow sphere (1-cell shell). Hue follows height.
void drawSphere(int cx, int cy, int cz, int r)
{
    for (int dx = -r; dx <= r; ++dx)
        for (int dy = -r; dy <= r; ++dy)
            for (int dz = -r; dz <= r; ++dz)
            {
                const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz));
                if (d > static_cast<float>(r) - 0.5f && d <= static_cast<float>(r) + 0.5f)
                    setVoxel(cx + dx, cy + dy, cz + dz,
                             rainbow(static_cast<float>(dy + r) / (2.0f * static_cast<float>(r))));
            }
}

// Redraws the model layer from the show/hide flags of each shape.
void rebuildModel()
{
    for (int x = 0; x < CELL_COUNT; ++x)
        for (int y = 0; y < CELL_COUNT; ++y)
            for (int z = 0; z < CELL_COUNT; ++z)
                g_modelFilled[x][y][z] = false;

    if (g_showBall)
        drawSphere(10, 10, 10, 4);                        // ball
    if (g_showRing)
        drawCircle(10, 10, 10, 8);                        // ring
    if (g_showLine)
        drawLine3D({3, 1, 3}, {17, 19, 17}, 0.0f, 1.0f);  // axis
}

// Blank canvas: hides every shape and removes hand-filled cells too.
void clearAll()
{
    g_showLine = g_showRing = g_showBall = false;
    for (int x = 0; x < CELL_COUNT; ++x)
        for (int y = 0; y < CELL_COUNT; ++y)
            for (int z = 0; z < CELL_COUNT; ++z)
                g_filled[x][y][z] = false;
    rebuildModel();
}

// ---------------------------------------------------------------------
// SHADER / WINDOW SETUP HELPERS
// ---------------------------------------------------------------------

GLuint compileShader(GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* code = source.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation error:\n" << log << '\n';
    }
    return shader;
}

GLuint buildShaderProgram()
{
    const std::string vertexSource = loadShaderSource("shaders/vertex.glsl");
    const std::string fragmentSource = loadShaderSource("shaders/fragment.glsl");
    if (vertexSource.empty() || fragmentSource.empty())
        return 0;

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

bool initWindow()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "CS5024 Assignment 1 - 3D Cubic Grid", nullptr, nullptr);
    if (!g_window)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(g_window);

    glewExperimental = GL_TRUE;
    const GLenum glewResult = glewInit();
    if (glewResult != GLEW_OK && glewResult != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr << "GLEW error: " << glewGetErrorString(glewResult) << '\n';
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    glLineWidth(1.5f);
    return true;
}

// Creates the cube mesh, grid mesh, HUD mesh, shader program, and reads
// all uniform locations. Returns false if anything failed.
bool setupScene()
{
    std::vector<float> cubeVertices;
    addCube(cubeVertices, glm::vec3(0.0f), glm::vec3(1.0f));
    g_cubeVAO = makeMesh(cubeVertices);

    const std::vector<float> gridVertices = buildGridVertices();
    g_gridVertexCount = static_cast<GLsizei>(gridVertices.size() / 6);
    g_gridVAO = makeMesh(gridVertices);

    const std::vector<float> hudVertices = buildHudVertices();
    g_hudVertexCount = static_cast<GLsizei>(hudVertices.size() / 6);
    g_hudVAO = makeMesh(hudVertices);

    g_shaderProgram = buildShaderProgram();
    if (g_shaderProgram == 0)
        return false;

    g_modelLoc = glGetUniformLocation(g_shaderProgram, "model");
    g_viewLoc = glGetUniformLocation(g_shaderProgram, "view");
    g_projectionLoc = glGetUniformLocation(g_shaderProgram, "projection");
    g_overrideColorLoc = glGetUniformLocation(g_shaderProgram, "overrideColor");
    g_useOverrideLoc = glGetUniformLocation(g_shaderProgram, "useOverrideColor");

    g_cubePosition = glm::vec3(cellCenter(1), cellCenter(1), cellCenter(1));

    rebuildModel(); // starts blank; press 1/2/3 to show shapes
    return true;
}

// ---------------------------------------------------------------------
// INPUT HELPERS
// ---------------------------------------------------------------------

bool keyPressedOnce(int key)
{
    const int now = glfwGetKey(g_window, key) == GLFW_PRESS;
    const bool edge = now && !g_previousKeys[key];
    g_previousKeys[key] = now;
    return edge;
}

void handleMovementKeys()
{
    if (keyPressedOnce(GLFW_KEY_LEFT))
        g_cubePosition.x = std::max(GRID_MIN + 0.5f, g_cubePosition.x - 1.0f);
    if (keyPressedOnce(GLFW_KEY_RIGHT))
        g_cubePosition.x = std::min(GRID_MAX - 0.5f, g_cubePosition.x + 1.0f);
    if (keyPressedOnce(GLFW_KEY_DOWN))
        g_cubePosition.y = std::max(GRID_MIN + 0.5f, g_cubePosition.y - 1.0f);
    if (keyPressedOnce(GLFW_KEY_UP))
        g_cubePosition.y = std::min(GRID_MAX - 0.5f, g_cubePosition.y + 1.0f);
    if (keyPressedOnce(GLFW_KEY_U))
        g_cubePosition.z = std::min(GRID_MAX - 0.5f, g_cubePosition.z + 1.0f);
    if (keyPressedOnce(GLFW_KEY_B))
        g_cubePosition.z = std::max(GRID_MIN + 0.5f, g_cubePosition.z - 1.0f);
}

void handleColorKey()
{
    if (keyPressedOnce(GLFW_KEY_C))
        g_cubeColor = readColor();
}

void fillCurrentCell()
{
    const int x = positionToCell(g_cubePosition.x);
    const int y = positionToCell(g_cubePosition.y);
    const int z = positionToCell(g_cubePosition.z);
    g_filled[x][y][z] = true;
    g_filledColor[x][y][z] = g_cubeColor;
}

void clearCurrentCell()
{
    const int x = positionToCell(g_cubePosition.x);
    const int y = positionToCell(g_cubePosition.y);
    const int z = positionToCell(g_cubePosition.z);
    g_filled[x][y][z] = false;
    g_modelFilled[x][y][z] = false;
}

void handleFillKeys()
{
    if (keyPressedOnce(GLFW_KEY_H))
        g_showGrid = !g_showGrid;
    if (keyPressedOnce(GLFW_KEY_0))
        clearAll();
    if (keyPressedOnce(GLFW_KEY_1))
    {
        g_showLine = !g_showLine;
        rebuildModel();
    }
    if (keyPressedOnce(GLFW_KEY_2))
    {
        g_showRing = !g_showRing;
        rebuildModel();
    }
    if (keyPressedOnce(GLFW_KEY_3))
    {
        g_showBall = !g_showBall;
        rebuildModel();
    }
    if (keyPressedOnce(GLFW_KEY_F))
        fillCurrentCell();
    if (keyPressedOnce(GLFW_KEY_W))
        clearCurrentCell();
}

void handleRotationKeys()
{
    if (keyPressedOnce(GLFW_KEY_L))
        g_rotation = glm::rotate(g_rotation, glm::radians(5.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    if (keyPressedOnce(GLFW_KEY_R))
        g_rotation = glm::rotate(g_rotation, glm::radians(-5.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    if (keyPressedOnce(GLFW_KEY_T))
        g_rotation = glm::rotate(g_rotation, glm::radians(5.0f),
                                 glm::vec3(1.0f, 0.0f, 0.0f));
    if (keyPressedOnce(GLFW_KEY_D))
        g_rotation = glm::rotate(g_rotation, glm::radians(-5.0f),
                                 glm::vec3(1.0f, 0.0f, 0.0f));
}

void processInput()
{
    handleMovementKeys();
    handleColorKey();
    handleFillKeys();
    handleRotationKeys();
}

// ---------------------------------------------------------------------
// DRAWING HELPERS
// ---------------------------------------------------------------------

// Points the shader's camera at the 3D scene (grid + cubes).
void useSceneCamera()
{
    const glm::mat4 view = glm::translate(glm::mat4(1.0f),
                                          glm::vec3(0.0f, 0.0f, -35.0f));
    const glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT),
        0.1f, 100.0f);
    glUniformMatrix4fv(g_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

// Switches the shader's camera to a flat 2D view for drawing the HUD text,
// where (0,0) is the top-left pixel of the window.
void useHudCamera()
{
    const glm::mat4 view(1.0f); // identity: HUD has no camera movement
    const glm::mat4 projection = glm::ortho(
        0.0f, static_cast<float>(WINDOW_WIDTH),
        static_cast<float>(WINDOW_HEIGHT), 0.0f,
        -1.0f, 1.0f);
    glUniformMatrix4fv(g_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void drawGridLines()
{
    glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(g_rotation));
    glUniform1i(g_useOverrideLoc, GL_FALSE);
    glBindVertexArray(g_gridVAO);
    glDrawArrays(GL_LINES, 0, g_gridVertexCount);
}

void drawFilledCubes()
{
    glUniform1i(g_useOverrideLoc, GL_TRUE);
    glBindVertexArray(g_cubeVAO);
    for (int x = 0; x < CELL_COUNT; ++x)
        for (int y = 0; y < CELL_COUNT; ++y)
            for (int z = 0; z < CELL_COUNT; ++z)
            {
                const bool manual = g_filled[x][y][z];
                if (!manual && !g_modelFilled[x][y][z])
                    continue;

                const glm::vec3& boxColor =
                    manual ? g_filledColor[x][y][z] : g_modelColor[x][y][z];
                glUniform3f(g_overrideColorLoc, boxColor.r, boxColor.g, boxColor.b);

                const glm::mat4 cellModel =
                    g_rotation * glm::translate(glm::mat4(1.0f),
                                                glm::vec3(cellCenter(x),
                                                          cellCenter(y),
                                                          cellCenter(z)));
                glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(cellModel));
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
}

void drawActiveCube()
{
    glUniform1i(g_useOverrideLoc, GL_TRUE);
    glUniform3f(g_overrideColorLoc, g_cubeColor.r, g_cubeColor.g, g_cubeColor.b);
    const glm::mat4 activeModel =
        g_rotation * glm::translate(glm::mat4(1.0f), g_cubePosition);
    glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(activeModel));
    glBindVertexArray(g_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

// Draws the controls legend on top of everything else, in the top-left
// corner of the window. Depth testing is turned off here so the HUD
// never gets hidden behind the rotated 3D scene.
void drawHud()
{
    glDisable(GL_DEPTH_TEST);
    useHudCamera();
    glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform1i(g_useOverrideLoc, GL_FALSE); // use each vertex's own baked-in color
    glBindVertexArray(g_hudVAO);

    // Background panel, blended so the grid shows through it.
    glEnable(GL_BLEND);
    glBlendColor(0.0f, 0.0f, 0.0f, HUD_PANEL_ALPHA);
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, HUD_PANEL_VERTEX_COUNT);
    glDisable(GL_BLEND);

    // Legend text fully solid so it stays crisp.
    glDrawArrays(GL_TRIANGLES, HUD_PANEL_VERTEX_COUNT,
                 g_hudVertexCount - HUD_PANEL_VERTEX_COUNT);

    glEnable(GL_DEPTH_TEST);
}

void renderFrame()
{
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_shaderProgram);

    useSceneCamera();
    if (g_showGrid)
        drawGridLines();
    drawFilledCubes();
    drawActiveCube();

    drawHud();

    glfwSwapBuffers(g_window);
    glfwPollEvents();
}

void cleanup()
{
    glDeleteVertexArrays(1, &g_cubeVAO);
    glDeleteVertexArrays(1, &g_gridVAO);
    glDeleteVertexArrays(1, &g_hudVAO);
    glDeleteProgram(g_shaderProgram);
    glfwDestroyWindow(g_window);
    glfwTerminate();
}

// ---------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------
int main()
{
    if (!initWindow())
        return -1;

    if (!setupScene())
    {
        cleanup();
        return -1;
    }

    std::cout << "Controls legend is shown in the top-left of the window.\n";

    while (!glfwWindowShouldClose(g_window))
    {
        if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(g_window, GLFW_TRUE);

        processInput();
        renderFrame();
    }

    cleanup();
    return 0;
}