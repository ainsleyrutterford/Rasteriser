#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModel.h"
#include <stdint.h>

using namespace std;
using glm::vec3;
using glm::mat3;

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 512
#define FULLSCREEN_MODE false


/* ----------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                            */
int t;

/* ----------------------------------------------------------------------------*/
/* FUNCTIONS                                                                   */

void Update(vector<vec3>& stars);
void DrawColourMixer(screen* screen);
void InitStars(vector<vec3>& stars);
void DrawStarfield(screen* screen, vector<vec3>& stars);
void Interpolate( float a, float b, vector<float>& result );
void Interpolate( vec3 a, vec3 b, vector<vec3>& result ) ;

int main( int argc, char* argv[] )
{

  screen *screen = InitializeSDL( SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE );
  t = SDL_GetTicks();	/*Set start value for timer.*/

  vector<vec3> stars( 1000 );
  InitStars(stars);
  while( NoQuitMessageSDL() )
  {
    DrawStarfield(screen, stars);
    Update(stars);
    SDL_Renderframe(screen);
  }


  SDL_SaveImage( screen, "screenshot.bmp" );

  KillSDL(screen);
  return 0;
}

/*Place your drawing here*/
void DrawColourMixer(screen* screen)
{
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  vec3 top_left(1.0, 0.0, 1.0);
  vec3 top_right(0.2, 0.0, 0.2);

  vec3 bottom_left(0.1, 0.0, 0.1);
  vec3 bottom_right(0, 0.0, 0.0);

  vector<vec3> left_side( screen->height );
  Interpolate(top_left, bottom_left, left_side);

  vector<vec3> right_side( screen->height );
  Interpolate(top_right, bottom_right, right_side);

  vector<vec3> row(screen->width);
  for (int y = 0; y < screen->height; y++)  {
    Interpolate(left_side[y], right_side[y], row);
    for (int x = 0; (int)x < screen->width; x++)  {
      PutPixelSDL(screen, x, y, row[x]);
    }
  }
}

void DrawStarfield(screen* screen, vector<vec3>& stars)  {
  //Blacken Space
  memset(screen->buffer, 0, screen->width*screen->height*sizeof(uint32_t));
  //Create Stars at random locations in 3D space

  //Project 3D space onto screen
  float ui, vi, f=screen->height/2.0f;
  vec3 star_colour(1.0, 1.0, 1.0);
  for (uint i = 0; i < stars.size(); i++)  {
    star_colour = 0.2f * vec3(0,1,1) / (stars[i].z*stars[i].z);
    ui = (f*stars[i].x/stars[i].z + screen->width/2.0f) ;
    vi = f*stars[i].y/stars[i].z + screen->height/2.0f;
    PutPixelSDL(screen, ui, vi, star_colour);
  }
}

void InitStars(vector<vec3>& stars)  {
  for (uint i = 0; i < 1000; i++)  {
    stars[i].x = 2*float(rand()) / float(RAND_MAX)-1;
    stars[i].y = 2*float(rand()) / float(RAND_MAX)-1;
    stars[i].z = float(rand()) / float(RAND_MAX);
  }
}



/*Place updates of parameters here*/
void Update(vector<vec3>& stars)
{
  static int t = SDL_GetTicks();
  int t2 = SDL_GetTicks();
  float dt = float(t2-t);
  t = t2;

  for( uint s=0; s<stars.size(); ++s ) {
    stars[s].z -= 0.001f*dt;
    if( stars[s].z <= 0 )
      stars[s].z += 1;
    if( stars[s].z > 1 )
      stars[s].z -= 1;
    }

}

void Interpolate( float a, float b, vector<float>& result )  {
  if(result.size() == 0)  {
    result[0] = (a+b)/2.0f;
    return;
  }
  float spacing = (b-a)/(result.size()-1);
  for(uint i = 0; i < result.size()+1; i++)  {
    result[i] = a + i*spacing;
  }
}

void Interpolate( vec3 a, vec3 b, vector<vec3>& result )  {
  vector<vector<float> > dim_results(3, vector<float>(result.size()));
  for (uint d = 0; d < 3; d++)  {
    Interpolate(a[d], b[d], dim_results[d]);
  }
  for (uint i = 0; i < result.size(); i++)  {
    result[i][0] = dim_results[0][i];
    result[i][1] = dim_results[1][i];
    result[i][2] = dim_results[2][i];
  }
}
