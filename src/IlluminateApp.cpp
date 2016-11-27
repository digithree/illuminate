#include "cinder/app/AppNative.h"
#include "cinder/Capture.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/gl.h"
#include "cinder/Vector.h"
#include "cinder/Camera.h"
#include "cinder/params/Params.h"
#include "OscArg.h"
#include "OscBundle.h"
#include "OscListener.h"
#include "OscMessage.h"

#define SCREEN_WIDTH 640 //1020
#define SCREEN_HEIGHT 480 //704

#define OSC_PORT    8000

using namespace ci;
using namespace ci::app;
using namespace std;

class IlluminateApp : public AppNative {
  public:
    const float ZOOM = 300, ML2R = -320, MT2B = -240, SKEW = 0, FEEDBACK = 0.9, HUE_ROT_SPD_FACTOR = 0.01f, NEW_FRAME_MIX = 0.f;
    
    // setup our functions/methods
    void prepareSettings(Settings *settings);
    void setup();
    //void resize();
    void update();
    void draw();
    void keyDown(KeyEvent event);
    
    // declare our variables
    CaptureRef	        mCapture;
    Surface         mImgSurface;
    Surface         mDisplaySurface;
    gl::Texture		imgTexture;
    
    params::InterfaceGl	mParams;
    
    // CAMERA Controls
    CameraPersp mCamPrep;
    Quatf				mSceneRotation;
    float				mCameraDistance;
    Vec3f				mEye, mCenter, mUp;
    
    Vec2f               mTrans;
    float               mMoveL2R;
    float               mMoveT2B;
    
    float               mSkew;
    
    float               mFeedback;
    bool                mBlurOn;
    
    bool                mHueModOn;
    float               mHueRotSpeed;
    float               mHuePosition;
    
    bool                mFlipHorz;
    
    float               mNewestFrameMix;
    
    osc::Listener       listener;
    
    cinder::Area        mDrawArea;
};

void IlluminateApp::prepareSettings( Settings *settings ) {
    settings->setFrameRate(60);
    //settings->enableSecondaryDisplayBlanking( false );
    settings->setWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (ci::Display::getDisplays().size() <= 1) {
        settings->setDisplay( ci::Display::getDisplays()[0] );
    } else {
        settings->setDisplay( ci::Display::getDisplays()[1] );
    }
    settings->setFullScreen();
    
    mDrawArea = cinder::Area();
}

void IlluminateApp::setup()
{
    // print the devices
    for( auto device = Capture::getDevices().begin(); device != Capture::getDevices().end(); ++device ) {
        console() << "Device: " << (*device)->getName() << " "
#if defined( CINDER_COCOA_TOUCH )
        << ( (*device)->isFrontFacing() ? "Front" : "Rear" ) << "-facing"
#endif
        << std::endl;
    }
    
    try {
        mCapture = Capture::create(SCREEN_WIDTH, SCREEN_HEIGHT);
        mCapture->start();
    }
    catch( ... ) {
        console() << "Failed to initialize capture" << std::endl;
    }
    
    mCameraDistance = ZOOM;
    mEye			= Vec3f( 0.0f, 0.0f, mCameraDistance );
    mCenter			= Vec3f::zero();
    mUp				= Vec3f::yAxis();
    mCamPrep.setPerspective(  75.0f, getWindowAspectRatio(), 5.0f, 2000.0f );
    
    mMoveL2R = ML2R;
    mMoveT2B = MT2B;
    mSkew = SKEW;
    
    mFeedback = FEEDBACK;
    
    mFlipHorz = true;
    
    mHueModOn = false;
    mHueRotSpeed = 0.f;
    mHuePosition = 0.f;
    
    mNewestFrameMix = NEW_FRAME_MIX;
    
    gl::enableAlphaBlending();
    
    /*
    // SETUP PARAMS
    mParams = params::InterfaceGl( "Setup Controls", Vec2i( 200, 160 ) );
    mParams.addParam( "Zoom", &mCameraDistance, "min=50.0 max=1500.0 step=5.0 keyIncr=q keyDecr=a" );
    mParams.addParam( "Move Left to Right", &mMoveL2R, "min=-1500.0 max=1500.0 step=5.0 keyIncr=w keyDecr=s" );
    mParams.addParam( "Move Top to Bottom", &mMoveT2B, "min=-1500.0 max=1500.0 step=5.0 keyIncr=e keyDecr=d" );
    mParams.addParam( "Skew", &mSkew, "min=-85.0 max=85.0 step=1.0 keyIncr=r keyDecr=f" );
    mParams.addParam( "Feedback", &mFeedback, "min=0.01 max=1.0 step=0.01 keyIncr=t keyDecr=g" );
    mParams.addParam( "Bur active", &mBlurOn, "" );
    mParams.addParam( "Hue rotation active", &mHueModOn, "" );
    mParams.addParam( "Hue rotation spd", &mHueRotSpeed, "min=0.00 max=1.0 step=0.01 keyIncr=y keyDecr=h" );
    mParams.addParam( "Flip horz", &mFlipHorz, "" );
    mParams.addParam( "Newest Frame Mix", &mNewestFrameMix, "min=0.00 max=1.0 step=0.01 keyIncr=u keyDecr=j" );
     */
    
    // Start OSC listener
    listener.setup(OSC_PORT);
}

void IlluminateApp::keyDown( KeyEvent event )
{
    switch( event.getCode() )
    {
        case KeyEvent::KEY_ESCAPE:
            quit();
            break;
        default:
            break;
    }
}

void IlluminateApp::update()
{
    // get osc messages
    if (listener.hasWaitingMessages()) {
        osc::Message message;
        while (listener.hasWaitingMessages()) {
            listener.getNextMessage(&message);
            console() << "OSC message: " << message.getAddress() << std::endl;
            // process message
            if (message.getAddress().compare("/1/zoom") == 0) {
                mCameraDistance = 50.f + (message.getArgAsFloat(0) * 1450.f);
            } else if (message.getAddress().compare("/1/move") == 0) {
                mMoveL2R = (message.getArgAsFloat(1) * 1500.f);
                mMoveT2B = (message.getArgAsFloat(0) * 1500.f);
            } else if (message.getAddress().compare("/1/skew") == 0) {
                mSkew = (message.getArgAsFloat(0) * 85.f);
            } else if (message.getAddress().compare("/1/horz_flip") == 0) {
                mFlipHorz = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/blur_switch") == 0) {
                mBlurOn = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/blur_amt") == 0) {
                mFeedback = 0.5f + (message.getArgAsFloat(0) * 0.5f);
            } else if (message.getAddress().compare("/1/col_rot_switch") == 0) {
                mHueModOn = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/col_rot_speed") == 0) {
                mHueRotSpeed = message.getArgAsFloat(0);
            } else if (message.getAddress().compare("/1/new_frame_mix") == 0) {
                mNewestFrameMix = 1.f - message.getArgAsFloat(0);
            } else {
                console() << "Didn't understand OSC mesage" << std::endl;
                console() << "- Num args: " << message.getNumArgs() << std::endl;
                for (int i = 0 ; i < message.getNumArgs() ; i++) {
                    console() << "--- Type of " << i << ": " << message.getArgType(i) << std::endl;
                }
            }
        }
    }
    
    mTrans.set(mMoveL2R, mMoveT2B);
    
    //char a = 'A' + 'B';
    //char *name = "Simon";
    //std::cout << "A + 1 is " << a << std::endl;
    
    // UPDATE VARS
    mHuePosition += (mHueRotSpeed * HUE_ROT_SPD_FACTOR);
    if (mHuePosition > (1.f)) {
        mHuePosition = 0.f;
    }
    //std::cout << "mHuePosition: " << mHuePosition << std::endl;
    
    // UPDATE CAMERA
    mEye = Vec3f( 0.0f, 0.0f, mCameraDistance );
    mCamPrep.lookAt( mEye, mCenter, mUp );
    gl::setMatrices( mCamPrep );
    gl::rotate(180);
    gl::rotate(Vec3f(mSkew, mFlipHorz ? 180 : 0, 0));
    gl::translate(mTrans);
    //gl::rotate( mSceneRotation );
    
    if (mCapture && mCapture->checkNewFrame()) {
        Surface newFrameSurface = mCapture->getSurface();
        if (mImgSurface) {
            Surface::Iter iterOldSurface = mImgSurface.getIter();
            Surface::Iter iterNewSurface = newFrameSurface.getIter();
            Surface::Iter iterDisplaySurface = mDisplaySurface.getIter();
            int c = 0;
            while (iterOldSurface.line() && iterNewSurface.line() && iterDisplaySurface.line()) {
                while (iterOldSurface.pixel() && iterNewSurface.pixel() && iterDisplaySurface.pixel()) {
                    if (mHueModOn) {
                        // hue rotation
                        Vec3f hsvVec = rgbToHSV(Colorf(iterNewSurface.r(), iterNewSurface.g(), iterNewSurface.b()));
                        hsvVec.x += mHuePosition;
                        if (hsvVec.x > 1.f) {
                            hsvVec.x -= 1.f;
                        }
                        Colorf pix = hsvToRGB(hsvVec);
                        iterNewSurface.r() = pix.r;
                        iterNewSurface.g() = pix.g;
                        iterNewSurface.b() = pix.b;
                    }
                    if (mBlurOn) {
                        // feedback
                        iterOldSurface.r() = (int)(((float)iterOldSurface.r()) * mFeedback);
                        iterOldSurface.g() = (int)(((float)iterOldSurface.g()) * mFeedback);
                        iterOldSurface.b() = (int)(((float)iterOldSurface.b()) * mFeedback);
                        // lighten
                        iterOldSurface.r() = iterNewSurface.r() > iterOldSurface.r() ? iterNewSurface.r() : iterOldSurface.r();
                        iterOldSurface.g() = iterNewSurface.g() > iterOldSurface.g() ? iterNewSurface.g() : iterOldSurface.g();
                        iterOldSurface.b() = iterNewSurface.b() > iterOldSurface.b() ? iterNewSurface.b() : iterOldSurface.b();
                    } else {
                        iterOldSurface.r() = iterNewSurface.r();
                        iterOldSurface.g() = iterNewSurface.g();
                        iterOldSurface.b() = iterNewSurface.b();
                    }
                    iterDisplaySurface.r() = (iterOldSurface.r() * (1 - mNewestFrameMix)) + (iterNewSurface.r() * mNewestFrameMix);
                    iterDisplaySurface.g() = (iterOldSurface.g() * (1 - mNewestFrameMix)) + (iterNewSurface.g() * mNewestFrameMix);
                    iterDisplaySurface.b() = (iterOldSurface.b() * (1 - mNewestFrameMix)) + (iterNewSurface.b() * mNewestFrameMix);
                    // inc c
                    c++;
                }
            }
            //std::cout << "Modified pixels: " << c << std::endl;
        } else {
            mImgSurface = newFrameSurface;
            mDisplaySurface = newFrameSurface.clone();
        }
        imgTexture = gl::Texture(mDisplaySurface);
    }
    
    // update draw area
    float multiplier = getWindowBounds().getWidth() / ((float)SCREEN_WIDTH);
    
    mDrawArea.set(0, 0, ((float)SCREEN_WIDTH) * multiplier, ((float)SCREEN_HEIGHT) * multiplier);
    if (mDrawArea.getHeight() < getWindowBounds().getHeight()) {
        multiplier = getWindowBounds().getHeight() / mDrawArea.getHeight();
        mDrawArea.set(0, 0, mDrawArea.getX2() * multiplier, mDrawArea.getY2() * multiplier);
    }
}

void IlluminateApp::draw()
{
    // clear out the window with black
    gl::clear( Color( 0, 0, 0.0f ), true );
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    if( imgTexture ) {
        gl::draw(imgTexture, mDrawArea);
    }else{
        gl::drawStringCentered("Loading image please wait..",getWindowCenter());
        
    }
    
    // Don't draw, replaced with OSC control
    //params::InterfaceGl::draw();
    //mParams.draw();
    
}

CINDER_APP_NATIVE( IlluminateApp, RendererGl )
