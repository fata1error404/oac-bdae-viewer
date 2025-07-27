#include <iostream>
#include <iomanip>
#include <string>
#include <filesystem>
#include "libs/glad/glad.h"                  // library for OpenGL functions loading (like glClear or glViewport)
#include "libs/glm/glm.hpp"                  // library for OpenGL style mathematics (basic vector and matrix mathematics functions)
#include "libs/glm/gtc/matrix_transform.hpp" // for matrix transformation functions
#include "libs/glm/gtc/type_ptr.hpp"         // for matrix conversion to raw pointers (OpenGL compatibility with GLM)
#include "libs/imgui/imgui.h"                // library for UI elements
#include "libs/imgui/imgui_impl_opengl3.h"   // connects Dear ImGui with OpenGL
#include "libs/imgui/imgui_impl_glfw.h"      // connects Dear ImGui with GLFW
#include "libs/imgui/ImGuiFileDialog.h"      // extension for file browsing dialog

#define STB_IMAGE_IMPLEMENTATION // define a STB_IMAGE_IMPLEMENTATION macro (to tell the compiler to include function implementations)
#include "libs/stb_image.h"      // library for image loading
#include "shader.h"              // implementation of the graphics pipeline
#include "camera.h"              // implementation of the camera system
#include "light.h"               // definition of the light settings and light cube

#ifdef __linux__
#include <GLFW/glfw3.h> // library for creating windows and handling input – mouse clicks, keyboard input, or window resizes
#elif _WIN32
#include "libs/glfw/glfw3.h"
#endif

#include "libs/io/PackPatchReader.h"
#include "resFile.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow *window);
void loadBDAEModel(const char *fpath);

// window settings
bool isFullscreen = false;
const unsigned int DEFAULT_SCR_WIDTH = 800;
const unsigned int DEFAULT_SCR_HEIGHT = 600;
const unsigned int DEFAULT_WINDOW_POS_X = 100;
const unsigned int DEFAULT_WINDOW_POS_Y = 100;

unsigned int currentScreenWidth = DEFAULT_SCR_WIDTH;
unsigned int currentScreenHeight = DEFAULT_SCR_HEIGHT;

// create a Camera class instance with a specified position and default values for other parameters, to access its functionality
Camera ourCamera;

float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f; // time of last frame

bool firstMouse = true;                  // flag to check if the mouse movement is being processed for the first time
double lastX = DEFAULT_SCR_WIDTH / 2.0;  // starting cursor position (x-axis)
double lastY = DEFAULT_SCR_HEIGHT / 2.0; // starting cursor position (y-axis)

// viewer variables
float meshPitch = 0.0f;
float meshYaw = 0.0f;
float meshRotationSensitivity = 0.3f;
bool displayBaseMesh = false;      // flag that indicates base / textured mesh display mode
bool modelLoaded = false;          // flag that indicates whether to display model info and settings
bool fileDialogOpen = false;       // flag that indicates whether to block all background inputs (when the file browsing dialog is open)
bool settingsPanelHovered = false; // flag that indicated whether to block background mouse input (when interacting with the settings panel)

std::string fileName;
int fileSize, vertexCount, faceCount, textureCount, alternativeTextureCount, selectedTexture, totalSubmeshCount;
unsigned int VAO, VBO;
std::vector<unsigned int> EBOs;
std::vector<float> vertices;
std::vector<std::vector<unsigned short>> indices;
std::vector<unsigned int> textures;
glm::vec3 meshCenter;

int main()
{
    // initialize and configure (use core profile mode and OpenGL v3.3)
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // GLFW window creation
    GLFWwindow *window = glfwCreateWindow(DEFAULT_SCR_WIDTH, DEFAULT_SCR_HEIGHT, "BDAE 3D Model Viewer", NULL, NULL);

    // set window icon
    int width, height, nrChannels;
    unsigned char *data = stbi_load("app icon.png", &width, &height, &nrChannels, 0);
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
    cfg.path = "./model";                                                                  // default path
    cfg.fileName = "";                                                                     // default file name (none)
    cfg.filePathName = "";                                                                 // default file path name (none)
    cfg.countSelectionMax = 1;                                                             // only allow to select one file
    cfg.flags = ImGuiFileDialogFlags_HideColumnType | ImGuiFileDialogFlags_HideColumnDate; // flags: hide file type and date columns
    cfg.userFileAttributes = NULL;                                                         // no custom columns
    cfg.userDatas = NULL;                                                                  // no custom user data passed to the dialog
    cfg.sidePane = NULL;                                                                   // no side panel
    cfg.sidePaneWidth = 0.0f;                                                              // side panel width (unused)

    // enable depth testing to ensure correct pixel rendering order in 3D space (depth buffer prevents incorrect overlaying and redrawing of objects)
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);                                // enable blending with the scene
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // use the opacity value of the model texture to blend it correctly, ensuring smooth transparency on the edges

    Shader ourShader("shader model.vs", "shader model.fs");

    ourShader.use();
    ourShader.setVec3("lightPos", lightPos);
    ourShader.setVec3("lightColor", lightColor);
    ourShader.setFloat("ambientStrength", ambientStrength);
    ourShader.setFloat("diffuseStrength", diffuseStrength);
    ourShader.setFloat("specularStrength", specularStrength);

    Light lightSource(ourCamera);

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

        // define settings panel with fixed size and position
        ImGui::SetNextWindowSize(ImVec2(200.0f, 270.0f), ImGuiCond_None);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_None);

        settingsPanelHovered = ImGui::GetIO().WantCaptureMouse;

        ImGui::Begin("Settings", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // add a button that opens file browsing dialog
        if (ImGui::Button("Load Model"))
        {
            IGFD::FileDialog::Instance()->OpenDialog(
                "File_Browsing_Dialog", // dialog ID (used to reference this dialog instance)
                "Load .bdae Model",     // dialog title
                ".bdae",                // file extension filter
                cfg                     // config
            );
        }

        // define file browsing dialog with fixed size and position in the center
        ImVec2 dialogSize(currentScreenWidth * 0.7f, currentScreenHeight * 0.6f);
        ImVec2 dialogPos((currentScreenWidth - dialogSize.x) * 0.5f, (currentScreenHeight - dialogSize.y) * 0.5f);
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
                loadBDAEModel(selection.begin()->second.c_str());
            }

            cfg.path = ImGuiFileDialog::Instance()->GetCurrentPath();
            IGFD::FileDialog::Instance()->Close(); // close the dialog after handling OK or Cancel
        }

        // if a model is loaded, show its stats + checkboxes
        if (modelLoaded)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("File:\xC2\xA0%s", fileName.c_str());
            ImGui::Text("Size: %d Bytes", fileSize);
            ImGui::Text("Vertices: %d", vertexCount);
            ImGui::Text("Faces: %d", faceCount);
            ImGui::NewLine();
            ImGui::Checkbox("Base Mesh On/Off", &displayBaseMesh);
            ImGui::Spacing();
            ImGui::Checkbox("Lighting On/Off", &showLighting);
            ImGui::NewLine();
            ImGui::Text("Alternative colors: %d", alternativeTextureCount);
            ImGui::Spacing();

            if (alternativeTextureCount > 0)
                ImGui::SliderInt(" Color", &selectedTexture, 0, alternativeTextureCount);
        }

        ImGui::End();

        // _____________________________

        glClearColor(0.85f, 0.85f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the color buffer (fill the screen with a clear color) and the depth buffer; otherwise the information of the previous frame stays in these buffers

        // update dynamic shader uniforms on GPU
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, meshCenter); // a trick to build the correct model matrix that rotates the mesh around its center
        model = glm::rotate(model, glm::radians(meshPitch), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(meshYaw), glm::vec3(0, 1, 0));
        model = glm::translate(model, -meshCenter);
        glm::mat4 view = ourCamera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(ourCamera.Zoom), (float)currentScreenWidth / (float)currentScreenHeight, 0.1f, 1000.0f);

        ourShader.use();
        ourShader.setMat4("model", model);
        ourShader.setMat4("view", view);
        ourShader.setMat4("projection", projection);
        ourShader.setBool("lighting", showLighting);
        ourShader.setVec3("cameraPos", ourCamera.Position);

        // render model
        glBindVertexArray(VAO);

        if (!displayBaseMesh)
        {
            ourShader.setInt("renderMode", 1);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            for (int i = 0; i < totalSubmeshCount; i++)
            {
                if (textureCount == totalSubmeshCount)
                {
                    glActiveTexture(GL_TEXTURE0); // [TODO] textures are assigned to the wrong submeshes
                    glBindTexture(GL_TEXTURE_2D, textures[i]);
                }

                if (alternativeTextureCount > 0 && textureCount == 1)
                    glBindTexture(GL_TEXTURE_2D, textures[selectedTexture]);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
                glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
            }
        }
        else
        {
            // first pass: render mesh edges (wireframe mode)
            ourShader.setInt("renderMode", 2);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            for (int i = 0; i < totalSubmeshCount; i++)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
                glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
            }

            // second pass: render mesh faces
            ourShader.setInt("renderMode", 3);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            for (int i = 0; i < totalSubmeshCount; i++)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
                glDrawElements(GL_TRIANGLES, indices[i].size(), GL_UNSIGNED_SHORT, 0);
            }
        }

        // render light cube
        lightSource.draw(view, projection);

        // render settings panel (and file browsing dialog, if open)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window); // make the contents of the back buffer (stores the completed frames) visible on the screen
        glfwPollEvents();        // if any events are triggered (like keyboard input or mouse movement events), updates the window state, and calls the corresponding functions (which we can register via callback methods)
    }

    // terminate, clearing all previously allocated GLFW resources
    glfwTerminate();
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
        meshYaw += xoffset * meshRotationSensitivity;
        meshPitch += -yoffset * meshRotationSensitivity;
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
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_K:
            displayBaseMesh = !displayBaseMesh;
            break;
        case GLFW_KEY_L:
            showLighting = !showLighting;
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

                currentScreenWidth = mode->width;
                currentScreenHeight = mode->height;
            }
            else
            {
                // restore to windowed mode with default position + size
                glfwSetWindowMonitor(window, NULL, DEFAULT_WINDOW_POS_X, DEFAULT_WINDOW_POS_Y, DEFAULT_SCR_WIDTH, DEFAULT_SCR_HEIGHT, 0);

                // reset mouse to avoid camera jump
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                lastX = xpos;
                lastY = ypos;
                firstMouse = true;

                currentScreenWidth = DEFAULT_SCR_WIDTH;
                currentScreenHeight = DEFAULT_SCR_HEIGHT;
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

// BDAE File Loading
// _________________

void loadBDAEModel(const char *fpath)
{
    // 1. clear GPU memory and reset viewer state
    if (VAO)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }

    if (VBO)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }

    if (!EBOs.empty())
    {
        glDeleteBuffers(totalSubmeshCount, EBOs.data());
        EBOs.clear();
    }

    if (!textures.empty())
    {
        glDeleteTextures(textureCount + alternativeTextureCount, textures.data());
        textures.clear();
    }

    vertices.clear();
    indices.clear();
    fileSize = vertexCount = faceCount = textureCount = alternativeTextureCount = selectedTexture = totalSubmeshCount = 0;
    std::vector<std::string> textureNames;

    // 2. load and parse the .bdae file, building the mesh vertex and index data
    CPackPatchReader *bdaeArchive = new CPackPatchReader(fpath, true, false);           // open outer .bdae archive file
    IReadResFile *bdaeFile = bdaeArchive->openFile("little_endian_not_quantized.bdae"); // open inner .bdae file

    if (bdaeFile)
    {
        File myFile;
        int result = myFile.Init(bdaeFile); // run the parser

        std::cout << "\n"
                  << (result != 1 ? "INITIALIZATION SUCCESS" : "INITIALIZATION ERROR") << std::endl;

        if (result != 1)
        {
            // std::cout << "\nRetrieving model vertex and index data, loading textures.." << std::endl;

            // retrieve the number of meshes, submeshes, and vertex count for each one
            int meshCount, meshInfoOffset;
            char *ptr = (char *)myFile.DataBuffer + 80 + 120; // points to mesh info in the Data section
            memcpy(&meshCount, ptr, sizeof(int));
            memcpy(&meshInfoOffset, ptr + 4, sizeof(int));

            std::cout << "\nMESHES: " << meshCount << std::endl;

            int meshVertexCount[meshCount], submeshCount[meshCount], meshMetadataOffset[meshCount], meshVertexDataOffset[meshCount];

            for (int i = 0; i < meshCount; i++)
            {
                memcpy(&meshMetadataOffset[i], ptr + 4 + meshInfoOffset + 20 + i * 24, sizeof(int));
                memcpy(&meshVertexCount[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 4, sizeof(int));
                memcpy(&submeshCount[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 12, sizeof(int));

                /* [TODO] parse mesh vertex data offset

                memcpy(&meshVertexDataOffset[i], ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i] + 88, sizeof(int));

                unsigned char *bytes = reinterpret_cast<unsigned char *>(ptr + 4 + meshInfoOffset + 20 + i * 24 + meshMetadataOffset[i]);

                for (int i = 0; i < 100; ++i)
                {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(bytes[i]) << " ";

                    if ((i + 1) % 16 == 0)
                        std::cout << "\n";
                }
                std::cout << std::dec << std::endl;

                */

                std::cout << "[" << i + 1 << "]  " << meshVertexCount[i] << " vertices, " << submeshCount[i] << " submeshes" << std::endl;
                totalSubmeshCount += submeshCount[i];
            }

            indices.resize(totalSubmeshCount);
            int currentSubmeshIndex = 0;

            // loop through each mesh, retrieve its vertex and index data; all vertex data is stored in a single flat vector, while index data is stored in separate vectors for each submesh
            for (int i = 0; i < meshCount; i++)
            {
                unsigned char *meshVertexDataPtr = (unsigned char *)myFile.RemovableBuffers[i + currentSubmeshIndex] + 4;
                unsigned int meshVertexDataSize = myFile.RemovableBuffersInfo[(i + currentSubmeshIndex) * 2] - 4;
                unsigned int bytesPerVertex = meshVertexDataSize / meshVertexCount[i];

                for (int j = 0; j < meshVertexCount[i]; j++)
                {
                    float vertex[8]; // each vertex has 3 position, 3 normal, and 2 texture coordinates (total of 8 float components; in fact, in the .bdae file there are more than 8 variables per vertex, that's why bytesPerVertex is more than 8 * sizeof(float))
                    memcpy(vertex, meshVertexDataPtr + j * bytesPerVertex, sizeof(vertex));

                    vertices.push_back(vertex[0]); // X
                    vertices.push_back(vertex[1]); // Y
                    vertices.push_back(vertex[2]); // Z

                    vertices.push_back(vertex[3]); // Nx
                    vertices.push_back(vertex[4]); // Ny
                    vertices.push_back(vertex[5]); // Nz

                    vertices.push_back(vertex[6]); // S
                    vertices.push_back(vertex[7]); // T
                }

                for (int k = 0; k < submeshCount[i]; k++)
                {
                    unsigned char *submeshIndexDataPtr = (unsigned char *)myFile.RemovableBuffers[i + currentSubmeshIndex + 1] + 4;
                    unsigned int submeshIndexDataSize = myFile.RemovableBuffersInfo[(i + currentSubmeshIndex) * 2 + 2] - 4;
                    unsigned int submeshTriangleCount = submeshIndexDataSize / (3 * sizeof(unsigned short));

                    for (int l = 0; l < submeshTriangleCount; l++)
                    {
                        unsigned short triangle[3];
                        memcpy(triangle, submeshIndexDataPtr + l * sizeof(triangle), sizeof(triangle));

                        indices[currentSubmeshIndex].push_back(triangle[0]);
                        indices[currentSubmeshIndex].push_back(triangle[1]);
                        indices[currentSubmeshIndex].push_back(triangle[2]);
                        faceCount++;
                    }

                    currentSubmeshIndex++;
                }
            }

            // compute the mesh's center in world space for its correct rotation (instead of always rotating around the origin (0, 0, 0))
            meshCenter = glm::vec3(0.0f);

            for (int i = 0, n = vertices.size() / 8; i < n; i++)
            {
                meshCenter.x += vertices[i * 8 + 0];
                meshCenter.y += vertices[i * 8 + 1];
                meshCenter.z += vertices[i * 8 + 2];
            }

            meshCenter /= (vertices.size() / 8);

            std::cout << "CENTER: " << meshCenter.x << "  " << meshCenter.y << "  " << meshCenter.z << std::endl;

            // search for texture names
            ptr = (char *)myFile.DataBuffer + 80 + 96;
            memcpy(&textureCount, ptr, sizeof(int));

            std::cout << "\nTEXTURES: " << ((textureCount != 0) ? std::to_string(textureCount) : "0, file name will be used as a texture name") << std::endl;

            // normalize model path for cross-platform compatibility (Windows uses '\', Linux uses '/')
            std::string modelPath(fpath);
            std::replace(modelPath.begin(), modelPath.end(), '\\', '/');

            // retrieve model subpath
            const char *subpathStart = std::strstr(modelPath.c_str(), "/model/") + 7; // subpath starts after '/model/' (texture and model files have the same subpath, e.g. 'creature/pet/')
            const char *subpathEnd = std::strrchr(modelPath.c_str(), '/') + 1;        // last '/' before the file name
            std::string textureSubpath(subpathStart, subpathEnd);

            bool isAlphaRef = false;       // for debugging textures
            bool isUnsortedFolder = false; // for 'unsorted' folder

            if (textureSubpath.rfind("unsorted/", 0) == 0)
                isUnsortedFolder = true;

            // [TODO] implement a more robust approach
            // loop through each retrieved string and find those that are texture names
            for (int i = 0, n = myFile.StringStorage.size(); i < n; i++)
            {
                std::string s = myFile.StringStorage[i];

                if (s == "alpharef")
                    isAlphaRef = true;

                // convert to lowercase
                for (char &c : s)
                    c = std::tolower(c);

                // remove 'avatar/' if it exists
                int avatarPos = s.find("avatar/");
                if (avatarPos != std::string::npos && !isUnsortedFolder)
                    s.erase(avatarPos, 7);

                // remove 'texture/' if it exists
                if (s.rfind("texture/", 0) == 0)
                    s.erase(0, 8);

                // a string is a texture file name if it ends with '.tga' and doesn't start with '_'
                if (s.length() >= 4 && s.compare(s.length() - 4, 4, ".tga") == 0 && s[0] != '_' && s.substr(0, 3) != "e:/")
                {
                    // replace the ending with '.png'
                    s.replace(s.length() - 4, 4, ".png");

                    // build final path
                    if (!isUnsortedFolder)
                        s = "texture/" + textureSubpath + s;
                    else
                        s = "texture/unsorted/" + s;

                    // ensure it is a unique texture name
                    if (std::find(textureNames.begin(), textureNames.end(), s) == textureNames.end())
                        textureNames.push_back(s);
                }
            }

            // set file info to be displayed in the settings panel
            fileName = modelPath.substr(modelPath.find_last_of("/\\") + 1); // file name is after the last path separator in the full path
            fileSize = myFile.Size;
            vertexCount = vertices.size() / 8;

            // if a texture file matching the model file name exists, override the parsed texture (for single-texture models only)
            std::string s = "texture/" + textureSubpath + fileName;
            s.replace(s.length() - 5, 5, ".png");

            if (textureCount == 1 && std::filesystem::exists(s))
            {
                textureNames.clear();
                textureNames.push_back(s);
            }

            // if a texture name is missing in the .bdae file, use this file's name instead (assuming the texture file was manually found and named)
            if (textureNames.empty())
            {
                textureNames.push_back(s);
                textureCount++;
            }

            for (int i = 0; i < textureNames.size(); i++)
                std::cout << "[" << i + 1 << "]  " << textureNames[i] << std::endl;

            // search for alternative texture files
            // [TODO] handle for multi-texture models
            if (textureNames.size() == 1 && std::filesystem::exists(textureNames[0]) && !isUnsortedFolder)
            {
                std::filesystem::path texturePath("texture/" + textureSubpath);
                std::string baseTextureName = std::filesystem::path(textureNames[0]).stem().string(); // texture file name without extension or folder (e.g. 'boar_01' or 'puppy_bear_black')

                std::string groupName; // name shared by a group of related textures

                // naming rule #1
                if (baseTextureName.find("lvl") != std::string::npos && baseTextureName.find("world") != std::string::npos)
                    groupName = baseTextureName;

                // naming rule #2
                for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
                {
                    if (!entry.is_regular_file())
                        continue;

                    std::filesystem::path entryPath = entry.path();

                    if (entryPath.extension() != ".png")
                        continue;

                    std::string baseEntryName = entryPath.stem().string();

                    if (baseEntryName.rfind(baseTextureName + '_', 0) == 0 &&                                  // starts with '<baseTextureName>_'
                        baseEntryName.size() > baseTextureName.size() + 1 &&                                   // has at least one character after the underscore
                        std::isdigit(static_cast<unsigned char>(baseEntryName[baseTextureName.size() + 1])) && // first character after '_' is a digit
                        entryPath.string() != textureNames[0])                                                 // not the original base texture itself
                    {
                        groupName = baseTextureName;
                        break;
                    }
                }

                // for a numeric suffix (e.g. '_01', '_2'), remove it if exists (to derive a group name for searching potential alternative textures, e.g 'boar')
                if (groupName.empty())
                {
                    auto lastUnderscore = baseTextureName.rfind('_');

                    if (lastUnderscore != std::string::npos)
                    {
                        std::string afterLastUnderscore = baseTextureName.substr(lastUnderscore + 1);

                        if (!afterLastUnderscore.empty() && std::all_of(afterLastUnderscore.begin(), afterLastUnderscore.end(), ::isdigit))
                            groupName = baseTextureName.substr(0, lastUnderscore);
                    }
                }

                // for a non numeric‑suffix (e.g. '_black'), use the “max‑match” approach to find the best group name
                if (groupName.empty())
                {
                    // build a list of all possible prefixes (e.g. 'puppy_black_bear', 'puppy_black', 'puppy')
                    std::vector<std::string> prefixes;
                    std::string s = baseTextureName;

                    while (true)
                    {
                        prefixes.push_back(s);
                        auto pos = s.rfind('_');

                        if (pos == std::string::npos)
                            break;

                        s.resize(pos); // remove the last '_suffix'
                    }

                    // try each prefix and find the one that gives the highest number of matching texture files
                    int bestCount = 0;

                    for (int i = 0, n = prefixes.size(); i < n; i++)
                    {
                        int count = 0;
                        std::string pref = prefixes[i];

                        // skip single-word prefixes ('puppy' cannot be a group name, otherwise puppy_wolf.png could be an alternative)
                        if (pref.find('_') == std::string::npos)
                            continue;

                        // loop through each file in the texture directory and count how many .png files start with '<pref>_'
                        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
                        {
                            if (!entry.is_regular_file())
                                continue;

                            std::filesystem::path entryPath = entry.path();

                            if (entryPath.extension() != ".png")
                                continue;

                            if (entryPath.stem().string().rfind(pref + '_', 0) == 0)
                                count++;
                        }

                        // compare and update the best count; if two prefixes match the same number of textures, prefer the longer one
                        if (count > bestCount || (count == bestCount && pref.length() > groupName.length()))
                        {
                            bestCount = count;
                            groupName = pref;
                        }
                    }
                }

                // finally, collect textures based on the best group name
                if (!groupName.empty())
                {
                    std::vector<std::string> found;

                    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(texturePath))
                    {
                        if (!entry.is_regular_file())
                            continue;

                        std::filesystem::path entryPath = entry.path();

                        if (entryPath.extension() != ".png")
                            continue;

                        // skip the file if its name doesn't exactly match the group name, and doesn’t start with the group name followed by an underscore.
                        if (!(entryPath.stem().string() == groupName || entryPath.stem().string().rfind(groupName + '_', 0) == 0))
                            continue;

                        std::string alternativeTextureName = "texture/" + textureSubpath + entryPath.filename().string();

                        // skip the original base texture (already in textureNames[0])
                        if (alternativeTextureName == textureNames[0])
                            continue;

                        // ensure it is a unique texture name
                        if (std::find(textureNames.begin(), textureNames.end(), alternativeTextureName) == textureNames.end())
                        {
                            found.push_back(alternativeTextureName);
                            alternativeTextureCount++;
                        }
                    }

                    if (!found.empty())
                    {
                        // append and report
                        textureNames.insert(textureNames.end(), found.begin(), found.end());

                        std::cout << "Found " << found.size() << " alternative(s) for '" << groupName << "':\n";

                        for (int i = 0; i < found.size(); i++)
                            std::cout << "  " << found[i] << "\n";
                    }
                    else
                        std::cout << "No alternatives found for group '" << groupName << "'\n";
                }
                else
                    std::cout << "No valid grouping name for '" << baseTextureName << "'\n";
            }

            // if (isAlphaRef)
            //     std::cout << "\nALPHAREF" << std::endl;
        }

        free(myFile.DataBuffer);
        delete[] static_cast<char *>(myFile.RemovableBuffers[0]);
        delete[] myFile.RemovableBuffers;
        delete[] myFile.RemovableBuffersInfo;
    }

    delete bdaeFile;
    delete bdaeArchive;

    // 3. setup buffers
    EBOs.resize(totalSubmeshCount);
    glGenVertexArrays(1, &VAO);                   // generate a Vertex Attribute Object to store vertex attribute configurations
    glGenBuffers(1, &VBO);                        // generate a Vertex Buffer Object to store vertex data
    glGenBuffers(totalSubmeshCount, EBOs.data()); // generate an Element Buffer Object for each submesh to store index data

    glBindVertexArray(VAO); // bind the VAO first so that subsequent VBO bindings and vertex attribute configurations are stored in it correctly

    glBindBuffer(GL_ARRAY_BUFFER, VBO);                                                              // bind the VBO
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW); // copy vertex data into the GPU buffer's memory

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0); // define the layout of the vertex data (vertex attribute configuration): index 0, 3 components per vertex, type float, not normalized, with a stride of 8 * sizeof(float) (next vertex starts after 8 floats), and an offset of 0 in the buffer
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    for (int i = 0; i < totalSubmeshCount; i++)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices[i].size() * sizeof(unsigned short), indices[i].data(), GL_STATIC_DRAW);
    }

    // 4. load texture(s)
    textures.resize(textureNames.size());
    glGenTextures(textureNames.size(), textures.data()); // generate and store texture ID(s)

    for (int i = 0; i < textureNames.size(); i++)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]); // bind the texture ID so that all upcoming texture operations affect this texture

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // for s (x) axis
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // for t (y) axis R

        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, nrChannels, format;
        unsigned char *data = stbi_load(textureNames[i].c_str(), &width, &height, &nrChannels, 0); // load the image and its parameters

        if (!data)
        {
            std::cerr << "Failed to load texture: " << textureNames[i] << "\n";
            continue;
        }

        format = (nrChannels == 4) ? GL_RGBA : GL_RGB;                                            // image format
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data); // create and store texture image inside the texture object (upload to GPU)
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
    }

    modelLoaded = true;
}
