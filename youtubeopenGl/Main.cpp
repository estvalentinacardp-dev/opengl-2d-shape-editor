// Aplicacion minima de GUI + dibujo usando GLFW + GLAD 
// Esta versión implementa una interfaz simple con botones dibujados
// manualmente y un 'canvas' en el que se pintan píxeles con algoritmos
// clásicos: Bresenham para líneas y algoritmo del punto medio para círculos.

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Tamaño de la ventana y del canvas
static const int WINDOW_W = 1000; // ancho de la ventana
static const int WINDOW_H = 800;  // alto de la ventana
static const int UI_W = 200;      // ancho del panel de UI a la izquierda
static const int CANVAS_W = WINDOW_W; // usar todo el buffer para dibujar (incluye UI a la izquierda)
static const int CANVAS_H = WINDOW_H;        // alto del área de dibujo

// Buffer de píxeles RGBA (uint8)
static std::vector<unsigned char> gPixels; // tamaño CANVAS_W * CANVAS_H * 4

// Color actual para dibujo (0..255)
static int gColorR = 200;
static int gColorG = 50;
static int gColorB = 50;
static bool gFillCircle = false; // activar relleno del círculo

// Textura OpenGL que contendrá el canvas de píxeles. Se actualiza cada frame con los datos de gPixels.
// TEXTURA REMOVIDA, ya que es más simple para este caso de uso (no necesitamos transformaciones ni efectos avanzados, solo mostrar el buffer de píxeles actualizado cada frame).

// Estado de la UI: botones y eventos del mouse
static bool gMouseDown = false;
static double gMouseX = 0.0, gMouseY = 0.0;
static bool gMouseClicked = false;
static int gClickX = 0, gClickY = 0;

// radio de círculo, con arrastre del mouse. El slider se dibuja dentro del menú desplegable y controla el radio de los círculos si no se usa el preset.
static int SLIDER_X = 710;
static const int SLIDER_Y = 15;
static const int SLIDER_W = 80;
static const int SLIDER_H = 20;
static bool gDraggingSlider = false;

// Menu UI: un único menú desplegable que agrupa las opciones
static bool gMenuOpen = false;
static const int MENU_X = 10;
static const int MENU_Y = 10;
static const int MENU_W = 140;
static const int MENU_H = 30;
static const int DROPDOWN_X = 10;
static const int DROPDOWN_Y = MENU_Y + MENU_H + 6;
static const int DROPDOWN_W = 320;
static const int DROPDOWN_H = 300;

//menu de apertura/cierre suave usando interpolación lineal de un valor de animación entre 0.0 (cerrado) y 1.0 (abierto), controlado por un temporizador para actualizar cada frame.
static float gMenuAnim = 0.0f; // 0.0 closed, 1.0 open
static float gMenuAnimTarget = 0.0f;
static float gMenuAnimSpeed = 6.0f; // units per second
static bool gMenuAnimating = false;
static double gLastTime = 0.0;

// Variables para dibujo de línea con dos clicks (opcional)
static bool gAwaitingSecondPoint = false;
static int gLineX0 = 0, gLineY0 = 0;
static int gPresetRadius = 50; // pixels
static const int gMinRadius = 5;
static const int gMaxRadius = 400;
static bool gUsePresetRadius = true;
// Toolbar state
enum Tool { TOOL_NONE=0, TOOL_LINE=1, TOOL_CIRCLE=2, TOOL_SELECT=3 };
static Tool gActiveTool = TOOL_NONE;

// Colors palette (light colors)
// Light visible palette
static const int PALETTE[][3] = {
    {102, 204, 255}, // celeste
    {255, 153, 204}, // rosado
    {255, 255, 153}, // amarillo claro
    {180, 255, 100}, // verde lima
    {255, 200, 140}, // naranja claro
    {200, 160, 255}, // morado claro
    {180, 220, 255}, // azul claro
    {255, 230, 180}  // crema
};
static const int PALETTE_COUNT = 8;

// ------------------------------------------------------------
// Utilidades de dibujo en el buffer de píxeles
// ------------------------------------------------------------

// Establece un píxel en el buffer de la aplicación.
// x,y: coordenadas del píxel en el canvas.
// r,g,b,a: componentes de color en 0..255.
// Comprueba límites para evitar escribir fuera del buffer.
static void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    // comprobar límites
    if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
    int idx = (y * CANVAS_W + x) * 4;
    gPixels[idx + 0] = r;
    gPixels[idx + 1] = g;
    gPixels[idx + 2] = b;
    gPixels[idx + 3] = a;
}

//TEST DE IMPLEMENTADO EN LA SECCIÓN DE FIGURAS, NO AQUÍ

// Devuelve la lista de coordenadas (x,y) que pertenecen
// al interior de un círculo (relleno) centrado en (cx,cy).
// Usado para rellenar círculos de forma eficiente por scanlines.
static std::vector<std::pair<int,int>> filledCirclePoints(int cx, int cy, int radius) {
    std::vector<std::pair<int,int>> pts;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        int dy = y - cy;
        int dxmax = (int)std::floor(std::sqrt((double)radius*radius - dy*dy));
        for (int x = cx - dxmax; x <= cx + dxmax; ++x) pts.emplace_back(x,y);
    }
    return pts;
}

// Rellena todo el canvas con un color de fondo.
// Parámetros opcionales r,g,b en 0..255.
static void clearCanvas(unsigned char r = 20, unsigned char g = 20, unsigned char b = 20) {
    for (int y = 0; y < CANVAS_H; ++y) {
        for (int x = 0; x < CANVAS_W; ++x) {
            int idx = (y * CANVAS_W + x) * 4;
            gPixels[idx + 0] = r;
            gPixels[idx + 1] = g;
            gPixels[idx + 2] = b;
            gPixels[idx + 3] = 255;
        }
    }
}

// Dibuja una línea entre (x0,y0) y (x1,y1) usando
// el algoritmo de Bresenham (enteros, sin antialias).
// Los colores r,g,b definen el color de la línea.
static void drawLineBresenham(int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        setPixel(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// Dibuja un círculo usando el algoritmo del punto medio.
// Si fill==true, dibuja el círculo relleno (scanlines);
// si false, solo dibuja el contorno de píxeles del círculo.
static void drawCircleMidpoint(int cx, int cy, int radius, unsigned char rcol, unsigned char gcol, unsigned char bcol, bool fill) {
    int x = radius;
    int y = 0;
    int err = 1 - x;
    while (x >= y) {
		// Dibujar los 8 octantes del círculo usando simetría. Si fill es true, dibujar líneas horizontales entre los puntos en cada octante para rellenar el círculo.
        if (fill) {
            // Para rellenar, dibujar líneas horizontales entre los puntos en cada octante
            for (int xi = cx - x; xi <= cx + x; ++xi) { setPixel(xi, cy + y, rcol, gcol, bcol); setPixel(xi, cy - y, rcol, gcol, bcol); }
            for (int xi = cx - y; xi <= cx + y; ++xi) { setPixel(xi, cy + x, rcol, gcol, bcol); setPixel(xi, cy - x, rcol, gcol, bcol); }
        } else {
            setPixel(cx + x, cy + y, rcol, gcol, bcol);
            setPixel(cx - x, cy + y, rcol, gcol, bcol);
            setPixel(cx + x, cy - y, rcol, gcol, bcol);
            setPixel(cx - x, cy - y, rcol, gcol, bcol);
            setPixel(cx + y, cy + x, rcol, gcol, bcol);
            setPixel(cx - y, cy + x, rcol, gcol, bcol);
            setPixel(cx + y, cy - x, rcol, gcol, bcol);
            setPixel(cx - y, cy - x, rcol, gcol, bcol);
        }
        y++;
        if (err <= 0) {
            err += 2*y + 1;
        } else {
            x--;
            err += 2*(y - x) + 1;
        }
    }
}

// ------------------------------------------------------------
// UI helpers (dibujar botones y sliders en la izquierda)
// ------------------------------------------------------------

// Test de colisión simple: devuelve true si el punto (mx,my)
// se encuentra dentro del rectángulo definido por (x,y,w,h).
static bool pointInRect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

// Dibuja un rectángulo sólido rellenando píxeles usando setPixel.
// Se usa para botones y fondos de controles simples.
static void drawUIRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
    for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) setPixel(xx, yy, r, g, b);
}

// Dibuja un rectángulo con mezcla alpha sobre el buffer de píxeles. 
// alpha en 0..1 controla la opacidad del rectángulo sobre lo ya dibujado.
static void drawUIRectAlpha(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            if (xx < 0 || xx >= CANVAS_W || yy < 0 || yy >= CANVAS_H) continue;
            int idx = (yy * CANVAS_W + xx) * 4;
            float inv = 1.0f - alpha;
            gPixels[idx+0] = (unsigned char)(r * alpha + gPixels[idx+0] * inv);
            gPixels[idx+1] = (unsigned char)(g * alpha + gPixels[idx+1] * inv);
            gPixels[idx+2] = (unsigned char)(b * alpha + gPixels[idx+2] * inv);
			//CANAL ALPHA NO SE USA PARA DIBUJO, SOLO PARA CONTROL DE OPACIDAD EN LA MEZCLA, POR LO QUE SE MANTIENE A 255 PARA SIMPLIFICAR EL CÓDIGo Y EVITAR CONFUSIÓN.
        }
    }
}

// Dibujar un icono de botón para 'círculo' y 'línea' dentro de un rect
// Dibuja un icono de círculo para usar dentro de un botón.
// No altera la lógica, solo pinta los píxeles del icono.
static void drawUIButtonIconCircle(int x, int y, int w, int h, unsigned char colR, unsigned char colG, unsigned char colB) {
    int cx = x + w/2; int cy = y + h/2; int rad = std::min(w,h)/3;
    // dibujar contorno simple con algoritmo del punto medio
    drawCircleMidpoint(cx, cy, rad, colR, colG, colB, false);
}
// Dibuja un icono de línea (diagonal) dentro de un rectángulo.
// Usado para representar la herramienta de líneas en la UI.
static void drawUIButtonIconLine(int x, int y, int w, int h, unsigned char colR, unsigned char colG, unsigned char colB) {
    int x0 = x + w/4; int y0 = y + h/4; int x1 = x + 3*w/4; int y1 = y + 3*h/4;
    drawLineBresenham(x0, y0, x1, y1, colR, colG, colB);
}

// ------------------------------------------------------------

// ------------------------------------------------------------
// Inicialización de OpenGL/GLFW y textura
// ------------------------------------------------------------
// Inicializa GLFW y solicita un contexto OpenGL adecuado.
// Devuelve true si la inicialización tuvo éxito.
static bool initGLFWandGL() {
    if (!glfwInit()) return false;
    // Solicitar OpenGL 3.3 core para usar VAO/VBO y shaders modernos
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    return true;
}

// ------------------------------------------------------------
// Shaders and GL helpers
// ------------------------------------------------------------
// Compila un shader de OpenGL a partir del código fuente proporcionado.
// type: GL_VERTEX_SHADER o GL_FRAGMENT_SHADER.
// src: código GLSL en una cadena C.
// Devuelve el identificador del shader compilado (o 0 en error).
static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, 512, NULL, buf); std::cout << "Shader compile error: " << buf << std::endl;
    }
    return s;
}
// Crea y enlaza un programa GLSL a partir de los fuentes vertex y fragment.
// Devuelve el id del programa enlazado.
static GLuint createProgramFromSrc(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char buf[512]; glGetProgramInfoLog(p, 512, NULL, buf); std::cout << "Program link error: " << buf << std::endl; }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

//textura que contiene el buffer de píxeles. Se renderiza cada frame con glDrawArrays para mostrar el canvas actualizado.
static GLuint gQuadVAO = 0;
static GLuint gQuadVBO = 0;
static GLuint gTexProgram = 0;
static GLuint gPointsProgram = 0;
static GLuint gTexture = 0;

static const char* texVS = R"glsl(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aTex;
out vec2 vTex;
void main(){ vTex = aTex; gl_Position = vec4(aPos,0.0,1.0); }
)glsl";
static const char* texFS = R"glsl(#version 330 core
in vec2 vTex; out vec4 FragColor; uniform sampler2D uTex; void main(){ FragColor = texture(uTex, vTex); }
)glsl";

static const char* pointsVS = R"glsl(#version 330 core
layout(location=0) in vec2 aPos; uniform float uPointSize; void main(){ gl_Position = vec4(aPos,0.0,1.0); gl_PointSize = uPointSize; }
)glsl";
static const char* pointsFS = R"glsl(#version 330 core
uniform vec3 uColor; out vec4 FragColor; void main(){ FragColor = vec4(uColor,1.0); }
)glsl";

// Crea los buffers VAO/VBO para un quad texturado que cubre toda la pantalla.
// El quad se usa para dibujar la textura que contiene el canvas de píxeles.
static void createTexturedQuad() {
    float verts[] = {
        // positions   // tex
        -1.0f,  1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
         1.0f,  1.0f,   1.0f, 0.0f
    };
    glGenVertexArrays(1, &gQuadVAO);
    glGenBuffers(1, &gQuadVBO);
    glBindVertexArray(gQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

// Crea la textura OpenGL que contendrá el buffer de píxeles (canvas).
// Inicializa la textura usando los datos de gPixels.
static void createTexture() {
    if (gTexture) glDeleteTextures(1, &gTexture);
    glGenTextures(1, &gTexture);
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,CANVAS_W,CANVAS_H,0,GL_RGBA,GL_UNSIGNED_BYTE,gPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
// Actualiza la textura OpenGL con los datos actuales de gPixels.
// Llamar cada frame después de modificar el buffer de píxeles.
static void updateTexture() {
    glBindTexture(GL_TEXTURE_2D, gTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,CANVAS_W,CANVAS_H,GL_RGBA,GL_UNSIGNED_BYTE,gPixels.data());
}

// ------------------------------------------------------------

// tipos de figuras que se pueden dibujar: líneas (polilíneas) y círculos.
enum FigureType { FIG_LINE=1, FIG_CIRCLE=2 };
// figuras, además del color y tipo. Se renderizan cada frame aplicando los offsets para permitir transformaciones simples (mover con flechas).
struct Figure {
    FigureType type = FIG_LINE;
	// color en 0..255 para esta figura (se guarda para permitir transformaciones sin perder el color original)
    int cr=200,cg=50,cb=50;
	//datos de la línea: lista de vértices para polilíneas (cada par es un punto)
    std::vector<std::pair<int,int>> verts; // logical vertices
	//  datos del círculo (centro y radio) para figuras de tipo círculo
    int cx=0, cy=0, radius=0; bool fill=false;
    //transformación
    int offsetX=0, offsetY=0;
};
static std::vector<Figure> gFigures;

//dibuja una polilínea continua entre clicks sucesivos, hasta que se cambie de herramienta o se cierre el menú.
static bool gDrawingPolyline = false;
//  declariciones de funciones para coleccionistas usados abajo
static void renderAllFiguresToCanvas();
static std::vector<std::pair<int,int>> bresenhamPoints(int x0, int y0, int x1, int y1);
static std::vector<std::pair<int,int>> midpointCirclePoints(int cx, int cy, int radius);
static std::vector<std::pair<int,int>> filledCirclePoints(int cx, int cy, int radius);
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double mx, my; glfwGetCursorPos(window, &mx, &my);
    int imx = (int)mx, imy = (int)my;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS) {
		// MENU DE APERTURA/CIERRE DEL MENÚ DESPLEGABLE
        if (pointInRect(imx, imy, MENU_X, MENU_Y, MENU_W, MENU_H)) { gMenuOpen = !gMenuOpen; gMenuAnimTarget = gMenuOpen ? 1.0f : 0.0f; gMenuAnimating = true; return; }

		//SI MENU ABIERTO, MANEJAR CLICKS DENTRO DEL ÁREA DEL DROPDOWN PARA TODAS LAS OPCIONES (HERRAMIENTAS, PALETA, SLIDER, UNDO/CLEAR)
        if (gMenuOpen && pointInRect(imx, imy, DROPDOWN_X, DROPDOWN_Y, DROPDOWN_W, DROPDOWN_H)) {
            // Tools buttons inside dropdown
            int toolY = DROPDOWN_Y + 8;
            if (pointInRect(imx, imy, DROPDOWN_X + 8, toolY, 80, 30)) { gActiveTool = TOOL_LINE; gDrawingPolyline = false; gAwaitingSecondPoint = false; renderAllFiguresToCanvas(); return; }
            if (pointInRect(imx, imy, DROPDOWN_X + 96, toolY, 80, 30)) { gActiveTool = TOOL_CIRCLE; gDrawingPolyline = false; gAwaitingSecondPoint = false; renderAllFiguresToCanvas(); return; }
			//PALETA DE COLORES DENTRO DEL DROPDOWN, CADA COLOR ES UN BOTÓN DE 30x30 PÍXELES, SELECCIONA EL COLOR ACTUAL Y REDIBUJA TODO EL CANVAS PARA MOSTRAR EL CAMBIO DE COLOR EN LAS FIGURAS EXISTENTES
            // (QUE SE RENDERIZAN CON SU COLOR GUARDADO, POR LO QUE SOLO AFECTA A LAS NUEVAS FIGURAS QUE SE CREAN DESPUÉS DEL CAMBIO DE COLOR).
            int palY = DROPDOWN_Y + 48;
            for (int i=0;i<PALETTE_COUNT;++i) {
                int bx = DROPDOWN_X + 8 + i*40;
                if (pointInRect(imx, imy, bx, palY, 30, 30)) {
                    gColorR = PALETTE[i][0]; gColorG = PALETTE[i][1]; gColorB = PALETTE[i][2];
                    renderAllFiguresToCanvas();
                    return;
                }
            }
			// control del radio con botones + y - y toggle preset, toggle fill
            int rcY = DROPDOWN_Y + 92;
            if (pointInRect(imx, imy, DROPDOWN_X + 8, rcY, 30, 30)) { gPresetRadius += 4; if (gPresetRadius>gMaxRadius) gPresetRadius=gMaxRadius; renderAllFiguresToCanvas(); return; }
            if (pointInRect(imx, imy, DROPDOWN_X + 48, rcY, 30, 30)) { gPresetRadius -= 4; if (gPresetRadius<gMinRadius) gPresetRadius=gMinRadius; renderAllFiguresToCanvas(); return; }
            if (pointInRect(imx, imy, DROPDOWN_X + 88, rcY, 20, 30)) { gUsePresetRadius = !gUsePresetRadius; renderAllFiguresToCanvas(); return; }
			// alterna entre círculo relleno y solo contorno, se aplica a los círculos creados después del cambio, las ya creadas mantienen su estado de relleno original guardado en la figura.
            if (pointInRect(imx, imy, DROPDOWN_X + 120, rcY, 100, 30)) { gFillCircle = !gFillCircle; renderAllFiguresToCanvas(); return; }
			//  controla el radio de forma visual (solo si no se usa el preset)
            SLIDER_X = DROPDOWN_X + 8;
            if (pointInRect(imx, imy, SLIDER_X, DROPDOWN_Y + 128, SLIDER_W, SLIDER_H)) {
                gDraggingSlider = true; gMouseDown = true;
                int rel = imx - SLIDER_X; if (rel < 0) rel = 0; if (rel > SLIDER_W) rel = SLIDER_W;
                gPresetRadius = gMinRadius + (rel * (gMaxRadius - gMinRadius)) / SLIDER_W;
                return;
            }  
			// undo / limpiar dentro del dropdown
            if (pointInRect(imx, imy, DROPDOWN_X + 8, DROPDOWN_Y + 168, 80, 30)) { if (!gFigures.empty()) gFigures.pop_back(); renderAllFiguresToCanvas(); return; }
            if (pointInRect(imx, imy, DROPDOWN_X + 96, DROPDOWN_Y + 168, 80, 30)) { gFigures.clear(); clearCanvas(); return; }
			//  ignore clicks but do not close menu ni iniciar dibujo
            return;
        }
		//modo de dibujo: manejar creación de polilínea o círculo (solo al hacer click en el canvas, fuera del menú)
        if (gActiveTool == TOOL_LINE) {
			// empieza o añade a la polilínea actual
            if (!gDrawingPolyline) {
                Figure nf; nf.type = FIG_LINE; nf.cr = gColorR; nf.cg = gColorG; nf.cb = gColorB; nf.offsetX = 0; nf.offsetY = 0;
                nf.verts.emplace_back(imx, imy);
                gFigures.push_back(nf);
                gDrawingPolyline = true;
            } else {
				// asume vertex continuo, no se crean nuevas figuras hasta que se cambie de herramienta o se cierre el menú
                gFigures.back().verts.emplace_back(imx, imy);
            }
			//redibujar todas las figuras (incluida la polilínea en construcción) cada vez que se añade un vértice, para mostrar el avance
            renderAllFiguresToCanvas();
            return;
        }
        if (gActiveTool == TOOL_CIRCLE) {
            if (!gAwaitingSecondPoint) { gLineX0 = imx; gLineY0 = imy; gAwaitingSecondPoint = true; }
            else {
                int r = gUsePresetRadius ? gPresetRadius : (int)std::round(std::sqrt((imx-gLineX0)*(imx-gLineX0) + (imy-gLineY0)*(imy-gLineY0)));
                Figure nf; nf.type = FIG_CIRCLE; nf.cx = gLineX0; nf.cy = gLineY0; nf.radius = r; nf.fill = gFillCircle;
                nf.cr = gColorR; nf.cg = gColorG; nf.cb = gColorB; nf.offsetX = 0; nf.offsetY = 0;
                gFigures.push_back(nf);
                gAwaitingSecondPoint = false;
                renderAllFiguresToCanvas();
            }
            return;
        }
		//  ignore otros clicks, no inician dibujo ni cierran el menú
    } else if (action == GLFW_RELEASE) {
        gMouseDown = false; gDraggingSlider = false;
		//  acciones de liberación: solo para el slider, no para el dibujo (que se maneja con clicks)
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        // Deshacer última figura: Z
        if (key == GLFW_KEY_Z) {
            if (!gFigures.empty()) gFigures.pop_back();
            renderAllFiguresToCanvas();
        }
        // L = llenar círculos; V = vaciar (solo contorno)
        if (key == GLFW_KEY_L) { gFillCircle = true; }
        if (key == GLFW_KEY_V) { gFillCircle = false; }
        // cancelar creación actual (si por error quedó esperando segundo punto)
        if (key == GLFW_KEY_ESCAPE) {
            gAwaitingSecondPoint = false;
            // también cancelar arrastre del slider
            gDraggingSlider = false; gMouseDown = false;
        }
		// mueve la última figura creada con las flechas, aplicando un offset a su posición lógica y redibujando todo el canvas para mostrar el cambio.
        // Solo afecta a la última figura para simplificar la lógica de transformación.
        if (!gFigures.empty()) {
            Figure &last = gFigures.back();
            if (key == GLFW_KEY_RIGHT) { last.offsetX += 5; renderAllFiguresToCanvas(); }
            if (key == GLFW_KEY_LEFT)  { last.offsetX -= 5; renderAllFiguresToCanvas(); }
            if (key == GLFW_KEY_UP)    { last.offsetY -= 5; renderAllFiguresToCanvas(); }
            if (key == GLFW_KEY_DOWN)  { last.offsetY += 5; renderAllFiguresToCanvas(); }
        }
        // Borrar todo con X
        if (key == GLFW_KEY_X) { gFigures.clear(); clearCanvas(); }
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) gPresetRadius += 4; else gPresetRadius -= 4;
    if (gPresetRadius < gMinRadius) gPresetRadius = gMinRadius;
    if (gPresetRadius > gMaxRadius) gPresetRadius = gMaxRadius;
}

//  RENDERIZA TODAS LAS FIGURAS EN EL BUFFER DE PÍXELES USANDO BRESENHAM (APLICA LOS OFFSETS)
static void renderAllFiguresToCanvas() {
	// LIMPIA TODO EL CANVAS (INCLUYENDO ÁREA DE UI, para simplificar)
    clearCanvas();
    for (auto &fig : gFigures) {
        if (fig.type == FIG_LINE) {
			//dibujando una polilínea entre los vértices usando Bresenham, aplicando los offsets de la figura a cada vértice para permitir transformaciones simples.
            for (size_t i=1;i<fig.verts.size();++i) {
                int x0 = fig.verts[i-1].first + fig.offsetX;
                int y0 = fig.verts[i-1].second + fig.offsetY;
                int x1 = fig.verts[i].first + fig.offsetX;
                int y1 = fig.verts[i].second + fig.offsetY;
                auto pts = bresenhamPoints(x0,y0,x1,y1);
                for (auto &p: pts) setPixel(p.first, p.second, fig.cr, fig.cg, fig.cb);
            }
        } else if (fig.type == FIG_CIRCLE) {
            int cx = fig.cx + fig.offsetX; int cy = fig.cy + fig.offsetY;
            if (fig.fill) {
                auto pts = filledCirclePoints(cx, cy, fig.radius);
                for (auto &p: pts) setPixel(p.first, p.second, fig.cr, fig.cg, fig.cb);
            } else {
                auto pts = midpointCirclePoints(cx, cy, fig.radius);
                for (auto &p: pts) setPixel(p.first, p.second, fig.cr, fig.cg, fig.cb);
            }
        }
    }
}

//  tests de colisión para selección de figuras, no implementados en esta versión simplificada pero podrían añadirse fácilmente iterando sobre gFigures y 
// comprobando si el punto del mouse cae dentro de sus límites lógicos (líneas con un margen de error, círculos con su radio, etc).

// declaraciones de funciones para coleccionistas usados abajo
static std::vector<std::pair<int,int>> bresenhamPoints(int x0, int y0, int x1, int y1);
static std::vector<std::pair<int,int>> midpointCirclePoints(int cx, int cy, int radius);
static std::vector<std::pair<int,int>> filledCirclePoints(int cx, int cy, int radius);

// recolectar y devuelve la lista de coordenadas de píxeles que forman la línea entre (x0,y0) y (x1,y1) usando el algoritmo de Bresenham. 
// Útil para procesar o repintar líneas sin dibujar directamente en el buffer.
// Colecciona y devuelve la lista de píxeles que forman la línea entre
// (x0,y0) y (x1,y1) usando Bresenham. Útil para procesar o repintar.
static std::vector<std::pair<int,int>> bresenhamPoints(int x0, int y0, int x1, int y1) {
    std::vector<std::pair<int,int>> out;
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        out.emplace_back(x0,y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
    return out;
}

//punto del medio para círculos, devuelve los puntos del perímetro de un círculo centrado en (cx,cy) con el radio dado.
// Colecciona y devuelve los puntos del perímetro de un círculo
// usando el algoritmo del punto medio. Devuelve puntos únicos ordenados.
static std::vector<std::pair<int,int>> midpointCirclePoints(int cx, int cy, int radius) {
    std::vector<std::pair<int,int>> pts;
    int x = radius; int y = 0; int err = 1 - x;
    auto add8 = [&](int xx,int yy){ pts.emplace_back(cx+xx, cy+yy); pts.emplace_back(cx-xx, cy+yy); pts.emplace_back(cx+xx, cy-yy); pts.emplace_back(cx-xx, cy-yy);
        pts.emplace_back(cx+yy, cy+xx); pts.emplace_back(cx-yy, cy+xx); pts.emplace_back(cx+yy, cy-xx); pts.emplace_back(cx-yy, cy-xx); };
    while (x >= y) {
        add8(x,y);
        y++;
        if (err <= 0) err += 2*y + 1;
        else { x--; err += 2*(y - x) + 1; }
    }
	// duplicado en octantes
    std::sort(pts.begin(), pts.end()); pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
	// angulo para que se dibujen en orden circular (no es necesario para el dibujo pero puede ser útil para otras aplicaciones)
    std::sort(pts.begin(), pts.end(), [&](const std::pair<int,int>& a, const std::pair<int,int>& b){
        double anga = atan2((double)(a.second - cy),(double)(a.first - cx));
        double angb = atan2((double)(b.second - cy),(double)(b.first - cx));
        return anga < angb;
    });
    return pts;
}

// no textura para dibujar el buffer de píxeles directamente, así que no necesitamos funciones de textura para círculos rellenos.

// ------------------------------------------------------------
// Main: ventana, bucle principal, UI y lógica de dibujo
// ------------------------------------------------------------
// Punto de entrada: inicializa GL, crea ventana, entra al bucle principal
// y gestiona render y eventos hasta que el usuario cierra la ventana.
int main() {
    // inicializar GLFW y crear ventana
    if (!initGLFWandGL()) { std::cout << "GLFW init failed" << std::endl; return -1; }
    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Algoritmos de Raster - GUI Minima", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "GLAD failed" << std::endl; return -1; }
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // preparar buffer de píxeles
    gPixels.resize(CANVAS_W * CANVAS_H * 4);
    clearCanvas();

    // shaders y recursos GL modernos
    glViewport(0, 0, WINDOW_W, WINDOW_H);
    gTexProgram = createProgramFromSrc(texVS, texFS);
    gPointsProgram = createProgramFromSrc(pointsVS, pointsFS);
    createTexturedQuad();
    createTexture();

    // inicializar tiempo
    gLastTime = glfwGetTime();
    // bucle principal
    while (!glfwWindowShouldClose(window)) {
        // leer posición del ratón
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        gMouseX = mx; gMouseY = my;

        // convertir coordenadas del mouse a coordenadas del canvas (si dentro)
        int imx = (int)mx;
        int imy = (int)my;

        // clicks handled in mouse callback; continuous interactions handled there

        // Render figures into canvas first
        renderAllFiguresToCanvas();

        // actualizar animación del menú
        double now = glfwGetTime(); double dt = now - gLastTime; gLastTime = now;
        if (gMenuAnimating) {
            float diff = gMenuAnimTarget - gMenuAnim;
            float step = (float)(gMenuAnimSpeed * dt);
            if (std::abs(diff) <= step) { gMenuAnim = gMenuAnimTarget; gMenuAnimating = false; }
            else gMenuAnim += (diff > 0 ? step : -step);
        }

        // dibujar toolbar en buffer (superior) - ahora con Menu principal
        // limpiar area toolbar (0..WINDOW_W, 0..60)
        for (int yy = 0; yy < 60; ++yy) for (int xx = 0; xx < WINDOW_W; ++xx) setPixel(xx, yy, 30,30,30);
        // Botón principal "Menu"
        drawUIRect(MENU_X, MENU_Y, MENU_W, MENU_H, gMenuOpen ? 200 : 120, 200, 120);
        // label simple dentro del botón
        // dibujar icono de menu (tres líneas) - estilizado
        for (int lx = 0; lx < MENU_W - 24; lx += 6) {
            int sx = MENU_X + 12 + lx/6*6;
            for (int xx = sx; xx < sx + 12; ++xx) for (int yy = MENU_Y + 8; yy < MENU_Y + 12; ++yy) setPixel(xx, yy, 0,0,0);
        }

        // Si el menú está (animando) abierto, dibujar dropdown con secciones ordenadas aplicando alpha
        if (gMenuAnim > 0.001f) {
            float a = gMenuAnim; // alpha
            // fondo del dropdown con alpha
            drawUIRectAlpha(DROPDOWN_X, DROPDOWN_Y, DROPDOWN_W, DROPDOWN_H, 45,45,45, a);
            // Títulos de sección
            // Tools
            for (int xx = DROPDOWN_X + 8; xx < DROPDOWN_X + DROPDOWN_W - 8; ++xx) for (int yy = DROPDOWN_Y + 4; yy < DROPDOWN_Y + 6; ++yy) setPixel(xx, yy, 120,120,120);
            // botones de herramientas
            int toolY = DROPDOWN_Y + 8;
            drawUIRectAlpha(DROPDOWN_X + 8, toolY, 80, 30, gActiveTool==TOOL_LINE?200:120,200,120, a);
            drawUIButtonIconLine(DROPDOWN_X + 8, toolY, 80, 30, (unsigned char)(0*a), (unsigned char)(0*a), (unsigned char)(0*a));
            drawUIRectAlpha(DROPDOWN_X + 96, toolY, 80, 30, gActiveTool==TOOL_CIRCLE?200:120,200,120, a);
            drawUIButtonIconCircle(DROPDOWN_X + 96, toolY, 80, 30, (unsigned char)(0*a), (unsigned char)(0*a), (unsigned char)(0*a));
            // Paleta de colores (alineada con spacing)
            int palY = DROPDOWN_Y + 48;
            for (int i=0;i<PALETTE_COUNT;++i) drawUIRectAlpha(DROPDOWN_X + 8 + i*40, palY, 30, 30, PALETTE[i][0], PALETTE[i][1], PALETTE[i][2], a);
            // Controles de radio
            int rcY = DROPDOWN_Y + 92;
            drawUIRectAlpha(DROPDOWN_X + 8, rcY, 30, 30, 120,200,120, a); // +
            drawUIRectAlpha(DROPDOWN_X + 48, rcY, 30, 30, 255,180,100, a); // -
            drawUIRectAlpha(DROPDOWN_X + 88, rcY, 20, 30, gUsePresetRadius?200:80,200,80, a); // toggle
			// linea vs círculo relleno
            drawUIRectAlpha(DROPDOWN_X + 120, rcY, 100, 30, gFillCircle?200:120, 200, 120, a);
			// Slider de control de radio (solo si no se usa el preset)
            int sliderY = DROPDOWN_Y + 128;
            SLIDER_X = DROPDOWN_X + 8; int rx = SLIDER_X; int ry = sliderY; int rw = SLIDER_W;
            int val = (int)(((float)(gPresetRadius - gMinRadius) / (gMaxRadius - gMinRadius)) * rw);
            drawUIRectAlpha(rx, ry, rw, SLIDER_H, 200,200,120, a);
            int kx = rx + val - 4; if (kx < rx) kx = rx; if (kx > rx + rw - 8) kx = rx + rw - 8;
            drawUIRectAlpha(kx, ry-2, 8, SLIDER_H+4, 80,80,200, a);
			//botones de limpiar y deshacer
            drawUIRectAlpha(DROPDOWN_X + 8, DROPDOWN_Y + 168, 80, 30, 160,160,200, a);
            drawUIRectAlpha(DROPDOWN_X + 96, DROPDOWN_Y + 168, 80, 30, 200,160,160, a);
        }

		// si estamos arrastrando el slider y el mouse está presionado, actualizar el radio preset según la posición actual del mouse en el slider, 
        // mapeando la posición X del mouse dentro del área del slider al rango de radios permitido.
        if (gDraggingSlider && gMouseDown) {
            int rel = (int)gMouseX - SLIDER_X; if (rel < 0) rel = 0; if (rel > SLIDER_W) rel = SLIDER_W;
            gPresetRadius = gMinRadius + (rel * (gMaxRadius - gMinRadius)) / SLIDER_W;
        }

		//  acutualizado (con figuras y UI) antes de dibujar el quad texturado
        updateTexture();
		//renderizando el buffer de píxeles (canvas) a la pantalla usando OpenGL. El canvas se ha actualizado con todas las figuras y la UI antes de este paso.
        glClearColor(0.0f,0.0f,0.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(gTexProgram);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gTexture);
        glUniform1i(glGetUniformLocation(gTexProgram, "uTex"), 0);
        glBindVertexArray(gQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

		//no renderizado con GL, sino que se dibuja desde el buffer de píxeles, así que no hay llamadas a glDrawArrays para figuras individuales aquí.

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

	// limpiar recursos GL
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
