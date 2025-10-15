TARGET = app

HEADER_DIRS = -Ilibs/oac/base \
		  	  -Ilibs/oac/io \
			  -Ilibs/oac/navmesh

SOURCE_FILES = terrain.cpp \
			   parserBDAE.cpp \
			   parserTRN.cpp \
		       libs/glad/glad.c \
		  	   libs/imgui/imgui.cpp \
          	   libs/imgui/imgui_draw.cpp \
          	   libs/imgui/imgui_tables.cpp \
          	   libs/imgui/imgui_widgets.cpp \
          	   libs/imgui/imgui_impl_glfw.cpp \
          	   libs/imgui/imgui_impl_opengl3.cpp \
		  	   libs/imgui/ImGuiFileDialog.cpp \
			   libs/oac/navmesh/DetourCommon.cpp \
			   libs/oac/navmesh/DetourNavMesh.cpp \
			   libs/oac/navmesh/DetourNode.cpp \
			   libs/lib_impl.cpp

OS = $(shell uname -s)

ifeq ($(OS),Linux)
# Linux build
app: clean main.cpp $(SOURCE_FILES)
	g++ main.cpp $(HEADER_DIRS) $(SOURCE_FILES) -o $(TARGET) libs/oac/io/libio_linux.a -lglfw
else
# Windows build
app: main.cpp $(SOURCE_FILES)
	g++ main.cpp $(SOURCE_FILES) $(HEADER_DIRS) aux_docs/resource.res -o $(TARGET) libs/oac/io/libio_windows.a libs/glfw/libglfw3.a -lgdi32
endif

clean:
	rm -f $(TARGET)