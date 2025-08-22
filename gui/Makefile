# ---------------------------------------------------------
# A minimal Makefile example for building an ImGui + SDL2 + OpenGL + GLEW project.
# Adjust IMGUI_DIR, CXXFLAGS, LIBS, etc. as needed.
# ---------------------------------------------------------

# Compiler and flags
CXX      := g++
CXXFLAGS := -Wall -O3 -Wextra -std=c++17 -g

# If ImGui is located in a subdirectory named 'imgui', set:
IMGUI_DIR := ./imgui

# We'll define IMGUI_IMPL_OPENGL_LOADER_GLEW so that ImGui's OpenGL3 backend uses GLEW
CXXFLAGS += -DIMGUI_IMPL_OPENGL_LOADER_GLEW

# Include paths:
#  -I$(IMGUI_DIR) for the core imgui files
#  -I$(IMGUI_DIR)/backends for backends
CXXFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMGUI_DIR)/misc/cpp

# Libraries:
#   -lSDL2 for SDL2
#   -lGLEW  for GLEW
#   -lGL    (on Linux) or -lopengl32 (on Windows)
LIBS := -O2 -DGUI -lSDL2 -ldl -pthread -lGLEW -framework OpenGL

# Name of your final executable:
TARGET := rv32i-gui

# Your main source file (the example from before, for instance):
SOURCES := rv32i-gui.cc

# ImGui core sources:
IMGUI_SOURCES := $(IMGUI_DIR)/imgui.cpp \
                 $(IMGUI_DIR)/imgui_draw.cpp \
                 $(IMGUI_DIR)/imgui_tables.cpp \
                 $(IMGUI_DIR)/imgui_widgets.cpp \
                 $(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp

# ImGui backends for SDL2 + OpenGL3:
IMGUI_BACKENDS := $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
                  $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# Combine everything into one list of sources:
ALL_SOURCES := $(SOURCES) $(IMGUI_SOURCES) $(IMGUI_BACKENDS)

# ---------------------------------------------------------
# Build Rules
# ---------------------------------------------------------
all: $(TARGET)

$(TARGET): $(ALL_SOURCES)
	$(CXX) -I/opt/homebrew/Cellar/sdl2/2.32.8/include/SDL2 -I/opt/homebrew/Cellar/glew/2.2.0_1/include $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)
