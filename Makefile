TARGET = app

LIB_HEADERS = -Ilibs/oac \
			  -Ilibs/oac/base \
    		  -Ilibs/oac/framework \
			  -Ilibs/oac/io \
			  -Ilibs/oac/physics

LIB_SOURCES = libs/glad/glad.c \
			  libs/imgui/imgui.cpp \
              libs/imgui/imgui_draw.cpp \
              libs/imgui/imgui_tables.cpp \
              libs/imgui/imgui_widgets.cpp \
              libs/imgui/imgui_impl_glfw.cpp \
              libs/imgui/imgui_impl_opengl3.cpp \
			  libs/imgui/ImGuiFileDialog.cpp \
			  libs/oac/base/Mutex.cpp \
			  libs/oac/framework/OS.cpp

OS = $(shell uname -s)

ifeq ($(OS),Linux)
# Linux build
app: main.cpp resFile.cpp $(LIB_SOURCES)
	g++ main.cpp resFile.cpp $(LIB_HEADERS) $(LIB_SOURCES) -o $(TARGET) libs/oac/io/libio_linux.a -lglfw
else
# Windows build
app: main.cpp resFile.cpp $(LIB_SOURCES)
	g++ main.cpp resFile.cpp $(LIB_SOURCES) $(LIB_SOURCES) aux_docs/resource.res -o $(TARGET) libs/oac/io/libio_windows.a libs/GLFW/libglfw3.a -lgdi32
endif

clean:
	rm -f $(TARGET)