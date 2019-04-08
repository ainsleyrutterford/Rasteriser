#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include "Pixel.cpp"
#include <stdint.h>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>

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

int indent_buffer = 0;

struct Vertex {
  vec4 position;
};

float depth_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec4  camera_position(0.f, 0.f, -3.f, 1.f);
float focal_length = 500.f;
float yaw = 0.f;
float pitch = 0.f;

vec4 light_position(0.f, -0.5f, -0.7f, 1.f);
vec3 light_power = 18.f * vec3(1.f, 1.f, 1.f);
vec3 indirect_power_per_area = 0.5f * vec3(1.f, 1.f, 1.f);

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void vertex_shader(const Vertex& v, Pixel& p);
void pixel_shader(screen* screen, const Pixel& p, vec4 current_normal, vec3 current_reflectance);
void interpolate(Pixel a, Pixel b, vector<Pixel>& result);
void interpolate(ivec2 a, ivec2 b, vector<ivec2>& result);
void draw_line_SDL(screen* screen, Pixel a, Pixel b);
void compute_polygon_rows(const vector<Pixel>& vertex_pixels, vector<Pixel>& left_pixels, vector<Pixel>& right_pixels);
void draw_rows(screen* screen, const vector<Pixel>& left_pixels, const vector<Pixel>& right_pixels, vec4 current_normal, vec3 current_reflectance);
void draw_polygon(screen* screen, const vector<Vertex>& vertices, vec4 current_normal, vec3 current_reflectance );
void print_buf();
void inc_buf();
void dec_buf();

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

void draw(screen* screen, vector<Triangle>& triangles) {
  // Clear screen buffer
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  // Clear depth_buffer
  memset(depth_buffer, 0, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(float));

  float r[16] = {cos(yaw), sin(pitch)*sin(yaw),  sin(yaw)*cos(pitch), 1.0f,
                 0.0f,     cos(pitch),          -sin(pitch),          1.0f,
                -sin(yaw), cos(yaw)*sin(pitch),  cos(pitch)*cos(yaw), 1.0f,
                 1.0f,     1.0f,                 1.0f,                1.0f};
  mat4 R;
  memcpy(glm::value_ptr(R), r, sizeof(r));

  vec4 current_normal;
  vec3 current_reflectance;

  for (uint32_t i = 0; i < triangles.size(); i++) {
    vector<Vertex> vertices(3);
    vertices[0].position = triangles[i].v0;
    vertices[1].position = triangles[i].v1;
    vertices[2].position = triangles[i].v2;

    current_normal      = triangles[i].normal;
    current_reflectance = triangles[i].color;

    draw_polygon(screen, vertices, current_normal, current_reflectance);
  }
}

void draw_polygon(screen* screen, const vector<Vertex>& vertices, vec4 current_normal, vec3 current_reflectance ) {
  // print_buf(); printf("draw_polygon start\n"); inc_buf(); //debugprint
  uint V = vertices.size();
  vector<Pixel> vertex_pixels(V);
  for (uint i = 0; i < V; i++) {
    vertex_shader(vertices[i], vertex_pixels[i]);
  }
  vector<Pixel> left_pixels;
  vector<Pixel> right_pixels;
  compute_polygon_rows(vertex_pixels, left_pixels, right_pixels);
  draw_rows(screen, left_pixels, right_pixels, current_normal, current_reflectance);
  // dec_buf(); print_buf(); printf("draw_polygon end\n"); //debugprint
}

void compute_polygon_rows(const vector<Pixel>& vertex_pixels,
                          vector<Pixel>& left_pixels,
                          vector<Pixel>& right_pixels) {
  // print_buf(); printf("compute_polygon_rows start\n"); inc_buf(); //debugprint
  int min_y = numeric_limits<int>::max();
  int max_y = numeric_limits<int>::min();
  for (uint i = 0; i < vertex_pixels.size(); i++) {
    if (vertex_pixels.at(i).y < min_y) min_y = vertex_pixels.at(i).y;
    if (vertex_pixels.at(i).y > max_y) max_y = vertex_pixels.at(i).y;
  }

  left_pixels.resize(max_y - min_y + 1);
  right_pixels.resize(max_y - min_y + 1);

  for (uint i = 0; i < left_pixels.size(); i++) {
    left_pixels.at(i).x = numeric_limits<int>::max();
    right_pixels.at(i).x = numeric_limits<int>::min();
  }

  for (uint i = 0; i < vertex_pixels.size(); i++) {
    Pixel a = vertex_pixels.at(i);
    Pixel b = vertex_pixels.at((i + 1) % vertex_pixels.size());

    Pixel delta = Pixel::abs(a-b);
    uint pixels = glm::max(delta.x, delta.y) + 1;

    vector<Pixel> line(pixels);
    interpolate(a, b, line);

    for (uint j = 0; j < line.size(); j++) {
      int row = line.at(j).y - min_y;

      if (left_pixels.at(row).x > line.at(j).x) {
        left_pixels.at(row) = line.at(j);
      }
      if (right_pixels.at(row).x < line.at(j).x) {
        right_pixels.at(row) = line.at(j);
      }
    }
  }
  // dec_buf(); print_buf(); printf("compute_polygon_rows end\n"); //debugprint
}

void draw_rows(screen* screen, const vector<Pixel>& left_pixels, const vector<Pixel>& right_pixels, vec4 current_normal, vec3 current_reflectance) {
  // print_buf(); printf("draw_rows start\n"); inc_buf(); //debugprint
  uint N = left_pixels.size();

  // Iterate through each row
  for (uint row = 0; row < N; row++) {

    // Calculate the depths of the pixels in this row
    vector<Pixel> row_pixels(right_pixels.at(row).x - left_pixels.at(row).x + 1);
    interpolate(left_pixels.at(row), right_pixels.at(row), row_pixels);

    for (uint i = 0; i < row_pixels.size(); i++)  {
      pixel_shader(screen, row_pixels[i], current_normal, current_reflectance);
    }
  }
  // dec_buf(); print_buf(); printf("draw_rows end\n"); //debugprint
}

void vertex_shader(const Vertex& v, Pixel& p) {
  // print_buf(); printf("vertex_shader start\n"); inc_buf(); //debugprint

  float X = (v.position.x - camera_position.x);
  float Y = (v.position.y - camera_position.y);
  float Z = (v.position.z - camera_position.z);
  float x = focal_length * X/Z + SCREEN_WIDTH /2.0f;
  float y = focal_length * Y/Z + SCREEN_HEIGHT/2.0f;
  float z = Z;
  p = Pixel((int) x, (int) y, z, v.position);
  // dec_buf(); print_buf(); printf("vertex_shader end\n"); //debugprint
}

void pixel_shader(screen* screen, const Pixel& p, vec4 current_normal, vec3 current_reflectance) {
  vec4 r = light_position - p.pos_3D;

  float radius = length(r); // WATCH OUT! Only works for w == 1
  vec4 n = current_normal;

  vec3 D = (vec3) (light_power * (float) fmax(glm::dot(r, n) , 0)) /
                                 (float) (4 * M_PI * radius * radius);
  vec3 R = current_reflectance * (D + indirect_power_per_area);

  int x = p.x;
  int y = p.y;
  if (1/p.z > depth_buffer[y][x]) {
    depth_buffer[y][x] = 1/p.z;
    PutPixelSDL(screen, x, y, R);
  }
}

void interpolate(Pixel a, Pixel b, vector<Pixel>& result) {
  // print_buf(); printf("interpolate start\n"); inc_buf(); //debugprint
  float steps = float(fmax(result.size() - 1, 1));
  for (uint i = 0; i < result.size(); i++) {
    result.at(i) = a + ((b - a) * ((float)i)) / steps;
    result[i].pos_3D = result[i].z * (a.pos_3D/a.z + ((b.pos_3D/b.z - a.pos_3D/a.z) * ((float)i)) / steps);
    result[i].pos_3D.w = 1.f;
  }
  // dec_buf(); print_buf(); printf("interpolate end\n"); //debugprint
}

// Place updates of parameters here
bool update() {
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
        case SDLK_w:
          light_position.z += 0.2;
          break;
        case SDLK_s:
          light_position.z -= 0.2;
          break;
        case SDLK_a:
          light_position.x -= 0.2;
          break;
        case SDLK_d:
          light_position.x += 0.2;
          break;
        case SDLK_ESCAPE:
          // Move camera quit
          return false;
      }
    }
  }
  return true;
}

void print_buf() { for (int i = 0; i < indent_buffer; i++) printf("  "); }
void inc_buf() { indent_buffer += 2; }
void dec_buf() { indent_buffer -= 2; }
