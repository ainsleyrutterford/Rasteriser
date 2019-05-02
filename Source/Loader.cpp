#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include "TestModelH.h"

using namespace std;
using glm::vec3;
using glm::vec4;
using glm::mat4;

vector<Triangle> load_obj(string filename) {

  // Defines colors:
  vec3 red         (0.75f, 0.15f, 0.15f);
  vec3 yellow      (0.75f, 0.75f, 0.15f);
  vec3 green       (0.15f, 0.75f, 0.15f);
  vec3 cyan        (0.15f, 0.75f, 0.75f);
  vec3 blue        (0.15f, 0.15f, 0.75f);
  vec3 mirror      (0.00f, 0.00f, 0.00f);
  vec3 purple      (0.75f, 0.15f, 0.75f);
  vec3 white       (0.75f, 0.75f, 0.75f);
  vec3 grey        (0.25f, 0.25f, 0.25f);
  vec3 dark_yellow (0.30f, 0.30f, 0.00f);
  vec3 dark_green  (0.00f, 0.25f, 0.00f);
  vec3 dark_purple (0.25f, 0.00f, 0.25f);

  float yaw   = 0.f;
  float pitch = 135.f;

  ifstream source(filename.c_str());
  vector<vec4> vertices;
  vector<Triangle> triangles;

  string line;

  float r[16] = {cos(yaw),  sin(pitch)*sin(yaw),   sin(yaw)*cos(pitch),  1.0f,
                 0.0f,      cos(pitch),           -sin(pitch),           1.0f,
                -sin(yaw),  cos(yaw)*sin(pitch),   cos(pitch)*cos(yaw),  1.0f,
                 1.0f,      1.0f,                  1.0f,                 1.0f};
  mat4 R;
  memcpy(glm::value_ptr(R), r, sizeof(r));

  while (getline(source, line)) {
    istringstream in(line);

    string s;
    in >> s;

    if (s == "v") {
      float x, y, z;
      in >> x >> y >> z;
      vertices.push_back(vec4(1.5f*x, 1.5f*y, 1.5f*z, 1.f));
    } else if (s == "f") {
      int v1, v2, v3;
      in >> v1 >> v2 >> v3;
      Triangle triangle = Triangle(R * vertices[v3-1], R * vertices[v2-1], R * vertices[v1-1], white);

      vec4 translate(-1.3f, 0.1f, -1.7f, 1.0f);

      triangle.v0 += translate;
      triangle.v0.w = 1.f;
      triangle.v1 += translate;
      triangle.v1.w = 1.f;
      triangle.v2 += translate;
      triangle.v2.w = 1.f;

      triangles.push_back(triangle);
    }
  }

  return triangles;
}
