#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include <stdint.h>

using namespace std;
using glm::ivec2;
using glm::vec2;
using glm::vec3;
using glm::mat3;
using glm::vec4;
using glm::mat4;

SDL_Event event;

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 1000
#define FULLSCREEN_MODE false

vec4 camera_position(0.0, 0.0, -3.0, 1.0);
float focal_length = 500.f;

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void VertexShader( const vec4& v, ivec2& p );

void VertexShader( const vec4& v, ivec2& p )  {
  float X = (v.x - camera_position.x);
  float Y = (v.y - camera_position.y);
  float Z = (v.z - camera_position.z);
  float x = focal_length*X/Z + SCREEN_WIDTH/2.0f;
  float y = focal_length*Y/Z + SCREEN_HEIGHT/2.0f;
  p = ivec2(x, y);
}

float max(float x, float y) {
  if (x >= y) return x;
  else return y;
}

void interpolate(ivec2 a, ivec2 b, vector<ivec2>& result) {
  vec2 step = vec2(b - a) / float(max(result.size()-1, 1));
  vec2 current(a);
  for (int i = 0; i < result.size(); i++) {
    result.at(i) = current;
    current += step;
  }
}

void draw_line_SDL(screen* screen, ivec2 a, ivec2 b, vec3 color) {
  ivec2 delta = glm::abs(a - b);
  uint pixels = glm::max(delta.x, delta.y) + 1;

  vector<ivec2> line(pixels);
  interpolate(a, b, line);
  for (uint i = 0; i < line.size(); i++) {
    PutPixelSDL(screen, line.at(i).x, line.at(i).y, color);
  }
}

void draw_polygon_edges(const vector<vec4>& vertices, screen* screen) {
  vector<ivec2> projected_vertices(vertices.size());
  for (uint i = 0; i < projected_vertices.size(); i++) {
    VertexShader(vertices.at(i), projected_vertices.at(i));
  }
  for (uint i = 0; i < vertices.size(); i++) {
    int j = (i + 1) % vertices.size();
    vec3 color(0, 1, 1);
    draw_line_SDL(screen, projected_vertices.at(i), projected_vertices.at(j), color);
  }
}

int main(int argc, char* argv[]) {

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);


  vector<Triangle> triangles;
  // Load Cornell Box
  LoadTestModel(triangles);

  while (update()) {
    draw(screen, triangles);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

// Place your drawing here
void draw(screen* screen, vector<Triangle>& triangles){/* Clear buffer */
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  for( uint32_t i=0; i<triangles.size(); ++i ){
    vector<vec4> vertices(3);
    vertices[0] = triangles[i].v0;
    vertices[1] = triangles[i].v1;
    vertices[2] = triangles[i].v2;

    draw_polygon_edges(vertices, screen);
  }
}

// Place updates of parameters here
bool update() {
  // static int t = SDL_GetTicks();
  // // Compute frame time
  // int t2 = SDL_GetTicks();
  // float dt = float(t2-t);
  // t = t2;

  SDL_Event e;
  while(SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      return false;
    } else if (e.type == SDL_KEYDOWN) {
      int key_code = e.key.keysym.sym;
      switch(key_code) {
        case SDLK_UP:
          // Move camera forward
          break;
        case SDLK_DOWN:
          // Move camera backwards
          break;
        case SDLK_LEFT:
          // Move camera left
          break;
        case SDLK_RIGHT:
          // Move camera right
          break;
        case SDLK_ESCAPE:
          // Move camera quit
          return false;
      }
    }
  }
  return true;
}
