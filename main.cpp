#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <sstream>

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
    bool loadedFromCSV = false;
    int pixelX1 = 0, pixelY1 = 0, pixelX2 = 0, pixelY2 = 0;  // Store original pixel coordinates
    ResizeHandle activeHandle = ResizeHandle::None;
};

class ImageViewer {
private:
    cv::Mat image;
    GLuint textureID = 0;
    std::string imagePath;
    std::vector<std::string> imageFiles;
    int currentImageIndex = -1;
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
    
    void SaveCSV() {
        SaveBoundingBoxToCSV();
    }
    
    void LoadCSV() {
        LoadBoundingBoxFromCSV();
    }
    
    bool LoadImage(const std::string& path) {
        std::cout << "LoadImage called with path length: " << path.length() << std::endl;
        std::cout << "LoadImage path parameter: " << path << std::endl;
        
        std::filesystem::path filePath(path);
        std::cout << "Attempting to load image: " << filePath.filename().string() << std::endl;
        
        // Check if file exists
        std::ifstream file(path);
        if (!file.good()) {
            std::cerr << "File does not exist: " << path << std::endl;
            return false;
        }
        file.close();
        
        image = cv::imread(path);
        if (image.empty()) {
            std::cerr << "Failed to load image (OpenCV could not decode): " << path << std::endl;
            return false;
        }
        
        // Debug: Check original image properties
        std::cout << "Original image - Channels: " << image.channels() << ", Type: " << image.type() << std::endl;
        
        // Convert BGR to RGB for OpenGL
        if (image.channels() == 3) {
            cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, image, cv::COLOR_BGRA2RGB);
        }
        
        // Don't flip - let's see the original image first
        // cv::flip(image, image, 0);
        
        // Ensure image data is continuous in memory
        if (!image.isContinuous()) {
            image = image.clone();
        }
        
        imagePath = path;
        
        // Scan for other images in the same directory
        ScanDirectory();
        
        // Try to load corresponding CSV file (coordinates will be calculated during first render)
        LoadBoundingBoxFromCSV();
        
        // Output image resolution
        std::cout << "Image loaded successfully: " << filePath.filename().string() << std::endl;
        std::cout << "Resolution: " << image.cols << "x" << image.rows << std::endl;
        std::cout << "Is continuous: " << image.isContinuous() << ", Step: " << image.step << std::endl;
        
        // Generate OpenGL texture
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
        
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Set pixel alignment to 1 byte to handle any row padding
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        return true;
    }
    
    void Render() {
        if (textureID == 0) return;
        
        // Get the main window size
        ImGuiIO& io = ImGui::GetIO();
        
        // Calculate image size maintaining aspect ratio
        float imageAspectRatio = (float)image.cols / (float)image.rows;
        float windowAspectRatio = io.DisplaySize.x / io.DisplaySize.y;
        
        if (imageAspectRatio > windowAspectRatio) {
            // Image is wider than window - fit to width
            imageSize.x = io.DisplaySize.x;
            imageSize.y = io.DisplaySize.x / imageAspectRatio;
        } else {
            // Image is taller than window - fit to height
            imageSize.y = io.DisplaySize.y;
            imageSize.x = io.DisplaySize.y * imageAspectRatio;
        }
        
        // Center the image in the window
        ImVec2 imageOffset;
        imageOffset.x = (io.DisplaySize.x - imageSize.x) * 0.5f;
        imageOffset.y = (io.DisplaySize.y - imageSize.y) * 0.5f;
        
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Image Viewer", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Set cursor position to center the image
        ImGui::SetCursorPos(imageOffset);
        imagePos = ImGui::GetCursorScreenPos();
        
        // Convert pixel coordinates to screen coordinates if loaded from CSV
        if (bbox.loadedFromCSV && bbox.isValid) {
            float scaleX = imageSize.x / (float)image.cols;
            float scaleY = imageSize.y / (float)image.rows;
            
            bbox.x1 = imagePos.x + bbox.pixelX1 * scaleX;
            bbox.y1 = imagePos.y + bbox.pixelY1 * scaleY;
            bbox.x2 = imagePos.x + bbox.pixelX2 * scaleX;
            bbox.y2 = imagePos.y + bbox.pixelY2 * scaleY;
            bbox.loadedFromCSV = false; // Only convert once
            
            std::cout << "Converted to screen coordinates: (" << bbox.x1 << "," << bbox.y1 << "," << bbox.x2 << "," << bbox.y2 << ")" << std::endl;
            std::cout << "Image size: " << imageSize.x << "x" << imageSize.y << ", Image pos: " << imagePos.x << "," << imagePos.y << std::endl;
            std::cout << "Scale factors: " << scaleX << "," << scaleY << std::endl;
        }
        
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
    
    void NavigateNext() {
        if (imageFiles.empty() || currentImageIndex == -1) return;
        
        int nextIndex = (currentImageIndex + 1) % imageFiles.size();
        std::filesystem::path nextPath(imageFiles[nextIndex]);
        std::cout << "Navigating to next image: " << nextPath.filename().string() << " (" << (nextIndex + 1) << "/" << imageFiles.size() << ")" << std::endl;
        bbox = BoundingBox(); // Reset bounding box for new image
        LoadImage(imageFiles[nextIndex]);
    }
    
    void NavigatePrevious() {
        if (imageFiles.empty() || currentImageIndex == -1) return;
        
        int prevIndex = (currentImageIndex - 1 + imageFiles.size()) % imageFiles.size();
        std::filesystem::path prevPath(imageFiles[prevIndex]);
        std::cout << "Navigating to previous image: " << prevPath.filename().string() << " (" << (prevIndex + 1) << "/" << imageFiles.size() << ")" << std::endl;
        bbox = BoundingBox(); // Reset bounding box for new image
        LoadImage(imageFiles[prevIndex]);
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
        if (textureID == 0 || (!bbox.isDrawing && !bbox.isValid)) {
            return;
        }
        
        // std::cout << "Drawing bounding box - isValid: " << bbox.isValid << ", isDrawing: " << bbox.isDrawing << std::endl;
        
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
        
        // Calculate YOLOv5 format: class x_center y_center width height (normalized)
        float x_center = (xmin + xmax) / 2.0f / image.cols;
        float y_center = (ymin + ymax) / 2.0f / image.rows;
        float width = (xmax - xmin) / (float)image.cols;
        float height = (ymax - ymin) / (float)image.rows;
        
        std::cout << "(Xmin, Ymin, Xmax, Ymax) = (" << xmin << ", " << ymin << ", " << xmax << ", " << ymax << ")" << std::endl;
        std::cout << "YOLOv5 format: 0 " << x_center << " " << y_center << " " << width << " " << height << std::endl;
    }
    
    void SaveBoundingBoxToCSV() {
        if (!bbox.isValid || imagePath.empty()) return;
        
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
        
        // Generate CSV file path (same as image path but with .csv extension)
        std::string csvPath = imagePath;
        size_t lastDot = csvPath.find_last_of('.');
        if (lastDot != std::string::npos) {
            csvPath = csvPath.substr(0, lastDot) + ".csv";
        } else {
            csvPath += ".csv";
        }
        
        // Write CSV file in overwrite mode
        std::ofstream csvFile(csvPath);
        if (csvFile.is_open()) {
            csvFile << "x_min,y_min,x_max,y_max" << std::endl;
            csvFile << xmin << "," << ymin << "," << xmax << "," << ymax << std::endl;
            csvFile.close();
            std::cout << "Bounding box saved to: " << csvPath << std::endl;
        } else {
            std::cerr << "Failed to save CSV file: " << csvPath << std::endl;
        }
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
    
    void ScanDirectory() {
        imageFiles.clear();
        currentImageIndex = -1;
        
        // Simple approach - extract directory from the stored member variable instead
        std::filesystem::path path(this->imagePath);  // Use the stored clean path
        std::filesystem::path directory = path.parent_path();
        
        std::cout << "Scanning for images in: " << directory.string() << std::endl;
        
        std::vector<std::string> supportedExtensions = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tga"};
        
        try {
            if (!std::filesystem::exists(directory)) {
                std::cerr << "Directory does not exist: " << directory.string() << std::endl;
                return;
            }
            
            if (!std::filesystem::is_directory(directory)) {
                std::cerr << "Path is not a directory: " << directory.string() << std::endl;
                return;
            }
            
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    
                    if (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end()) {
                        imageFiles.push_back(entry.path().string());
                    }
                }
            }
            
            std::sort(imageFiles.begin(), imageFiles.end());
            
            // Find current image in the list
            auto it = std::find(imageFiles.begin(), imageFiles.end(), this->imagePath);
            if (it != imageFiles.end()) {
                currentImageIndex = std::distance(imageFiles.begin(), it);
            }
            
            std::cout << "Found " << imageFiles.size() << " images in directory" << std::endl;
            std::cout << "Current image index: " << currentImageIndex << std::endl;
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Error scanning directory: " << ex.what() << std::endl;
            std::cerr << "Directory path: " << directory.string() << std::endl;
        }
    }
    
    void LoadBoundingBoxFromCSV() {
        if (imagePath.empty()) {
            std::cout << "No image path available for CSV loading" << std::endl;
            return;
        }
        
        // Generate CSV file path (same as image path but with .csv extension)
        std::string csvPath = imagePath;
        size_t lastDot = csvPath.find_last_of('.');
        if (lastDot != std::string::npos) {
            csvPath = csvPath.substr(0, lastDot) + ".csv";
        } else {
            csvPath += ".csv";
        }
        
        std::cout << "Current image path: " << imagePath << std::endl;
        std::cout << "Looking for CSV file: " << csvPath << std::endl;
        
        // Check if CSV file exists
        if (!std::filesystem::exists(csvPath)) {
            std::cout << "CSV file does not exist: " << csvPath << std::endl;
            return;
        }
        
        std::ifstream csvFile(csvPath);
        if (!csvFile.is_open()) {
            std::cout << "Failed to open CSV file: " << csvPath << std::endl;
            return;
        }
        
        std::cout << "Successfully opened CSV file" << std::endl;
        
        std::string line;
        bool headerSkipped = false;
        int lineCount = 0;
        
        while (std::getline(csvFile, line)) {
            lineCount++;
            std::cout << "Reading line " << lineCount << ": " << line << std::endl;
            
            // Skip header line if it contains text (not numbers)
            if (!headerSkipped) {
                headerSkipped = true;
                // Check if this line looks like a header (contains letters) or data (only numbers and commas)
                bool isHeader = false;
                for (char c : line) {
                    if (std::isalpha(c)) {
                        isHeader = true;
                        break;
                    }
                }
                if (isHeader) {
                    std::cout << "Skipping header line: " << line << std::endl;
                    continue;
                } else {
                    std::cout << "First line appears to be data, not header: " << line << std::endl;
                }
            }
            
            // Parse CSV line: x_min,y_min,x_max,y_max
            std::stringstream ss(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (std::getline(ss, token, ',')) {
                tokens.push_back(token);
            }
            
            std::cout << "Parsed " << tokens.size() << " tokens from line" << std::endl;
            for (size_t i = 0; i < tokens.size(); i++) {
                std::cout << "Token " << i << ": '" << tokens[i] << "'" << std::endl;
            }
            
            if (tokens.size() >= 4) {
                try {
                    int xmin = std::stoi(tokens[0]);
                    int ymin = std::stoi(tokens[1]);
                    int xmax = std::stoi(tokens[2]);
                    int ymax = std::stoi(tokens[3]);
                    
                    // Store pixel coordinates - screen coordinates will be calculated during render
                    bbox.pixelX1 = xmin;
                    bbox.pixelY1 = ymin;
                    bbox.pixelX2 = xmax;
                    bbox.pixelY2 = ymax;
                    bbox.isValid = true;
                    bbox.isSelected = false;
                    bbox.isDrawing = false;
                    bbox.loadedFromCSV = true;
                    bbox.activeHandle = ResizeHandle::None;
                    
                    std::cout << "Loaded bounding box from CSV: (" << xmin << "," << ymin << "," << xmax << "," << ymax << ")" << std::endl;
                    std::cout << "bbox.isValid = " << bbox.isValid << ", bbox.loadedFromCSV = " << bbox.loadedFromCSV << std::endl;
                    
                    break; // Only load the first bounding box for now
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing CSV line: " << line << " - " << e.what() << std::endl;
                }
            }
        }
        
        csvFile.close();
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
    io.IniFilename = nullptr; // Disable saving imgui.ini file
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Create image viewer
    ImageViewer viewer;
    
    // Load image from command line if provided
    if (argc > 1) {
        // Use the raw argv directly to avoid any string copying issues
        const char* rawPath = argv[1];
        std::cout << "Raw argument length: " << strlen(rawPath) << std::endl;
        
        // Create string carefully
        std::string imagePath(rawPath);
        std::cout << "String length: " << imagePath.length() << std::endl;
        std::cout << "Command line argument: " << imagePath << std::endl;
        
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
        
        // Check for 's' key press to save CSV
        static bool sPrevPressed = false;
        bool sCurrentPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        if (sCurrentPressed && !sPrevPressed) {
            viewer.SaveCSV();
        }
        sPrevPressed = sCurrentPressed;
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Check for arrow key presses using ImGui (after ImGui::NewFrame())
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            viewer.NavigatePrevious();
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            viewer.NavigateNext();
        }
        
        // Check for 'L' key press to load CSV
        if (ImGui::IsKeyPressed(ImGuiKey_L)) {
            viewer.LoadCSV();
        }
        
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