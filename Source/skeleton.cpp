#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include "Pixel.cpp"
#include <stdint.h>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include "Loader.cpp"
#include "Edge.cpp"
#include <set>

using namespace std;
using glm::ivec2;
using glm::vec2;
using glm::vec3;
using glm::mat3;
using glm::vec4;
using glm::mat4;

SDL_Event event;

#define SCREEN_WIDTH 720
#define SCREEN_HEIGHT 720
#define FULLSCREEN_MODE false

int indent_buffer = 0;

struct Vertex {
  vec4 position;
};

float depth_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec3 screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
int stencil_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec4  camera_position(0.f, 0.f, -3.f, 1.f);
float focal_length = 500.f;
float yaw = 0.f;
float pitch = 0.f;

vec4 light_position(0.f, -0.5f, -0.7f, 1.f);
vec4 original_light_position(0.f, -0.5f, -0.7f, 1.f);
vec3 light_power = 18.f * vec3(1.f, 1.f, 1.f);
vec3 indirect_power_per_area = 0.5f * vec3(1.f, 1.f, 1.f);

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void draw_screen(screen* screen);
vector<Triangle> shadows(vector<Triangle> clipped_triangles);
vector<Triangle> clip_space(vector<Triangle>& triangles);
vector<Triangle> clip(vector<Triangle>& triangles);
vector<Triangle> clip_top(vector<Triangle>& triangles);
vector<Triangle> clip_right(vector<Triangle>& triangles);
vector<Triangle> clip_bottom(vector<Triangle>& triangles);
vector<Triangle> clip_left(vector<Triangle>& triangles);
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

  SDL_ShowCursor(SDL_DISABLE);

  vector<Triangle> triangles;
  // Load Cornell Box
  LoadTestModel(triangles);
  vector<Triangle> bunny = load_obj("Source/bunny_200.obj");
  triangles.insert(triangles.end(), bunny.begin(), bunny.end());

  while (update()) {
    draw(screen, triangles);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

void draw(screen* screen, vector<Triangle>& triangles) {
  // Clear screen buffer and depth_buffer
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));
  memset(depth_buffer  , 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(float   ));
  memset(screen_buffer , 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(vec3    ));
  memset(stencil_buffer, 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(int     ));

  light_position = original_light_position - camera_position;

  vector<Triangle> shadow_triangles = shadows(triangles);

  vector<Triangle> clipped_triangles = clip_space(triangles);

  shadow_triangles = clip_space(shadow_triangles);

  clipped_triangles.insert(clipped_triangles.end(), shadow_triangles.begin(), shadow_triangles.end());

  clipped_triangles = clip_top(clipped_triangles);
  clipped_triangles = clip_right(clipped_triangles);
  clipped_triangles = clip_bottom(clipped_triangles);
  clipped_triangles = clip_left(clipped_triangles);

  for (uint32_t i = 0; i < clipped_triangles.size(); i++) {
    vector<Vertex> vertices(3);

    vertices[0].position = clipped_triangles[i].v0;
    vertices[1].position = clipped_triangles[i].v1;
    vertices[2].position = clipped_triangles[i].v2;

    vec4 current_normal      = clipped_triangles[i].normal;
    vec3 current_reflectance = clipped_triangles[i].color;

    draw_polygon(screen, vertices, current_normal, current_reflectance);
  }
  draw_screen(screen);
}

void draw_screen(screen* screen) {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (stencil_buffer[y][x] > 0) {
        screen_buffer[y][x] -= 0.5f;
      }
      PutPixelSDL(screen, x, y, screen_buffer[y][x]);
    }
  }
}

vector<Triangle> shadows(vector<Triangle> clipped_triangles) {
  vector<Edge> contour_edges;

  for (uint32_t i = 0; i < clipped_triangles.size(); i++) {

    Triangle triangle = clipped_triangles[i];

    vec4 average_pos = (triangle.v0 + triangle.v1 + triangle.v2) / 3.f;

    vec4 incident_light_dir = average_pos - original_light_position;

    vector<Edge> triangle_edges;
    triangle_edges.push_back(Edge(triangle.v0, triangle.v1));
    triangle_edges.push_back(Edge(triangle.v1, triangle.v2));
    triangle_edges.push_back(Edge(triangle.v2, triangle.v0));

    if (glm::dot( vec3(incident_light_dir), vec3(clipped_triangles[i].normal) ) >= 0.f) {
      for (int e = 0; e < 3; e++) {
        auto id = std::find(contour_edges.begin(), contour_edges.end(), triangle_edges[e]);
        if (id != contour_edges.end()) {
          contour_edges.erase(id);
        } else {
          contour_edges.push_back(triangle_edges[e]);
        }
      }
    }
  }

  vector<Triangle> shadow_triangles;

  for (uint32_t i = 0; i < contour_edges.size(); i++) {
    vec4 p1 = contour_edges[i].p1;
    vec4 p2 = contour_edges[i].p2;
    shadow_triangles.push_back(Triangle(p1, p2, p1 + 20.f * (p1 - original_light_position), vec3(-1.f,-1.f,-1.f)));
    shadow_triangles.push_back(Triangle(p2, p1 + 20.f * (p1 - original_light_position), p2 + 20.f * (p2 - original_light_position), vec3(-1.f,-1.f,-1.f)));
  }

  return shadow_triangles;
}

vector<Triangle> clip_space(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;

  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];
    triangle.v0   = triangle.v0 - camera_position;
    triangle.v0.w = triangle.v0.z/focal_length;

    triangle.v1   = triangle.v1 - camera_position;
    triangle.v1.w = triangle.v1.z/focal_length;

    triangle.v2   = triangle.v2 - camera_position;
    triangle.v2.w = triangle.v2.z/focal_length;

    clipped_triangles.push_back(triangle);
  }
  return clipped_triangles;
}

void two_offscreen_x(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  float t1 = 0.9999f * (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) /
             ((v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) - (v1.w - 2 * v1.x / (side * SCREEN_WIDTH)));
  float t2 = 0.9999f * (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) /
             ((v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) - (v2.w - 2 * v2.x / (side * SCREEN_WIDTH)));
  vec4 i1 = v0 + t1 * (v1 - v0);
  vec4 i2 = v0 + t2 * (v2 - v0);
  Triangle new_triangle(v0, i1, i2, triangle.color); new_triangle.normal = triangle.normal;
  triangles.push_back(new_triangle);
}

void one_offscreen_x(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  float t1 = 0.9999f * (v1.w - 2 * v1.x / (side * SCREEN_WIDTH)) /
             ((v1.w - 2 * v1.x / (side * SCREEN_WIDTH)) - (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)));
  float t2 = 0.9999f * (v2.w - 2 * v2.x / (side * SCREEN_WIDTH)) /
             ((v2.w - 2 * v2.x / (side * SCREEN_WIDTH)) - (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)));
  vec4 i1 = v1 + t1 * (v0 - v1);
  vec4 i2 = v2 + t2 * (v0 - v2);
  Triangle new_triangle1(i1, v1, v2, triangle.color); new_triangle1.normal = triangle.normal;
  Triangle new_triangle2(i1, i2, v2, triangle.color); new_triangle2.normal = triangle.normal;
  triangles.push_back(new_triangle1);
  triangles.push_back(new_triangle2);
}

void two_offscreen_y(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  float t1 = 0.9999f * (v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)) /
             ((v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)) - (v1.w - 2 * v1.y / (side * SCREEN_HEIGHT)));
  float t2 = 0.9999f * (v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)) /
             ((v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)) - (v2.w - 2 * v2.y / (side * SCREEN_HEIGHT)));
  vec4 i1 = v0 + t1 * (v1 - v0);
  vec4 i2 = v0 + t2 * (v2 - v0);
  Triangle new_triangle(v0, i1, i2, triangle.color); new_triangle.normal = triangle.normal;
  triangles.push_back(new_triangle);
}

void one_offscreen_y(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  float t1 = 0.9999f * (v1.w - 2 * v1.y / (side * SCREEN_HEIGHT)) /
             ((v1.w - 2 * v1.y / (side * SCREEN_HEIGHT)) - (v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)));
  float t2 = 0.9999f * (v2.w - 2 * v2.y / (side * SCREEN_HEIGHT)) /
             ((v2.w - 2 * v2.y / (side * SCREEN_HEIGHT)) - (v0.w - 2 * v0.y / (side * SCREEN_HEIGHT)));
  vec4 i1 = v1 + t1 * (v0 - v1);
  vec4 i2 = v2 + t2 * (v0 - v2);
  Triangle new_triangle1(i1, v1, v2, triangle.color); new_triangle1.normal = triangle.normal;
  Triangle new_triangle2(i1, i2, v2, triangle.color); new_triangle2.normal = triangle.normal;
  triangles.push_back(new_triangle1);
  triangles.push_back(new_triangle2);
}

vector<Triangle> clip_top(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;
  float y_max = SCREEN_HEIGHT / 2;
  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];

    bool v0 = true, v1 = true, v2 = true;
    bool on_screen = true;

    if (triangle.v0.y <= -triangle.v0.w * y_max) {
      v0        = false;
      on_screen = false;
    }
    if (triangle.v1.y <= -triangle.v1.w * y_max) {
      v1        = false;
      on_screen = false;
    }
    if (triangle.v2.y <= -triangle.v2.w * y_max) {
      v2        = false;
      on_screen = false;
    }

    if (on_screen) clipped_triangles.push_back(triangle);

    if      ( v0 && !v1 && !v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if (!v0 &&  v1 && !v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if (!v0 && !v1 &&  v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
    else if (!v0 &&  v1 &&  v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if ( v0 && !v1 &&  v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if ( v0 &&  v1 && !v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
  }
  return clipped_triangles;
}

vector<Triangle> clip_right(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;
  float x_max = SCREEN_WIDTH / 2;
  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];

    bool v0 = true, v1 = true, v2 = true;
    bool on_screen = true;

    if (triangle.v0.x >= triangle.v0.w * x_max) {
      v0        = false;
      on_screen = false;
    }
    if (triangle.v1.x >= triangle.v1.w * x_max) {
      v1        = false;
      on_screen = false;
    }
    if (triangle.v2.x >= triangle.v2.w * x_max) {
      v2        = false;
      on_screen = false;
    }

    if (on_screen) clipped_triangles.push_back(triangle);

    if      ( v0 && !v1 && !v2) two_offscreen_x(1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if (!v0 &&  v1 && !v2) two_offscreen_x(1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if (!v0 && !v1 &&  v2) two_offscreen_x(1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
    else if (!v0 &&  v1 &&  v2) one_offscreen_x(1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if ( v0 && !v1 &&  v2) one_offscreen_x(1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if ( v0 &&  v1 && !v2) one_offscreen_x(1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
  }
  return clipped_triangles;
}

vector<Triangle> clip_bottom(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;
  float y_max = SCREEN_HEIGHT / 2;
  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];

    bool v0 = true, v1 = true, v2 = true;
    bool on_screen = true;

    if (triangle.v0.y >= triangle.v0.w * y_max) {
      v0        = false;
      on_screen = false;
    }
    if (triangle.v1.y >= triangle.v1.w * y_max) {
      v1        = false;
      on_screen = false;
    }
    if (triangle.v2.y >= triangle.v2.w * y_max) {
      v2        = false;
      on_screen = false;
    }

    if (on_screen) clipped_triangles.push_back(triangle);

    if      ( v0 && !v1 && !v2) two_offscreen_y(1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if (!v0 &&  v1 && !v2) two_offscreen_y(1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if (!v0 && !v1 &&  v2) two_offscreen_y(1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
    else if (!v0 &&  v1 &&  v2) one_offscreen_y(1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if ( v0 && !v1 &&  v2) one_offscreen_y(1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if ( v0 &&  v1 && !v2) one_offscreen_y(1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
  }
  return clipped_triangles;
}

vector<Triangle> clip_left(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;
  float x_max = SCREEN_WIDTH / 2;
  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];

    bool v0 = true, v1 = true, v2 = true;
    bool on_screen = true;

    if (triangle.v0.x <=  -triangle.v0.w * x_max) {
      v0        = false;
      on_screen = false;
    }
    if (triangle.v1.x <=  -triangle.v1.w * x_max) {
      v1        = false;
      on_screen = false;
    }
    if (triangle.v2.x <=  -triangle.v2.w * x_max) {
      v2        = false;
      on_screen = false;
    }

    if (on_screen) clipped_triangles.push_back(triangle);

    if      ( v0 && !v1 && !v2) two_offscreen_x(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if (!v0 &&  v1 && !v2) two_offscreen_x(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if (!v0 && !v1 &&  v2) two_offscreen_x(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
    else if (!v0 &&  v1 &&  v2) one_offscreen_x(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if ( v0 && !v1 &&  v2) one_offscreen_x(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if ( v0 &&  v1 && !v2) one_offscreen_x(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
  }
  return clipped_triangles;
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

void vertex_shader(const Vertex& v, Pixel& p) {
  // print_buf(); printf("vertex_shader start\n"); inc_buf(); //debugprint
  float x = focal_length * v.position.x/v.position.z + SCREEN_WIDTH /2.0f;
  float y = focal_length * v.position.y/v.position.z + SCREEN_HEIGHT/2.0f;
  float z_inv = 1.f/v.position.z;
  p = Pixel((int) x, (int) y, z_inv, v.position);
  // dec_buf(); print_buf(); printf("vertex_shader end\n"); //debugprint
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

void pixel_shader(screen* screen, const Pixel& p, vec4 current_normal, vec3 current_reflectance) {
  // print_buf(); printf("pixel_shader start\n"); inc_buf(); //debugprint
  vec4 r = light_position - p.pos_3D;
  r.w = 0.0f;
  float radius_sq = r.x * r.x + r.y * r.y + r.z * r.z; // WATCH OUT! Only works for w == 1
  vec4 n = current_normal;

  vec3 D = (vec3) (light_power * (float) fmax(glm::dot(r, n) , 0)) /
                                 (float) (4 * M_PI * radius_sq);
  vec3 R = current_reflectance * (D + indirect_power_per_area);

  int x = p.x;
  int y = p.y;
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    if (p.z_inv > depth_buffer[y][x]) {
      depth_buffer[y][x] = p.z_inv;
      if (current_reflectance == vec3(-1.f, -1.f, -1.f)) {
        stencil_buffer[y][x]++;
      } else {
        screen_buffer[y][x] = R;
      }
    }
  }
  // dec_buf(); print_buf(); printf("pixel_shader end\n"); //debugprint
}

void interpolate(Pixel a, Pixel b, vector<Pixel>& result) {
  // print_buf(); printf("interpolate start\n"); inc_buf(); //debugprint
  float steps = float(fmax(result.size() - 1, 1));
  for (uint i = 0; i < result.size(); i++) {
    result.at(i) = a + ((b - a) * ((float)i)) / steps;
    result[i].pos_3D = (a.pos_3D*a.z_inv + ((b.pos_3D*b.z_inv - a.pos_3D*a.z_inv) * ((float) i)) / steps) / result[i].z_inv;
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
    } else if (e.type == SDL_MOUSEMOTION) {
      yaw   += e.motion.xrel * 0.01f;
      pitch -= e.motion.yrel * 0.01f;
    } else if (e.type == SDL_MOUSEWHEEL) {
      focal_length += e.wheel.y;
    } else if (e.type == SDL_KEYDOWN) {
      int key_code = e.key.keysym.sym;
      switch(key_code) {
        case SDLK_UP:
          // Move camera forward
          camera_position.z += 0.1;
          break;
        case SDLK_DOWN:
          // Move camera backwards
          camera_position.z -= 0.1;
          break;
        case SDLK_i:
          // Move camera forward
          camera_position.y += 0.1;
          break;
        case SDLK_o:
          // Move camera backwards
          camera_position.y -= 0.1;
          break;
        case SDLK_LEFT:
          // Move camera left
          camera_position.x -= 0.1;
          break;
        case SDLK_RIGHT:
          // Move camera right
          camera_position.x += 0.1;
          break;
        case SDLK_w:
          original_light_position.z += 0.2;
          break;
        case SDLK_s:
          original_light_position.z -= 0.2;
          break;
        case SDLK_a:
          original_light_position.x -= 0.2;
          break;
        case SDLK_d:
          original_light_position.x += 0.2;
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
