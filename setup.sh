#!/bin/bash

# Download ImGui
if [ ! -d "imgui" ]; then
    echo "Downloading ImGui..."
    git clone https://github.com/ocornut/imgui.git
    cd imgui
    git checkout v1.90.5
    cd ..
fi

# Download gl3w
if [ ! -d "gl3w" ]; then
    echo "Downloading gl3w..."
    git clone https://github.com/skaslev/gl3w.git
    cd gl3w
    python3 gl3w_gen.py
    cd ..
fi

# Install dependencies (Ubuntu/Debian)
echo "Installing dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglew-dev \
    libopencv-dev \
    pkg-config

echo "Setup complete!"
echo "Now run: mkdir build && cd build && cmake .. && make"