// ImGui SDL2 binding with OpenGL
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#ifndef IMGUI_IMPL_SDL_H
#define IMGUI_IMPL_SDL_H

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_opengl.h>
#include <imgui.h>

IMGUI_API void        ImGui_ImplSdl_RenderDrawLists(ImDrawData* draw_data);
IMGUI_API bool        ImGui_ImplSdlGL2_Init(SDL_Window* window);
IMGUI_API void        ImGui_ImplSdlGL2_Shutdown();
IMGUI_API void        ImGui_ImplSdlGL2_NewFrame(SDL_Window* window);
IMGUI_API ImGuiKey    ImGui_ImplSDL2_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode);
IMGUI_API void        ImGui_ImplSDL2_UpdateKeyModifiers(SDL_Keymod sdl_key_mods);
IMGUI_API bool        ImGui_ImplSdlGL2_ProcessEvent(SDL_Event* event);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        ImGui_ImplSdlGL2_InvalidateDeviceObjects();
IMGUI_API bool        ImGui_ImplSdlGL2_CreateDeviceObjects();

#endif
