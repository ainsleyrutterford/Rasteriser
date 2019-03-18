using glm::vec3;

class Pixel {
  public:
    int x;
    int y;
    float z_inv;
    vec3 illumination;

    Pixel() {}

    Pixel (const Pixel& p) {
      this->x = p.x;
      this->y = p.y;
      this->z_inv = p.z_inv;
      this->illumination = p.illumination;
    }

    Pixel(int x, int y, float z_inv, vec3 illumination) {
      this->x = x;
      this->y = y;
      this->z_inv = z_inv;
      this->illumination = illumination;
    }

    Pixel operator +(const Pixel& p) {
      Pixel pixel;
      pixel.x = this->x + p.x;
      pixel.y = this->y + p.y;
      pixel.z_inv = 1/(1/this->z_inv + 1/(p.z_inv));
      pixel.illumination = this->illumination + p.illumination;
      return pixel;
    }

    void operator +=(const Pixel& p) {
      this->x = this->x + p.x;
      this->y = this->y + p.y;
      this->z_inv = 1/(1/this->z_inv + 1/(p.z_inv));
      this->illumination += p.illumination;
    }

    Pixel operator -( const Pixel& p ) {
      Pixel pixel;
      pixel.x = this->x - p.x;
      pixel.y = this->y - p.y;
      pixel.z_inv = 1/(1/this->z_inv - 1/(p.z_inv));
      pixel.illumination = this->illumination - p.illumination;
      return pixel;
    }

    Pixel operator /(float f)  {
      Pixel pixel;
      pixel.x = this->x / f;
      pixel.y = this->y / f;
      pixel.z_inv = this->z_inv * f;
      pixel.illumination = this->illumination / f;
      return pixel;
    }

    static Pixel abs(const Pixel& p)  {
      Pixel pixel;
      pixel.x = (p.x>0)? p.x :-p.x;
      pixel.y = (p.y>0) ? p.y :-p.y;
      pixel.z_inv = p.z_inv;
      pixel.illumination = p.illumination;
      return pixel;
    }
};
