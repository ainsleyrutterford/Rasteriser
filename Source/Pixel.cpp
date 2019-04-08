using glm::vec4;

class Pixel {
  public:
    int x;
    int y;
    float z;
    vec4 pos_3D;

    Pixel() {}

    Pixel (const Pixel& p) {
      this->x = p.x;
      this->y = p.y;
      this->z = p.z;
      this->pos_3D = p.pos_3D;
    }

    Pixel(int x, int y, float z, vec4 pos_3D) {
      this->x = x;
      this->y = y;
      this->z = z;
      this->pos_3D = pos_3D;
    }

    Pixel operator +(const Pixel& p) {
      Pixel pixel;
      pixel.x = this->x + p.x;
      pixel.y = this->y + p.y;
      pixel.z = this->z + p.z;
      pixel.pos_3D = this->pos_3D + p.pos_3D;
      pixel.pos_3D.w = 1.f;
      return pixel;
    }

    void operator +=(const Pixel& p) {
      this->x = this->x + p.x;
      this->y = this->y + p.y;
      this->z = this->z + p.z;
      this->pos_3D += p.pos_3D;
      this->pos_3D.w = 1.f;

    }

    Pixel operator -(const Pixel& p) {
      Pixel pixel;
      pixel.x = this->x - p.x;
      pixel.y = this->y - p.y;
      pixel.z = this->z - p.z;
      pixel.pos_3D = this->pos_3D - p.pos_3D;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    Pixel operator /(float f) {
      Pixel pixel;
      pixel.x = this->x / f;
      pixel.y = this->y / f;
      pixel.z = this->z / f;
      pixel.pos_3D = this->pos_3D / f;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    Pixel operator *(float f) {
      Pixel pixel;
      pixel.x = this->x * f;
      pixel.y = this->y * f;
      pixel.z = this->z * f;
      pixel.pos_3D = this->pos_3D * f;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    static Pixel abs(const Pixel& p) {
      Pixel pixel;
      pixel.x = (p.x > 0) ? p.x : -p.x;
      pixel.y = (p.y > 0) ? p.y : -p.y;
      pixel.z = (p.z > 0) ? p.z : -p.z;
      pixel.pos_3D = p.pos_3D;
      return pixel;
    }
};
