#include <iostream>
#include <string>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <opencv2/opencv.hpp>

struct BoundingBox {
    float x1, y1, x2, y2;
    bool isDrawing = false;
    bool isValid = false;
};

class ImageViewer {
private:
    cv::Mat image;
    GLuint textureID = 0;
    std::string imagePath;
    BoundingBox bbox;
    ImVec2 imagePos;
    ImVec2 imageSize;
    
public:
    ImageViewer() = default;
    
    ~ImageViewer() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
    }
    
    bool LoadImage(const std::string& path) {
        image = cv::imread(path);
        if (image.empty()) {
            std::cerr << "Failed to load image: " << path << std::endl;
            return false;
        }
        
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        imagePath = path;
        
        // Generate OpenGL texture
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
        
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);
        
        return true;
    }
    
    void Render() {
        if (textureID == 0) return;
        
        // Get the main window size
        ImGuiIO& io = ImGui::GetIO();
        
        // Set image size to fill the entire window
        imageSize.x = io.DisplaySize.x;
        imageSize.y = io.DisplaySize.y;
        
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Image Viewer", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        imagePos = ImGui::GetCursorScreenPos();
        
        // Display image filling the entire window
        ImGui::Image((void*)(intptr_t)textureID, imageSize);
        
        // Handle mouse input for bounding box
        HandleMouseInput();
        
        // Draw bounding box overlay
        DrawBoundingBox();
        
        ImGui::End();
    }
    
private:
    void HandleMouseInput() {
        if (textureID == 0) return;
        
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        
        // Check if mouse is over the image
        bool isOverImage = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + imageSize.x &&
                           mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + imageSize.y);
        
        if (isOverImage) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Start drawing bounding box
                bbox.x1 = mousePos.x;
                bbox.y1 = mousePos.y;
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
                bbox.isDrawing = true;
                bbox.isValid = false;
            }
            
            if (bbox.isDrawing && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Update bounding box
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
            }
            
            if (bbox.isDrawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                // Finish drawing bounding box
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
                bbox.isDrawing = false;
                bbox.isValid = true;
                
                // Output coordinates to terminal
                OutputBoundingBox();
            }
        }
    }
    
    void DrawBoundingBox() {
        if (textureID == 0 || (!bbox.isDrawing && !bbox.isValid)) return;
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw bounding box rectangle
        ImVec2 p1(std::min(bbox.x1, bbox.x2), std::min(bbox.y1, bbox.y2));
        ImVec2 p2(std::max(bbox.x1, bbox.x2), std::max(bbox.y1, bbox.y2));
        
        // Clamp to image bounds
        p1.x = std::max(p1.x, imagePos.x);
        p1.y = std::max(p1.y, imagePos.y);
        p2.x = std::min(p2.x, imagePos.x + imageSize.x);
        p2.y = std::min(p2.y, imagePos.y + imageSize.y);
        
        ImU32 color = bbox.isDrawing ? IM_COL32(255, 255, 0, 128) : IM_COL32(255, 0, 0, 128);
        drawList->AddRect(p1, p2, color, 0.0f, 0, 2.0f);
        drawList->AddRectFilled(p1, p2, IM_COL32(255, 255, 255, 20));
    }
    
    void OutputBoundingBox() {
        if (!bbox.isValid) return;
        
        // Convert screen coordinates to image coordinates
        float scaleX = (float)image.cols / imageSize.x;
        float scaleY = (float)image.rows / imageSize.y;
        
        int xmin = (int)((std::min(bbox.x1, bbox.x2) - imagePos.x) * scaleX);
        int ymin = (int)((std::min(bbox.y1, bbox.y2) - imagePos.y) * scaleY);
        int xmax = (int)((std::max(bbox.x1, bbox.x2) - imagePos.x) * scaleX);
        int ymax = (int)((std::max(bbox.y1, bbox.y2) - imagePos.y) * scaleY);
        
        // Clamp to image bounds
        xmin = std::max(0, std::min(xmin, image.cols));
        ymin = std::max(0, std::min(ymin, image.rows));
        xmax = std::max(0, std::min(xmax, image.cols));
        ymax = std::max(0, std::min(ymax, image.rows));
        
        std::cout << xmin << "," << ymin << "," << xmax << "," << ymax << std::endl;
    }
};

int main(int argc, char* argv[]) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // Setup GLFW window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Bounding Box Annotation Tool", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize OpenGL loader
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize OpenGL loader" << std::endl;
        return -1;
    }
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Create image viewer
    ImageViewer viewer;
    
    // Load image from command line if provided
    if (argc > 1) {
        std::string imagePath = argv[1];
        
        // Convert relative path to absolute path if needed
        if (imagePath[0] != '/') {
            char* cwd = getcwd(nullptr, 0);
            if (cwd != nullptr) {
                imagePath = std::string(cwd) + "/" + imagePath;
                free(cwd);
            }
        }
        
        if (!viewer.LoadImage(imagePath)) {
            std::cerr << "Failed to load image: " << imagePath << std::endl;
        }
    }
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render image viewer
        viewer.Render();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}