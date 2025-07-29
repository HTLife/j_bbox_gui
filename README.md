# Bounding Box Annotation Tool

A simple ImGui-based application for loading JPG images and drawing bounding boxes with mouse drag functionality.

## Features

- Load JPG images through file path input
- Draw bounding boxes by mouse drag
- Display bounding box coordinates (xmin, ymin, xmax, ymax)
- Output coordinates to terminal with button click
- Clear and redraw bounding boxes

## Setup

1. Run the setup script to install dependencies and download ImGui:
   ```bash
   ./setup.sh
   ```

2. Build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Run the application:
   ```bash
   ./bbox_gui
   ```

## Usage

1. Enter the path to a JPG image in the "Image Path" field
2. Click "Load" to load the image
3. Click and drag on the image to draw a bounding box
4. The coordinates will be displayed in the Controls window
5. Click "Print to Console" to output coordinates to terminal
6. Click "Clear" to remove the bounding box

## Dependencies

- OpenGL 3.3+
- GLFW3
- OpenCV
- ImGui (downloaded automatically by setup script)
- gl3w (downloaded automatically by setup script)