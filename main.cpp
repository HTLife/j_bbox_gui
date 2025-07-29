#include <iostream>
#include <string>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <opencv2/opencv.hpp>

enum class ResizeHandle {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Top,
    Bottom,
    Left,
    Right
};

struct BoundingBox {
    float x1, y1, x2, y2;
    bool isDrawing = false;
    bool isValid = false;
    bool isSelected = false;
    ResizeHandle activeHandle = ResizeHandle::None;
};

class ImageViewer {
private:
    cv::Mat image;
    GLuint textureID = 0;
    std::string imagePath;
    BoundingBox bbox;
    ImVec2 imagePos;
    ImVec2 imageSize;
    ResizeHandle hoveredHandle = ResizeHandle::None;
    
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
        
        // Output image resolution
        std::cout << "Image loaded: " << path << std::endl;
        std::cout << "Resolution: " << image.cols << "x" << image.rows << std::endl;
        
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
        
        // Update hovered handle and set appropriate cursor
        UpdateHoveredHandle();
        SetCursorForHandle();
        
        // Handle mouse input for bounding box
        HandleMouseInput();
        
        // Draw bounding box overlay
        DrawBoundingBox();
        
        // Draw crosshair lines
        DrawCrosshair();
        
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
                if (bbox.isValid) {
                    // Check if clicking on resize handles
                    ResizeHandle handle = GetResizeHandle(mousePos);
                    if (handle != ResizeHandle::None) {
                        bbox.activeHandle = handle;
                        bbox.isSelected = true;
                        return;
                    }
                    
                    // Check if clicking inside existing bbox
                    if (IsPointInBoundingBox(mousePos)) {
                        bbox.isSelected = true;
                        return;
                    }
                }
                
                // Start drawing new bounding box
                bbox.x1 = mousePos.x;
                bbox.y1 = mousePos.y;
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
                bbox.isDrawing = true;
                bbox.isValid = false;
                bbox.isSelected = false;
                bbox.activeHandle = ResizeHandle::None;
            }
            
            if (bbox.isDrawing && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Update bounding box
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
            }
            
            if (bbox.activeHandle != ResizeHandle::None && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Resize existing bounding box
                ResizeBoundingBox(mousePos);
            }
            
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (bbox.isDrawing) {
                    // Finish drawing bounding box
                    bbox.x2 = mousePos.x;
                    bbox.y2 = mousePos.y;
                    bbox.isDrawing = false;
                    bbox.isValid = true;
                    bbox.isSelected = true;
                    
                    // Output coordinates to terminal
                    OutputBoundingBox();
                }
                
                if (bbox.activeHandle != ResizeHandle::None) {
                    bbox.activeHandle = ResizeHandle::None;
                    // Output coordinates to terminal
                    OutputBoundingBox();
                }
            }
        } else {
            // Click outside image, deselect bbox
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                bbox.isSelected = false;
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
        
        ImU32 color = bbox.isDrawing ? IM_COL32(255, 255, 0, 128) : 
                     (bbox.isSelected ? IM_COL32(0, 255, 0, 128) : IM_COL32(255, 0, 0, 128));
        drawList->AddRect(p1, p2, color, 0.0f, 0, 2.0f);
        drawList->AddRectFilled(p1, p2, IM_COL32(255, 255, 255, 20));
        
        // Draw resize handles when selected
        if (bbox.isSelected && bbox.isValid) {
            DrawResizeHandles(drawList, p1, p2);
        }
        
        // Highlight hovered edge
        if (hoveredHandle != ResizeHandle::None && bbox.isValid) {
            DrawHighlightedEdge(drawList, p1, p2, hoveredHandle);
        }
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
        
        std::cout << "(Xmin, Ymin, Xmax, Ymax) = (" << xmin << ", " << ymin << ", " << xmax << ", " << ymax << ")" << std::endl;
    }
    
    bool IsPointInBoundingBox(ImVec2 point) {
        float minX = std::min(bbox.x1, bbox.x2);
        float maxX = std::max(bbox.x1, bbox.x2);
        float minY = std::min(bbox.y1, bbox.y2);
        float maxY = std::max(bbox.y1, bbox.y2);
        
        return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
    }
    
    ResizeHandle GetResizeHandle(ImVec2 point) {
        if (!bbox.isValid) return ResizeHandle::None;
        
        float minX = std::min(bbox.x1, bbox.x2);
        float maxX = std::max(bbox.x1, bbox.x2);
        float minY = std::min(bbox.y1, bbox.y2);
        float maxY = std::max(bbox.y1, bbox.y2);
        
        const float cornerSize = 12.0f;
        const float edgeSize = 6.0f;
        
        // Check corner handles first (always detect corners)
        if (std::abs(point.x - minX) <= cornerSize && std::abs(point.y - minY) <= cornerSize)
            return ResizeHandle::TopLeft;
        if (std::abs(point.x - maxX) <= cornerSize && std::abs(point.y - minY) <= cornerSize)
            return ResizeHandle::TopRight;
        if (std::abs(point.x - minX) <= cornerSize && std::abs(point.y - maxY) <= cornerSize)
            return ResizeHandle::BottomLeft;
        if (std::abs(point.x - maxX) <= cornerSize && std::abs(point.y - maxY) <= cornerSize)
            return ResizeHandle::BottomRight;
        
        // Check edge handles (only when selected)
        if (bbox.isSelected) {
            if (std::abs(point.y - minY) <= edgeSize && point.x > minX + cornerSize && point.x < maxX - cornerSize)
                return ResizeHandle::Top;
            if (std::abs(point.y - maxY) <= edgeSize && point.x > minX + cornerSize && point.x < maxX - cornerSize)
                return ResizeHandle::Bottom;
            if (std::abs(point.x - minX) <= edgeSize && point.y > minY + cornerSize && point.y < maxY - cornerSize)
                return ResizeHandle::Left;
            if (std::abs(point.x - maxX) <= edgeSize && point.y > minY + cornerSize && point.y < maxY - cornerSize)
                return ResizeHandle::Right;
        }
        
        return ResizeHandle::None;
    }
    
    void ResizeBoundingBox(ImVec2 mousePos) {
        switch (bbox.activeHandle) {
            case ResizeHandle::TopLeft:
                bbox.x1 = mousePos.x;
                bbox.y1 = mousePos.y;
                break;
            case ResizeHandle::TopRight:
                bbox.x2 = mousePos.x;
                bbox.y1 = mousePos.y;
                break;
            case ResizeHandle::BottomLeft:
                bbox.x1 = mousePos.x;
                bbox.y2 = mousePos.y;
                break;
            case ResizeHandle::BottomRight:
                bbox.x2 = mousePos.x;
                bbox.y2 = mousePos.y;
                break;
            case ResizeHandle::Top:
                bbox.y1 = mousePos.y;
                break;
            case ResizeHandle::Bottom:
                bbox.y2 = mousePos.y;
                break;
            case ResizeHandle::Left:
                bbox.x1 = mousePos.x;
                break;
            case ResizeHandle::Right:
                bbox.x2 = mousePos.x;
                break;
            default:
                break;
        }
    }
    
    void DrawResizeHandles(ImDrawList* drawList, ImVec2 p1, ImVec2 p2) {
        const float handleSize = 8.0f;
        ImU32 handleColor = IM_COL32(255, 255, 255, 255);
        
        // Corner handles
        drawList->AddRectFilled(
            ImVec2(p1.x - handleSize/2, p1.y - handleSize/2),
            ImVec2(p1.x + handleSize/2, p1.y + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(p2.x - handleSize/2, p1.y - handleSize/2),
            ImVec2(p2.x + handleSize/2, p1.y + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(p1.x - handleSize/2, p2.y - handleSize/2),
            ImVec2(p1.x + handleSize/2, p2.y + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(p2.x - handleSize/2, p2.y - handleSize/2),
            ImVec2(p2.x + handleSize/2, p2.y + handleSize/2),
            handleColor
        );
        
        // Edge handles
        float midX = (p1.x + p2.x) / 2;
        float midY = (p1.y + p2.y) / 2;
        
        drawList->AddRectFilled(
            ImVec2(midX - handleSize/2, p1.y - handleSize/2),
            ImVec2(midX + handleSize/2, p1.y + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(midX - handleSize/2, p2.y - handleSize/2),
            ImVec2(midX + handleSize/2, p2.y + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(p1.x - handleSize/2, midY - handleSize/2),
            ImVec2(p1.x + handleSize/2, midY + handleSize/2),
            handleColor
        );
        drawList->AddRectFilled(
            ImVec2(p2.x - handleSize/2, midY - handleSize/2),
            ImVec2(p2.x + handleSize/2, midY + handleSize/2),
            handleColor
        );
    }
    
    void UpdateHoveredHandle() {
        if (!bbox.isValid) {
            hoveredHandle = ResizeHandle::None;
            return;
        }
        
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        
        // Check if mouse is over the image
        bool isOverImage = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + imageSize.x &&
                           mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + imageSize.y);
        
        if (isOverImage) {
            hoveredHandle = GetResizeHandle(mousePos);
        } else {
            hoveredHandle = ResizeHandle::None;
        }
    }
    
    void SetCursorForHandle() {
        switch (hoveredHandle) {
            case ResizeHandle::TopLeft:
            case ResizeHandle::TopRight:
            case ResizeHandle::BottomLeft:
            case ResizeHandle::BottomRight:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                break;
            case ResizeHandle::Top:
            case ResizeHandle::Bottom:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                break;
            case ResizeHandle::Left:
            case ResizeHandle::Right:
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                break;
            default:
                // Check if hovering over image for crosshair
                ImGuiIO& io = ImGui::GetIO();
                ImVec2 mousePos = io.MousePos;
                bool isOverImage = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + imageSize.x &&
                                   mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + imageSize.y);
                if (isOverImage) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                }
                break;
        }
    }
    
    void DrawHighlightedEdge(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, ResizeHandle handle) {
        ImU32 highlightColor = IM_COL32(255, 255, 0, 255);
        float thickness = 3.0f;
        
        switch (handle) {
            case ResizeHandle::Top:
                drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p1.y), highlightColor, thickness);
                break;
            case ResizeHandle::Bottom:
                drawList->AddLine(ImVec2(p1.x, p2.y), ImVec2(p2.x, p2.y), highlightColor, thickness);
                break;
            case ResizeHandle::Left:
                drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p1.x, p2.y), highlightColor, thickness);
                break;
            case ResizeHandle::Right:
                drawList->AddLine(ImVec2(p2.x, p1.y), ImVec2(p2.x, p2.y), highlightColor, thickness);
                break;
            // Don't highlight corner edges, just change cursor
            case ResizeHandle::TopLeft:
            case ResizeHandle::TopRight:
            case ResizeHandle::BottomLeft:
            case ResizeHandle::BottomRight:
            default:
                break;
        }
    }
    
    void DrawCrosshair() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        
        // Check if mouse is over the image
        bool isOverImage = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + imageSize.x &&
                           mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + imageSize.y);
        
        if (isOverImage) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Draw horizontal line across entire window that follows mouse
            drawList->AddLine(
                ImVec2(0, mousePos.y), 
                ImVec2(io.DisplaySize.x, mousePos.y), 
                IM_COL32(255, 255, 255, 128), 
                1.0f
            );
            
            // Draw vertical line across entire window that follows mouse
            drawList->AddLine(
                ImVec2(mousePos.x, 0), 
                ImVec2(mousePos.x, io.DisplaySize.y), 
                IM_COL32(255, 255, 255, 128), 
                1.0f
            );
        }
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
        
        // Check for 'q' key press to exit
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        
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