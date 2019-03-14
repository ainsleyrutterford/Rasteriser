#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include <stdint.h>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using glm::ivec2;
using glm::vec2;
using glm::vec3;
using glm::mat3;
using glm::vec4;
using glm::mat4;

SDL_Event event;

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define FULLSCREEN_MODE false

vec4 camera_position(1.0, 1.0, -3.0, 1.0);
float focal_length = 500.f;
float yaw = 0.f;
float pitch = 0.f;

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void vertex_shader(const vec4& v, ivec2& p);
void interpolate(ivec2 a, ivec2 b, vector<ivec2>& result);
void draw_line_SDL(screen* screen, ivec2 a, ivec2 b, vec3 color);
void draw_polygon_edges(screen* screen, const vector<vec4>& vertices, vec3 color);
void compute_polygon_rows(const vector<ivec2>& vertex_pixels,
                          vector<ivec2>& left_pixels,
                          vector<ivec2>& right_pixels);
void draw_rows(screen* screen, const vector<ivec2>& left_pixels,
               const vector<ivec2>& right_pixels, vec3 color);
void draw_polygon(screen* screen, const vector<vec4>& vertices, vec3 color);

int main(int argc, char* argv[]) {
  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  vector<Triangle> triangles;
  // Load Cornell Box
  LoadTestModel(triangles);

  vector<ivec2> vertexPixels(3);
  vertexPixels[0] = ivec2(10, 5);
  vertexPixels[1] = ivec2( 5,10);
  vertexPixels[2] = ivec2(15,15);
  vector<ivec2> leftPixels;
  vector<ivec2> rightPixels;
  compute_polygon_rows( vertexPixels, leftPixels, rightPixels );
  for( int row=0; row<leftPixels.size(); ++row ) {
    cout << "Start: ("
         << leftPixels[row].x << ","
         << leftPixels[row].y << "). "
         << "End: ("
         << rightPixels[row].x << ","
         << rightPixels[row].y << "). " << endl;
  }

  while (update()) {
    draw(screen, triangles);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

// Place your drawing here
void draw(screen* screen, vector<Triangle>& triangles) {
  // Clear buffer
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  float r[16] = {cos(yaw), sin(pitch)*sin(yaw),  sin(yaw)*cos(pitch), 1.0f,
                 0.0f,     cos(pitch),          -sin(pitch),          1.0f,
                -sin(yaw), cos(yaw)*sin(pitch),  cos(pitch)*cos(yaw), 1.0f,
                 1.0f,     1.0f,                 1.0f,                1.0f};
  mat4 R;
  memcpy(glm::value_ptr(R), r, sizeof(r));

  for (uint32_t i = 0; i < triangles.size(); i++) {
    vector<vec4> vertices(3);
    vertices[0] = R * triangles[i].v0;
    vertices[1] = R * triangles[i].v1;
    vertices[2] = R * triangles[i].v2;

    draw_polygon_edges(screen, vertices, triangles.at(i).color);
    draw_polygon(screen, vertices, triangles.at(i).color);
  }
}

void draw_polygon_edges(screen* screen, const vector<vec4>& vertices, vec3 color) {
  vector<ivec2> projected_vertices(vertices.size());
  for (uint i = 0; i < projected_vertices.size(); i++) {
    vertex_shader(vertices.at(i), projected_vertices.at(i));
  }
  for (uint i = 0; i < vertices.size(); i++) {
    int j = (i + 1) % vertices.size();
    draw_line_SDL(screen, projected_vertices.at(i), projected_vertices.at(j), color);
  }
}

void vertex_shader(const vec4& v, ivec2& p) {
  float X = (v.x - camera_position.x);
  float Y = (v.y - camera_position.y);
  float Z = (v.z - camera_position.z);
  float x = focal_length * X/Z + SCREEN_WIDTH /2.0f;
  float y = focal_length * Y/Z + SCREEN_HEIGHT/2.0f;
  p = ivec2(x, y);
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
  result.at(result.size() - 1) = b;
}

void compute_polygon_rows(const vector<ivec2>& vertex_pixels,
                          vector<ivec2>& left_pixels,
                          vector<ivec2>& right_pixels) {

  int min_y = numeric_limits<int>::max();
  int max_y = numeric_limits<int>::min();
  for (int i = 0; i < vertex_pixels.size(); i++) {
    if (vertex_pixels.at(i).y < min_y) min_y = vertex_pixels.at(i).y;
    if (vertex_pixels.at(i).y > max_y) max_y = vertex_pixels.at(i).y;
  }

  left_pixels.resize(max_y - min_y + 1);
  right_pixels.resize(max_y - min_y + 1);

  for (int i = 0; i < left_pixels.size(); i++) {
    left_pixels.at(i).x = numeric_limits<int>::max();
    right_pixels.at(i).x = numeric_limits<int>::min();
  }

  for (int i = 0; i < vertex_pixels.size(); i++) {
    ivec2 a = vertex_pixels.at(i);
    ivec2 b = vertex_pixels.at((i + 1) % vertex_pixels.size());
    ivec2 delta = glm::abs(a - b);
    uint pixels = glm::max(delta.x, delta.y) + 1;
    vector<ivec2> line(pixels);
    interpolate(a, b, line);
    for (uint j = 0; j < pixels; j++) {
      if (left_pixels.at(line.at(j).y - min_y).x > line.at(j).x) {
        left_pixels.at(line.at(j).y - min_y).x = line.at(j).x;
        left_pixels.at(line.at(j).y - min_y).y = line.at(j).y;
      }
      if (right_pixels.at(line.at(j).y - min_y).x < line.at(j).x) {
        right_pixels.at(line.at(j).y - min_y).x = line.at(j).x;
        right_pixels.at(line.at(j).y - min_y).y = line.at(j).y;
      }
    }
  }
}

void draw_rows(screen* screen, const vector<ivec2>& left_pixels,
               const vector<ivec2>& right_pixels, vec3 color) {
  uint N = left_pixels.size();
  for (uint y = 0; y < N; y++) {
    for (uint x = left_pixels.at(y).x; x < right_pixels.at(y).x; x++) {
      PutPixelSDL(screen, x, left_pixels.at(y).y, color);
    }
  }
}

void draw_polygon(screen* screen, const vector<vec4>& vertices, vec3 color) {
  int V = vertices.size();
  vector<ivec2> vertex_pixels(V);
  for (int i = 0; i < V; i++) vertex_shader(vertices[i], vertex_pixels[i]);
  vector<ivec2> left_pixels;
  vector<ivec2> right_pixels;
  compute_polygon_rows(vertex_pixels, left_pixels, right_pixels);
  draw_rows(screen, left_pixels, right_pixels, color);
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
          pitch -= 0.1;
          break;
        case SDLK_DOWN:
          // Move camera backwards
          pitch += 0.1;
          break;
        case SDLK_LEFT:
          // Move camera left
          yaw += 0.1;
          break;
        case SDLK_RIGHT:
          // Move camera right
          yaw -= 0.1;
          break;
        case SDLK_ESCAPE:
          // Move camera quit
          return false;
      }
    }
  }
  return true;
}
