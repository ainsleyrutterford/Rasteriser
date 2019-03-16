#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include "Pixel.cpp"
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
float depth_buffer[SCREEN_WIDTH][SCREEN_HEIGHT];

vec4 camera_position(1.0, 1.0, -3.0, 1.0);
float focal_length = 500.f;
float yaw = 0.f;
float pitch = 0.f;

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void vertex_shader(const vec4& v, Pixel& p);
void interpolate(Pixel a, Pixel b, vector<Pixel>& result);
void interpolate(ivec2 a, ivec2 b, vector<ivec2>& result);
void draw_line_SDL(screen* screen, Pixel a, Pixel b, vec3 color);
void draw_polygon_edges(screen* screen, const vector<vec4>& vertices, vec3 color);
void compute_polygon_rows(const vector<Pixel>& vertex_pixels,
                          vector<Pixel>& left_pixels,
                          vector<Pixel>& right_pixels);
void draw_rows(screen* screen, const vector<Pixel>& left_pixels,
               const vector<Pixel>& right_pixels, vec3 color);
void draw_polygon(screen* screen, const vector<vec4>& vertices, vec3 color);

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
void draw(screen* screen, vector<Triangle>& triangles) {
  // Clear screen buffer
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  // Clear depth_buffer
  memset(depth_buffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(float));

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

    // draw_polygon_edges(screen, vertices, triangles.at(i).color);
    draw_polygon(screen, vertices, triangles.at(i).color);
  }
}

void draw_polygon_edges(screen* screen, const vector<vec4>& vertices, vec3 color) {
  printf("dpe\n");
  vector<Pixel> projected_vertices(vertices.size());

  for (uint i = 0; i < projected_vertices.size(); i++) {
    vertex_shader(vertices.at(i), projected_vertices.at(i));
  }

  for (uint i = 0; i < vertices.size(); i++) {
    int j = (i + 1) % vertices.size();
    draw_line_SDL(screen, projected_vertices.at(i), projected_vertices.at(j), color);
  }
  printf("dpe f\n");

}

void vertex_shader(const vec4& v, Pixel& p) {
  printf("vs\n");

  float X = (v.x - camera_position.x);
  float Y = (v.y - camera_position.y);
  float Z = (v.z - camera_position.z);
  float x = focal_length * X/Z + SCREEN_WIDTH /2.0f;
  float y = focal_length * Y/Z + SCREEN_HEIGHT/2.0f;
  float z_inv = 1/Z;
  p = Pixel(int(x), int(y), z_inv);
  printf("vs f\n");

}

void draw_line_SDL(screen* screen, Pixel a, Pixel b, vec3 color) {
  printf("dlsdl\n");


  Pixel delta = Pixel::abs(a - b);
  uint pixels = glm::max(delta.x, delta.y) + 1;

  vector<Pixel> line(pixels);
  interpolate(a, b, line);
  for (uint i = 0; i < line.size(); i++) {
    PutPixelSDL(screen, line.at(i).x, line.at(i).y, color);
  }
  printf("dlsdl f\n");

}

void interpolate(ivec2 a, ivec2 b, vector<ivec2>& result) {
  printf("i \n");

  vec2 step = vec2(b - a) / float(fmax(result.size()-1, 1));
  vec2 current(a);
  for (uint i = 0; i < result.size(); i++) {
    result.at(i) = round(current);
    current += step;
  }
  printf("i f\n");

}

void interpolate(Pixel a_pixel, Pixel b_pixel, vector<Pixel>& result) {
  printf("i2 \n");

  vec3 a = (vec3)a_pixel;
  vec3 b = (vec3)b_pixel;

  vec3 step = vec3(b - a) / float(fmax(result.size()-1, 1));
  vec3 current(a);
  for (uint i = 0; i < result.size(); i++) {
    result.at(i) = Pixel(current);
    current += step;
  }
  printf("i2 f \n");

}

void interpolate(Pixel a_pixel, Pixel b_pixel, vector<float>& result) {
  printf("i3 \n");

  float a = 1/a_pixel.z_inv;
  float b = 1/b_pixel.z_inv;

  float step = (b - a) / float(fmax(result.size()-1, 1));
  float current = a;
  for (uint i = 0; i < result.size(); i++) {
    result.at(i) = 1/current;
    current += step;
  }
  printf("i3 f \n");

}

void compute_polygon_rows(const vector<Pixel>& vertex_pixels,
                          vector<Pixel>& left_pixels,
                          vector<Pixel>& right_pixels) {
  printf("cpr \n");

  int min_y = numeric_limits<int>::max();
  int max_y = numeric_limits<int>::min();
  for (uint i = 0; i < vertex_pixels.size(); i++) {
    if (vertex_pixels.at(i).y < min_y) min_y = vertex_pixels.at(i).y;
    if (vertex_pixels.at(i).y > max_y) max_y = vertex_pixels.at(i).y;
  }

  left_pixels.resize(max_y - min_y + 1);
  right_pixels.resize(max_y - min_y + 1);
  printf("cpr 1 \n");

  for (uint i = 0; i < left_pixels.size(); i++) {
    left_pixels.at(i).x = numeric_limits<int>::max();
    right_pixels.at(i).x = numeric_limits<int>::min();
  }
  printf("cpr 2 \n");

  for (uint i = 0; i < vertex_pixels.size(); i++) {
    Pixel a = vertex_pixels.at(i);
    Pixel b = vertex_pixels.at((i + 1) % vertex_pixels.size());

    Pixel delta = Pixel::abs(a-b);

    uint pixels = glm::max(delta.x, delta.y) + 1;
    printf("cpr 3 \n");

    vector<Pixel> line(pixels, Pixel(0, 0, 0.0f));

    printf("cpr 4 \n");


    interpolate(a, b, line);
    for (uint j = 0; j < pixels; j++) {
      int row = line.at(j).y - min_y;
      if (left_pixels.at(row).x > line.at(j).x) {
        left_pixels.at(row).x = line.at(j).x;
        left_pixels.at(row).y = line.at(j).y;
        left_pixels.at(row).z_inv = line.at(j).z_inv;
      }
      if (right_pixels.at(row).x < line.at(j).x) {
        right_pixels.at(row).x = line.at(j).x;
        right_pixels.at(row).y = line.at(j).y;
        right_pixels.at(row).z_inv = line.at(j).z_inv;
      }
    }
  }
  printf("cpr f \n");

}

void draw_rows(screen* screen, const vector<Pixel>& left_pixels,
               const vector<Pixel>& right_pixels, vec3 color) {
  uint N = left_pixels.size();

  // Iterate through each row
  for (uint row = 0; row < N; row++) {

    // Calculate the depths of the pixels in this row
    vector<float> inverses(right_pixels.at(row).x - left_pixels.at(row).x + 1);
    interpolate(left_pixels.at(row), right_pixels.at(row), inverses);

    // Iterate through each pixel in this row
    for (int x = left_pixels.at(row).x; x < right_pixels.at(row).x; x++) {
      int depth_index = x - left_pixels.at(row).x;
      int y = left_pixels.at(row).y;
      // If the pixel is closer than any before, draw it.
      if (inverses.at(depth_index) > depth_buffer[x][y]) {
        PutPixelSDL(screen, x, y, color);
        depth_buffer[x][y] = inverses.at(depth_index);
      }
    }
  }
}

void draw_polygon(screen* screen, const vector<vec4>& vertices, vec3 color) {
  uint V = vertices.size();
  vector<Pixel> vertex_pixels(V);
  for (uint i = 0; i < V; i++) vertex_shader(vertices[i], vertex_pixels[i]);
  vector<Pixel> left_pixels;
  vector<Pixel> right_pixels;
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
