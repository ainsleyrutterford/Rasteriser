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
#include <algorithm>

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
#define AA 2

struct Vertex {
  vec4 position;
};

float depth_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec3  screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
int   stencil_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
vec4  camera_position(0.f, 0.f, -3.f, 1.f);
float focal_length = 600.f * AA;
float yaw = 0.f;
float pitch = 0.f;

vec4 light_position(0.f, -0.5f, -0.7f, 1.f);
vec4 original_light_position(0.f, -0.5f, -0.7f, 1.f);
vec3 light_power = 18.f * vec3(1.f, 1.f, 1.f);
vec3 indirect_power_per_area = 0.5f * vec3(1.f, 1.f, 1.f);

bool update();
void draw(screen* screen, vector<Triangle>& triangles);
void draw_shadows();
void draw_screen(screen* screen);
vector<Triangle> shadows(vector<Triangle> clipped_triangles);
vector<Triangle> clip_space(vector<Triangle>& triangles, mat4 R);
vector<Triangle> clip(vector<Triangle>& triangles);
vector<Triangle> clip_top(vector<Triangle>& triangles);
vector<Triangle> clip_right(vector<Triangle>& triangles);
vector<Triangle> clip_bottom(vector<Triangle>& triangles);
vector<Triangle> clip_left(vector<Triangle>& triangles);
void vertex_shader(const Vertex& v, Pixel& p);
void pixel_shader(screen* screen, const Pixel& p, vec4 current_normal, vec3 current_reflectance);
void interpolate(Pixel a, Pixel b, vector<Pixel>& result);
void compute_polygon_rows(const vector<Pixel>& vertex_pixels, vector<Pixel>& left_pixels, vector<Pixel>& right_pixels);
void draw_rows(screen* screen, const vector<Pixel>& left_pixels, const vector<Pixel>& right_pixels, vec4 current_normal, vec3 current_reflectance);
void draw_polygon(screen* screen, const vector<Vertex>& vertices, vec4 current_normal, vec3 current_reflectance );

int main(int argc, char* argv[]) {
  screen *screen = InitializeSDL(SCREEN_WIDTH/AA, SCREEN_HEIGHT/AA, FULLSCREEN_MODE);

  SDL_ShowCursor(SDL_DISABLE);

  vector<Triangle> triangles;
  // Load Cornell Box.
  LoadTestModel(triangles);
  // Import low poly stanford bunny model.
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
  // Clear screen, depth, and stencil buffers.
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));
  memset(depth_buffer  , 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(float   ));
  memset(screen_buffer , 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(vec3    ));
  memset(stencil_buffer, 0, SCREEN_HEIGHT  * SCREEN_WIDTH  * sizeof(int     ));

  // Rotation matrix.
  float r[16] = {cos(yaw),  sin(pitch)*sin(yaw),   sin(yaw)*cos(pitch),  1.0f,
                 0.0f,      cos(pitch),           -sin(pitch),           1.0f,
                -sin(yaw),  cos(yaw)*sin(pitch),   cos(pitch)*cos(yaw),  1.0f,
                 1.0f,      1.0f,                  1.0f,                 1.0f};
  mat4 R;
  memcpy(glm::value_ptr(R), r, sizeof(r));

  // Rotate the light.
  light_position = R * (original_light_position - camera_position);

  // Map triangles from world space to clip space.
  vector<Triangle> clipped_triangles = clip_space(triangles, R);

  // Generate shadow volume triangles.
  vector<Triangle> shadow_triangles = shadows(triangles);
  // Map shadow volume triangles to clip space.
  shadow_triangles = clip_space(shadow_triangles, R);
  // Attach shadow_volume triangles to triangles to be clipped.
  clipped_triangles.insert(clipped_triangles.end(), shadow_triangles.begin(), shadow_triangles.end());

  // Clip top, right, bottom, and left planes of the frustrum.
  clipped_triangles = clip_top(clipped_triangles);
  clipped_triangles = clip_right(clipped_triangles);
  clipped_triangles = clip_bottom(clipped_triangles);
  clipped_triangles = clip_left(clipped_triangles);

  // Call draw_polygon for each triangle.
  for (uint32_t i = 0; i < clipped_triangles.size(); i++) {
    vector<Vertex> vertices(3);

    vertices[0].position = clipped_triangles[i].v0;
    vertices[1].position = clipped_triangles[i].v1;
    vertices[2].position = clipped_triangles[i].v2;

    vec4 current_normal      = clipped_triangles[i].normal;
    vec3 current_reflectance = clipped_triangles[i].color;

    draw_polygon(screen, vertices, current_normal, current_reflectance);
  }
  // Draw the shadows on the screen buffer.
  draw_shadows();
  // Draw the screen with anti-aliasing.
  draw_screen(screen);
}

// Draw the shadows on the screen buffer. For each pixel, if the stencil
// buffer is greater than 0, it is in shadow. If the pixel is in shadow,
// remove 0.5 from the colour value.
void draw_shadows() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (stencil_buffer[y][x] > 0) {
        screen_buffer[y][x] -= 0.5f;
      }
    }
  }
}

// Draw the screen with anti-aliasing. Loop through every pixel and average
// the pixel values with its neighbours. Finally, call PutPixelSDL with the
// correct x and y position, and the average colour value.
void draw_screen(screen* screen) {
  for (int y = 0; y < SCREEN_HEIGHT; y+=AA) {
    for (int x = 0; x < SCREEN_WIDTH; x+=AA) {
      vec3 average;
      for (int a = 0; a < AA; a++) {
        for (int aa = 0; aa < AA; aa++) {
          average += screen_buffer[y+a][x+aa];
        }
      }
      average /= AA*AA;
      PutPixelSDL(screen, int(x/AA), int(y/AA), average);
    }
  }
}

// Generate the shadow triangles and return them.
vector<Triangle> shadows(vector<Triangle> clipped_triangles) {
  // The edges that lie between lit and dark surfaces. These edges will
  // then be extruded to create the shadow volumes.
  vector<Edge> contour_edges;

  // This algorithm iterates through the triangles and fills the contour edges
  // list with the correct edges mentioned above.
  for (uint32_t i = 0; i < clipped_triangles.size(); i++) {

    Triangle triangle = clipped_triangles[i];

    // The average position of the triangle.
    vec4 average_pos = (triangle.v0 + triangle.v1 + triangle.v2) / 3.f;

    // The direction of the incident light to the triangle.
    vec4 incident_light_dir = average_pos - original_light_position;

    // The three edges of the triangle.
    vector<Edge> triangle_edges;
    triangle_edges.push_back(Edge(triangle.v0, triangle.v1));
    triangle_edges.push_back(Edge(triangle.v1, triangle.v2));
    triangle_edges.push_back(Edge(triangle.v2, triangle.v0));

    // If the triangle faces away from the light source...
    if (glm::dot( vec3(incident_light_dir), vec3(clipped_triangles[i].normal) ) >= 0.f) {
      // Iterate through each edge of the triangle.
      for (int e = 0; e < 3; e++) {
        // If the contour_edges contains the edge...
        // Note that this line takes O(n) time since we are using vectors. It would definitely
        // be worth implementing the contour edges list using an unordered_set for example, as
        // the lookup time would be O(1). The line below slows down the rasteriser considerably.
        std::vector<Edge>::iterator id = std::find(contour_edges.begin(), contour_edges.end(), triangle_edges[e]);
        if (id != contour_edges.end()) {
          // Then remove the edge from the contour edges list.
          contour_edges.erase(id);
        } else {
          // Otherwise, add it to the contour edges list.
          contour_edges.push_back(triangle_edges[e]);
        }
      }
    }
  }

  // The final shadow volumes to be returned.
  vector<Triangle> shadow_triangles;

  // Iterate over the contour edges.
  for (uint32_t i = 0; i < contour_edges.size(); i++) {
    vec4 p1 = contour_edges[i].p1;
    vec4 p2 = contour_edges[i].p2;
    // Create two triangles starting at the contour edge, and extruding out in
    // the direction from the light source to the edge. The triangles can extrude
    // a large distance as the shadow triangles will eventually be clipped anyway.
    // The shadow triangles have a colour of (-1, -1, -1) so we know which
    // triangles are which later on.
    shadow_triangles.push_back(Triangle(p1, p2, p1 + 20.f * (p1 - original_light_position), vec3(-1.f,-1.f,-1.f)));
    shadow_triangles.push_back(Triangle(p2 + 20.f * (p2 - original_light_position), p1 + 20.f * (p1 - original_light_position), p2, vec3(-1.f,-1.f,-1.f)));
  }

  // Finally, return the shadow triangles.
  return shadow_triangles;
}

// Map shadow volume triangles to clip space. We also rotate the triangles
// using the rotation matrix.
vector<Triangle> clip_space(vector<Triangle>& triangles, mat4 R) {
  vector<Triangle> clipped_triangles;

  for (uint i = 0; i < triangles.size(); i++) {
    // For each vertex, set the position relative to the camera position,
    // and rotate by R. Then set the w co-ordinate to z/f so it is in clip space.
    vec4 v0   = R * (triangles[i].v0 - camera_position);
    v0.w = v0.z/focal_length;

    vec4 v1   = R * (triangles[i].v1 - camera_position);
    v1.w = v1.z/focal_length;

    vec4 v2   = R * (triangles[i].v2 - camera_position);
    v2.w = v2.z/focal_length;

    // Create a triangle out of the new vertices.
    Triangle triangle(v0, v1, v2, triangles[i].color);

    // Add the triangle to the clipped triangles.
    clipped_triangles.push_back(triangle);
  }
  return clipped_triangles;
}

// This function handles the case when two vertices are off screen, and one is
// on screen. The two_offscreen_y is very similar, just in the y axis. These
// two functions could be merged and generalised later on.
void two_offscreen_x(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  // t1 and t2 are the 'distances' along the lines from the start of the lines to
  // the intersections of the lines with the clipping plane.
  // We multiply by 0.9999 as the intersection points calculated could be one
  // one pixel out of frame, for example.
  float t1 = 0.9999f * (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) /
             ((v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) - (v1.w - 2 * v1.x / (side * SCREEN_WIDTH)));
  float t2 = 0.9999f * (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) /
             ((v0.w - 2 * v0.x / (side * SCREEN_WIDTH)) - (v2.w - 2 * v2.x / (side * SCREEN_WIDTH)));
  // The intersection points are calculated.
  vec4 i1 = v0 + t1 * (v1 - v0);
  vec4 i2 = v0 + t2 * (v2 - v0);
  // A new triangle is created.
  Triangle new_triangle(v0, i1, i2, triangle.color); new_triangle.normal = triangle.normal;
  triangles.push_back(new_triangle);
}

void one_offscreen_x(int side, vector<Triangle>& triangles, Triangle triangle, vec4 v0, vec4 v1, vec4 v2) {
  // Same as above.
  float t1 = 0.9999f * (v1.w - 2 * v1.x / (side * SCREEN_WIDTH)) /
             ((v1.w - 2 * v1.x / (side * SCREEN_WIDTH)) - (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)));
  float t2 = 0.9999f * (v2.w - 2 * v2.x / (side * SCREEN_WIDTH)) /
             ((v2.w - 2 * v2.x / (side * SCREEN_WIDTH)) - (v0.w - 2 * v0.x / (side * SCREEN_WIDTH)));
  vec4 i1 = v1 + t1 * (v0 - v1);
  vec4 i2 = v2 + t2 * (v0 - v2);
  // This time two new triangles are created.
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

// Clip the top plane.
vector<Triangle> clip_top(vector<Triangle>& triangles) {
  vector<Triangle> clipped_triangles;
  // The max possible y.
  float y_max = SCREEN_HEIGHT / 2;
  for (uint i = 0; i < triangles.size(); i++) {
    Triangle triangle = triangles[i];

    bool v0 = true, v1 = true, v2 = true;
    bool on_screen = true;

    // Check if v0 is on screen.
    if (triangle.v0.y <= -triangle.v0.w * y_max) {
      v0        = false;
      on_screen = false;
    }
    // Check if v1 is on screen.
    if (triangle.v1.y <= -triangle.v1.w * y_max) {
      v1        = false;
      on_screen = false;
    }
    // Check if v2 is on screen.
    if (triangle.v2.y <= -triangle.v2.w * y_max) {
      v2        = false;
      on_screen = false;
    }

    // If any of them are not on screen, then dont add the triangle to the
    // clipped triangles. If they are all on screen, add the triangle to the
    // clipped triangles as nothing needs to be done to this triangle.
    if (on_screen) clipped_triangles.push_back(triangle);

    // Now cover each possible case.
    if      ( v0 && !v1 && !v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if (!v0 &&  v1 && !v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if (!v0 && !v1 &&  v2) two_offscreen_y(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
    else if (!v0 &&  v1 &&  v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v0, triangle.v1, triangle.v2);
    else if ( v0 && !v1 &&  v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v1, triangle.v0, triangle.v2);
    else if ( v0 &&  v1 && !v2) one_offscreen_y(-1, clipped_triangles, triangle, triangle.v2, triangle.v0, triangle.v1);
  }
  return clipped_triangles;
}

// Clip the right plane. Similar to clip_top() above.
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

// Clip the bottom plane. Similar to clip_top() above.
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

// Clip the left plane. Similar to clip_top() above.
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

// Takes three certices and draws the triangle on the screen.
void draw_polygon(screen* screen, const vector<Vertex>& vertices, vec4 current_normal, vec3 current_reflectance) {
  uint V = vertices.size();
  vector<Pixel> vertex_pixels(V);
  // Get the pixel for each vertex.
  for (uint i = 0; i < V; i++) {
    vertex_shader(vertices[i], vertex_pixels[i]);
  }
  vector<Pixel> left_pixels;
  vector<Pixel> right_pixels;
  // Fill left_pixels and right_pixels with the correct values.
  compute_polygon_rows(vertex_pixels, left_pixels, right_pixels);
  // Colour in the rows between the left_pixels and right_pixels.
  draw_rows(screen, left_pixels, right_pixels, current_normal, current_reflectance);
}

// Create a pixel given a vertex.
void vertex_shader(const Vertex& v, Pixel& p) {
  // Homogenisation.
  float x = focal_length * v.position.x/v.position.z + SCREEN_WIDTH /2.0f;
  float y = focal_length * v.position.y/v.position.z + SCREEN_HEIGHT/2.0f;
  float z_inv = 1.f/v.position.z;
  p = Pixel((int) x, (int) y, z_inv, v.position);
}

// Fill left_pixels and right_pixels vectors with the correct values.
void compute_polygon_rows(const vector<Pixel>& vertex_pixels,
                          vector<Pixel>& left_pixels,
                          vector<Pixel>& right_pixels) {
  int min_y = numeric_limits<int>::max();
  int max_y = numeric_limits<int>::min();

  // Find the min and max y values.
  for (uint i = 0; i < vertex_pixels.size(); i++) {
    if (vertex_pixels.at(i).y < min_y) min_y = vertex_pixels.at(i).y;
    if (vertex_pixels.at(i).y > max_y) max_y = vertex_pixels.at(i).y;
  }

  // Change the left_pixels and right_pixels lengths to the correct lengths.
  left_pixels.resize(max_y - min_y + 1);
  right_pixels.resize(max_y - min_y + 1);

  for (uint i = 0; i < left_pixels.size(); i++) {
    left_pixels.at(i).x = numeric_limits<int>::max();
    right_pixels.at(i).x = numeric_limits<int>::min();
  }

  // For each edge, fill the left_pixels and right_pixels vectors with the
  // correct values.
  for (uint i = 0; i < vertex_pixels.size(); i++) {
    Pixel a = vertex_pixels.at(i);
    Pixel b = vertex_pixels.at((i + 1) % vertex_pixels.size());

    Pixel delta = Pixel::abs(a-b);
    uint pixels = glm::max(delta.x, delta.y) + 1;

    vector<Pixel> line(pixels);
    interpolate(a, b, line);

    // For each pixel returned by interploate(), if the x value is smaller
    // than the existing x value, then update the left pixel. If the x value
    // is larger than the existing x value, then update the right pixel.
    for (uint j = 0; j < line.size(); j++) {
      int row = line.at(j).y - min_y;

      // If new x is smaller, update left pixel.
      if (left_pixels.at(row).x > line.at(j).x) {
        left_pixels.at(row) = line.at(j);
      }

      // If new x is larger, update right pixel.
      if (right_pixels.at(row).x < line.at(j).x) {
        right_pixels.at(row) = line.at(j);
      }
    }
  }
}

// Given a vector of left and right pixels, call pixel_shader for each pixel
// in between the left and right pixels.
void draw_rows(screen* screen, const vector<Pixel>& left_pixels, const vector<Pixel>& right_pixels, vec4 current_normal, vec3 current_reflectance) {
  uint N = left_pixels.size();

  // Iterate through each row.
  for (uint row = 0; row < N; row++) {

    // Create a vector of row_pixels that is the correct length.
    vector<Pixel> row_pixels(right_pixels.at(row).x - left_pixels.at(row).x + 1);
    interpolate(left_pixels.at(row), right_pixels.at(row), row_pixels);

    // For each row pixel, call pixel_shader.
    for (uint i = 0; i < row_pixels.size(); i++)  {
      pixel_shader(screen, row_pixels[i], current_normal, current_reflectance);
    }
  }
}

// Given a pixel, draw it on the correct position of the screen.
void pixel_shader(screen* screen, const Pixel& p, vec4 current_normal, vec3 current_reflectance) {
  // The ray from the pixel to the light.
  vec4 r = light_position - p.pos_3D;
  r.w = 0.0f;

  // Calculate the squared radius of the ray.
  float radius_sq = r.x * r.x + r.y * r.y + r.z * r.z;
  vec4 n = current_normal;

  // Calculate the pixel light intensity.
  vec3 D = (vec3) (light_power * (float) fmax(glm::dot(r, n) , 0)) /
                                 (float) (4 * M_PI * radius_sq);

  // Multiply the intensity (plus the indirect light) by the colour.
  vec3 R = current_reflectance * (D + indirect_power_per_area);

  int x = p.x;
  int y = p.y;
  // Check that the x and y are actually in the screen.
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    // If this is the closest pixel so far...
    if (p.z_inv > depth_buffer[y][x]) {
      // Update the depth_buffer.
      depth_buffer[y][x] = p.z_inv;
      // If this pixel is a shadow pixel.
      if (current_reflectance == vec3(-1.f, -1.f, -1.f)) {
        // Update the stencil buffer.
        stencil_buffer[y][x]++;
      } else {
        // Otherwise, update the screen buffer with the pixel's colour.
        screen_buffer[y][x] = R;
      }
    }
  }
}

// Interpolate between two pixels.
void interpolate(Pixel a, Pixel b, vector<Pixel>& result) {
  float steps = float(fmax(result.size() - 1, 1));
  for (uint i = 0; i < result.size(); i++) {
    // We have a pixel class with +, -, *, and / operators in Pixel.cpp.
    result.at(i) = a + ((b - a) * ((float)i)) / steps;
    // Perspective correct interpolation for the pos_3D.
    result[i].pos_3D = (a.pos_3D*a.z_inv + ((b.pos_3D*b.z_inv - a.pos_3D*a.z_inv) * ((float) i)) / steps) / result[i].z_inv;
    result[i].pos_3D.w = 1.f;
  }
}

// Place updates of parameters here
bool update() {
  SDL_Event e;
  while(SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      return false;
    } else if (e.type == SDL_MOUSEMOTION) { // Handle mouse movement.
      yaw   += e.motion.xrel * 0.01f;
      pitch -= e.motion.yrel * 0.01f;
    } else if (e.type == SDL_MOUSEWHEEL) {  // Hande mouse wheel input.
      focal_length += e.wheel.y;
    } else if (e.type == SDL_KEYDOWN) {     // Handle key presses.
      int key_code = e.key.keysym.sym;
      switch(key_code) {
        case SDLK_w:
          // Move camera forward.
          camera_position.z += 0.1;
          break;
        case SDLK_s:
          // Move camera backwards.
          camera_position.z -= 0.1;
          break;
        case SDLK_a:
          // Move camera left.
          camera_position.x -= 0.1;
          break;
        case SDLK_d:
          // Move camera right.
          camera_position.x += 0.1;
          break;
        case SDLK_UP:
          original_light_position.z += 0.2;
          break;
        case SDLK_DOWN:
          original_light_position.z -= 0.2;
          break;
        case SDLK_LEFT:
          original_light_position.x -= 0.2;
          break;
        case SDLK_RIGHT:
          original_light_position.x += 0.2;
          break;
        case SDLK_i:
          // Move camera up.
          camera_position.y += 0.1;
          break;
        case SDLK_o:
          // Move camera down.
          camera_position.y -= 0.1;
          break;
        case SDLK_ESCAPE:
          // Move camera quit
          return false;
      }
    }
  }
  return true;
}
