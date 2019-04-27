using glm::vec4;

class Pixel {
  public:
    int x;
    int y;
    float z_inv;
    vec4 pos_3D;

    Pixel() {}

    Pixel (const Pixel& p) {
      this->x = p.x;
      this->y = p.y;
      this->z_inv = p.z_inv;
      this->pos_3D = p.pos_3D;
    }

    Pixel(int x, int y, float z_inv, vec4 pos_3D) {
      this->x = x;
      this->y = y;
      this->z_inv = z_inv;
      this->pos_3D = pos_3D;
    }

    Pixel operator +(const Pixel& p) {
      Pixel pixel;
      pixel.x = this->x + p.x;
      pixel.y = this->y + p.y;
      pixel.z_inv = this->z_inv + p.z_inv;
      pixel.pos_3D = this->pos_3D + p.pos_3D;
      pixel.pos_3D.w = 1.f;
      return pixel;
    }

    void operator +=(const Pixel& p) {
      this->x = this->x + p.x;
      this->y = this->y + p.y;
      this->z_inv = this->z_inv + p.z_inv;
      this->pos_3D += p.pos_3D;
      this->pos_3D.w = 1.f;

    }

    Pixel operator -(const Pixel& p) {
      Pixel pixel;
      pixel.x = this->x - p.x;
      pixel.y = this->y - p.y;
      pixel.z_inv = this->z_inv - p.z_inv;
      pixel.pos_3D = this->pos_3D - p.pos_3D;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    Pixel operator /(float f) {
      Pixel pixel;
      pixel.x = this->x / f;
      pixel.y = this->y / f;
      pixel.z_inv = this->z_inv / f;
      pixel.pos_3D = this->pos_3D / f;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    Pixel operator *(float f) {
      Pixel pixel;
      pixel.x = this->x * f;
      pixel.y = this->y * f;
      pixel.z_inv = this->z_inv * f;
      pixel.pos_3D = this->pos_3D * f;
      pixel.pos_3D.w = 1.f;

      return pixel;
    }

    static Pixel abs(const Pixel& p) {
      Pixel pixel;
      pixel.x     = (p.x > 0) ? p.x : -p.x;
      pixel.y     = (p.y > 0) ? p.y : -p.y;
      pixel.z_inv = (p.z_inv > 0) ? p.z_inv : -p.z_inv;
      pixel.pos_3D = p.pos_3D;
      return pixel;
    }
};
