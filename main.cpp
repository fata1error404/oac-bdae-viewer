#include <iostream>
#include <iomanip>
#include <string>
#include <filesystem>
#include "libs/glad/glad.h"                  // library for OpenGL functions loading (like glClear or glViewport)
#include "libs/glm/glm.hpp"                  // library for OpenGL style mathematics (basic vector and matrix mathematics functions)
#include "libs/glm/gtc/matrix_transform.hpp" // for matrix transformation functions
#include "libs/glm/gtc/type_ptr.hpp"         // for matrix conversion to raw pointers (OpenGL compatibility with GLM)
#include "libs/stb_image.h"                  // library for image loading
#include "libs/imgui/imgui.h"                // library for UI elements
#include "libs/imgui/imgui_impl_opengl3.h"   // connects Dear ImGui with OpenGL
#include "libs/imgui/imgui_impl_glfw.h"      // connects Dear ImGui with GLFW
#include "libs/imgui/ImGuiFileDialog.h"      // extension for file browsing dialog

#include "shader.h"     // implementation of the graphics pipeline
#include "camera.h"     // implementation of the camera system
#include "sound.h"      // implementation of the sound system
#include "light.h"      // definition of the light settings and light cube
#include "terrain.h"    // implementation of the terrain engine
#include "parserBDAE.h" // parser for 3D models
#include "parserTRN.h"  // parser for terrain meshes

#ifdef __linux__
#include <GLFW/glfw3.h> // library for creating windows and handling input – mouse clicks, keyboard input, or window resizes
#elif _WIN32
#include "libs/glfw/glfw3.h"
#endif

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow *window);

// window settings
bool isFullscreen = false;
const unsigned int DEFAULT_WINDOW_WIDTH = 800;
const unsigned int DEFAULT_WINDOW_HEIGHT = 600;
const unsigned int DEFAULT_WINDOW_POS_X = 100;
const unsigned int DEFAULT_WINDOW_POS_Y = 100;

unsigned int currentWindowWidth = DEFAULT_WINDOW_WIDTH;
unsigned int currentWindowHeight = DEFAULT_WINDOW_HEIGHT;

// create a Camera class instance with a specified position and default values for other parameters, to access its functionality
Camera ourCamera;

// for passing local application variables to callback functions (GLFW supports only one user pointer per window, so to pass multiple objects, we wrap them in a context struct and pass a pointer to that struct)
struct AppContext
{
    Model *model;
    Light *light;
};

float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f; // time of last frame

bool firstMouse = true;                     // flag to check if the mouse movement is being processed for the first time
double lastX = DEFAULT_WINDOW_WIDTH / 2.0;  // starting cursor position (x-axis)
double lastY = DEFAULT_WINDOW_HEIGHT / 2.0; // starting cursor position (y-axis)

// viewer variables
bool fileDialogOpen = false;       // flag that indicates whether to block all background inputs (when the file browsing dialog is open)
bool settingsPanelHovered = false; // flag that indicated whether to block background mouse input (when interacting with the settings panel)
bool displayBaseMesh = false;      // flag that indicates base / textured mesh display mode
bool isTerrainViewer = false;

int main()
{
    // initialize and configure (use core profile mode and OpenGL v3.3)
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // GLFW window creation
    GLFWwindow *window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "BDAE 3D Model Viewer", NULL, NULL);

    // set window icon
    int width, height, nrChannels;
    unsigned char *data = stbi_load("aux_docs/app icon.png", &width, &height, &nrChannels, 0);
    GLFWimage icon;
    icon.width = width;
    icon.height = height;
    icon.pixels = data;
    glfwSetWindowIcon(window, 1, &icon);
    stbi_image_free(data);

    // set OpenGL context and callback
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);   // register mouse wheel callback
    glfwSetCursorPosCallback(window, mouse_callback); // register mouse movement callback
    glfwSetKeyCallback(window, key_callback);         // register key callback

    // load all OpenGL function pointers
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // setup settings panel (Dear ImGui library)
    ImGui::CreateContext();
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    ImGui::GetIO().IniFilename = NULL; // disable saving UI states to .ini file

    // apply styles to have a grayscale theme
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;                                              // border radius
    style.WindowBorderSize = 0.0f;                                            // border width
    style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);             // text color
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);         // background color of the panel's main content area
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);    // background color of the panel's title bar
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); // .. (when panel is hidden)
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);          // .. (when panel is overlayed and inactive)
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);          // background color of input fields, checkboxes
    style.Colors[ImGuiCol_Button] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);           // background color of buttons
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);        // mark color in checkboxes
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);       // background color of sliders
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.65f, 0.65f, 0.65f, 1.0f); // background color of table headers (for file browsing dialog)
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);   // background color of scrollbar tracks (for file browsing dialog)
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.65f, 0.65f, 0.65f, 1.0f); // background color of scrollbar thumbs (for file browsing dialog)
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);       // background color of tooltips (for file browsing dialog)

    // configure file browsing dialog
    IGFD::FileDialogConfig cfg;
    cfg.path = "./data/model";                                                             // default path
    cfg.fileName = "";                                                                     // default file name (none)
    cfg.filePathName = "";                                                                 // default file path name (none)
    cfg.countSelectionMax = 1;                                                             // only allow to select one file
    cfg.flags = ImGuiFileDialogFlags_HideColumnType | ImGuiFileDialogFlags_HideColumnDate; // flags: hide file type and date columns
    cfg.userFileAttributes = NULL;                                                         // no custom columns
    cfg.userDatas = NULL;                                                                  // no custom user data passed to the dialog
    cfg.sidePane = NULL;                                                                   // no side panel
    cfg.sidePaneWidth = 0.0f;                                                              // side panel width (unused)

    unsigned int switchIcon;
    data = stbi_load("aux_docs/button_switch.png", &width, &height, &nrChannels, 0);
    glGenTextures(1, &switchIcon);
    glBindTexture(GL_TEXTURE_2D, switchIcon);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    // enable depth testing to ensure correct pixel rendering order in 3D space (depth buffer prevents incorrect overlaying and redrawing of objects)
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);                                // enable blending with the scene
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // use the opacity value of the model texture to blend it correctly, ensuring smooth transparency on the edges

    Light ourLight;

    Sound ourSound;

    Model bdaeModel;

    Terrain terrainModel(ourCamera, ourLight);

    AppContext appContext;
    appContext.model = &bdaeModel;
    appContext.light = &ourLight;

    glfwSetWindowUserPointer(window, &appContext);

    // game loop
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // handle keyboard input
        if (!fileDialogOpen)
            processInput(window);

        // prepare ImGui for a new frame
        // _____________________________

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // define settings panel with dynamic size and fixed position
        ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 270.0f), ImVec2(200.0f, FLT_MAX));
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_None);

        settingsPanelHovered = ImGui::GetIO().WantCaptureMouse;

        ImGui::Begin("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);

        // add a button that opens file browsing dialog
        if (ImGui::Button(isTerrainViewer ? "Load Terrain (beta)" : "Load Model"))
        {
            cfg.path = isTerrainViewer ? "./data/terrain" : cfg.path;

            IGFD::FileDialog::Instance()->OpenDialog(
                "File_Browsing_Dialog",                         // dialog ID (used to reference this dialog instance)
                isTerrainViewer ? "Load Map" : "Load 3D Model", // dialog title
                isTerrainViewer ? ".trn" : ".bdae",             // file extension filter
                cfg                                             // config
            );
        }

        ImGui::SameLine();

        // define viewer mode change button
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));

        if (ImGui::ImageButton("##viewer_mode_change_button", switchIcon, ImVec2(25, 25)))
        {
            isTerrainViewer ? terrainModel.reset() : bdaeModel.reset();
            isTerrainViewer = !isTerrainViewer;
            cfg.path = isTerrainViewer ? "./data/terrain" : "./data/model";
            ma_sound_stop(&ourSound.sound);
        }

        ImGui::PopStyleColor(3);

        // define file browsing dialog with fixed size and position in the center
        ImVec2 dialogSize(currentWindowWidth * 0.7f, currentWindowHeight * 0.6f);
        ImVec2 dialogPos((currentWindowWidth - dialogSize.x) * 0.5f, (currentWindowHeight - dialogSize.y) * 0.5f);
        ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(dialogPos, ImGuiCond_Always);

        fileDialogOpen = IGFD::FileDialog::Instance()->IsOpened("File_Browsing_Dialog");

        // if the dialog is opened with the load button, show it
        if (IGFD::FileDialog::Instance()->Display("File_Browsing_Dialog", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            // if selection is confirmed (by OK or double-click), process it
            if (IGFD::FileDialog::Instance()->IsOk())
            {
                std::map<std::string, std::string> selection = IGFD::FileDialog::Instance()->GetSelection(); // returns pairs (file name, full path)

                if (!isTerrainViewer)
                    bdaeModel.load(selection.begin()->second.c_str(), ourSound);
                else
                    terrainModel.load(selection.begin()->second.c_str(), ourSound);
            }

            cfg.path = ImGuiFileDialog::Instance()->GetCurrentPath(); // save most recent path
            IGFD::FileDialog::Instance()->Close();                    // close the dialog after handling OK or Cancel
        }

        // if a model is loaded, show its stats + checkboxes
        if (bdaeModel.modelLoaded && !isTerrainViewer)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("File:\xC2\xA0%s", bdaeModel.fileName.c_str());
            ImGui::Text("Size: %d Bytes", bdaeModel.fileSize);
            ImGui::Text("Vertices: %d", bdaeModel.vertexCount);
            ImGui::Text("Faces: %d", bdaeModel.faceCount);
            ImGui::NewLine();
            ImGui::Checkbox("Base Mesh On/Off", &displayBaseMesh);
            ImGui::Spacing();
            ImGui::Checkbox("Lighting On/Off", &ourLight.showLighting);
            ImGui::NewLine();
            ImGui::Text("Alternative colors: %d", bdaeModel.alternativeTextureCount);
            ImGui::Spacing();

            if (bdaeModel.alternativeTextureCount > 0)
            {
                ImGui::PushItemWidth(130.0f);
                ImGui::SliderInt(" Color", &bdaeModel.selectedTexture, 0, bdaeModel.alternativeTextureCount);
                ImGui::PopItemWidth();
            }

            ourSound.updateSoundUI(bdaeModel.sounds);
        }

        if (terrainModel.terrainLoaded && isTerrainViewer)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("File:\xC2\xA0%s", terrainModel.fileName.c_str());
            ImGui::Text("Size: %d Bytes", terrainModel.fileSize);
            ImGui::Text("Vertices: %d", terrainModel.vertexCount);
            ImGui::Text("Faces: %d", terrainModel.faceCount);
            ImGui::Text("3D Models: %d", terrainModel.modelCount);
            ImGui::NewLine();
            ImGui::Checkbox("Base Mesh On/Off", &displayBaseMesh);
            ImGui::Spacing();
            ImGui::Checkbox("Lighting On/Off", &ourLight.showLighting);
            ImGui::NewLine();
            ImGui::TextWrapped("Terrain: %d x %d tiles", terrainModel.tilesX, terrainModel.tilesZ);
            ImGui::Text("Position: (x, y, z)");
            ImGui::Spacing();

            ImGui::PushItemWidth(180.0f);
            ImGui::DragFloat3("##Camera Pos", &ourCamera.Position.x, 0.1f, -FLT_MAX, FLT_MAX, "%.0f");
            ImGui::PopItemWidth();

            ImGui::Text("x: min %d, max %d", (int)terrainModel.minX, (int)terrainModel.maxX);
            ImGui::Text("z: min %d, max %d", (int)terrainModel.minZ, (int)terrainModel.maxZ);

            ourSound.updateSoundUI(terrainModel.sounds);
        }

        ImGui::End();

        // _____________________________

        glClearColor(0.85f, 0.85f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the color buffer (fill the screen with a clear color) and the depth buffer; otherwise the information of the previous frame stays in these buffers

        // update dynamic shader uniforms on GPU
        glm::mat4 view = ourCamera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(ourCamera.Zoom), (float)currentWindowWidth / (float)currentWindowHeight, 0.1f, 1000.0f);

        if (!isTerrainViewer)
        {
            bdaeModel.draw(view, projection, ourCamera.Position, ourLight.showLighting, displayBaseMesh); // render model

            ourLight.draw(view, projection); // render light cube
        }
        else
            terrainModel.draw(view, projection, displayBaseMesh); // render terrain

        // render settings panel (and file browsing dialog, if open)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window); // make the contents of the back buffer (stores the completed frames) visible on the screen
        glfwPollEvents();        // if any events are triggered (like keyboard input or mouse movement events), updates the window state, and calls the corresponding functions (which we can register via callback methods)
    }

    // terminate, clearing all previously allocated resources
    glfwTerminate();
    ma_engine_uninit(&ourSound.engine);
    return 0;
}

// whenever the window size changed (by OS or user resize), this callback function executes
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// whenever the mouse uses scroll wheel, this callback function executes
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (fileDialogOpen)
        return;

    // handle mouse wheel scroll input using the Camera class function
    ourCamera.ProcessMouseScroll(yoffset);
}

// whenever the mouse moves, this callback function executes
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (fileDialogOpen || settingsPanelHovered)
        return;

    // calculate the mouse offset since the last frame
    // (xpos and ypos are the current cursor coordinates in screen space)
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
    lastX = xpos;
    lastY = ypos;

    // only rotate the mesh if the right mouse button is pressed
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        AppContext *ctx = static_cast<AppContext *>(glfwGetWindowUserPointer(window));

        ctx->model->meshYaw += xoffset * meshRotationSensitivity;
        ctx->model->meshPitch += -yoffset * meshRotationSensitivity;
        return;
    }

    // only rotate the camera if the left mouse button is pressed
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
        return;

    // skip camera rotation for the first frame to prevent a sudden jump or when
    if (firstMouse)
    {
        firstMouse = false;
        return;
    }

    // handle mouse movement input using the Camera class function
    ourCamera.ProcessMouseMovement(xoffset, yoffset);
}

// whenever a key is pressed, this callback function executes and only once, preventing continuous toggling when a key is held down (which would occur in processInput)
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    AppContext *ctx = static_cast<AppContext *>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_K:
            displayBaseMesh = !displayBaseMesh;
            break;
        case GLFW_KEY_L:
            ctx->light->showLighting = !ctx->light->showLighting;
            break;
        case GLFW_KEY_F:
        {
            isFullscreen = !isFullscreen;
            if (isFullscreen)
            {
                // switch to fullscreen mode on primary monitor
                GLFWmonitor *monitor = glfwGetPrimaryMonitor();      // main display in the system
                const GLFWvidmode *mode = glfwGetVideoMode(monitor); // video mode (info like resolution, color depth, refresh rate)
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

                currentWindowWidth = mode->width;
                currentWindowHeight = mode->height;
            }
            else
            {
                // restore to windowed mode with default position + size
                glfwSetWindowMonitor(window, NULL, DEFAULT_WINDOW_POS_X, DEFAULT_WINDOW_POS_Y, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0);

                // reset mouse to avoid camera jump
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                lastX = xpos;
                lastY = ypos;
                firstMouse = true;

                currentWindowWidth = DEFAULT_WINDOW_WIDTH;
                currentWindowHeight = DEFAULT_WINDOW_HEIGHT;
            }
        }
        break;
        }
    }
}

// process all input: query GLFW whether relevant keys are pressed / released this frame and react accordingly
void processInput(GLFWwindow *window)
{
    // Escape key to close the program
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    /*
       WASD keys for camera movement:
       W – move forward (along the camera's viewing direction vector, i.e. z-axis)
       S – move backward
       A – move left (along the right vector, i.e. x-axis; computed using the cross product)
       D – move right
    */
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        ourCamera.ProcessKeyboard(FORWARD);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        ourCamera.ProcessKeyboard(BACKWARD);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        ourCamera.ProcessKeyboard(LEFT);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        ourCamera.ProcessKeyboard(RIGHT);

    ourCamera.UpdatePosition(deltaTime);
}
