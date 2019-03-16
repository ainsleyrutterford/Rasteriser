using glm::vec3;


class Pixel 
{
  public:
    int x;
    int y;
    float z_inv;

    Pixel() {}
    Pixel(int x1, int y1, float z_inv1) {
      x = x1;
      y = y1;
      z_inv = z_inv1;
    }
    Pixel(const vec3 & vec) {
      x = round(vec.x);
      y = round(vec.y);
      z_inv = vec.z;
    }

    Pixel operator +( const Pixel& p ) {
      Pixel pixel;
      pixel.x = this->x + p.x;
      pixel.y = this->y + p.y;
      pixel.z_inv = 1/(1/this->z_inv + 1/(p.z_inv));
      return pixel;
    }

    Pixel operator -( const Pixel& p ) {
      Pixel pixel;
      pixel.x = this->x - p.x;
      pixel.y = this->y - p.y;
      pixel.z_inv = 1/(1/this->z_inv - 1/(p.z_inv));
      return pixel;
    }

    Pixel operator /(float f)  {
      Pixel pixel;
      pixel.x = this->x / f;
      pixel.y = this->y / f;
      pixel.z_inv = this->z_inv * f;
      return pixel;
    }

    Pixel (const Pixel & p) {
      x = p.x;
      y = p.y;
      z_inv = p.z_inv;
    }

    void *operator new(size_t size) 
    { 
        void *p = ::new Pixel(0, 0, 0.0f);  
      
        return p; 
    } 
  
    void operator delete(void *p) 
    { 
        free(p); 
    } 

    operator vec3()  {
      return vec3(this->x, this->y, this->z_inv);
    }

    static Pixel abs(const Pixel& p)  {
      Pixel pixel;
      pixel.x = (p.x>0)? p.x :-p.x;
      pixel.y = (p.y>0) ? p.y :-p.y;
      pixel.z_inv = p.z_inv;
      return pixel;
    }

};