#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class IlluminateApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
};

void IlluminateApp::setup()
{
}

void IlluminateApp::mouseDown( MouseEvent event )
{
}

void IlluminateApp::update()
{
}

void IlluminateApp::draw()
{
	// clear out the window with black
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP_NATIVE( IlluminateApp, RendererGl )
