//
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
#include "XmlSettings.h"
#include "fileDialog.h"

#define OSC_PORT    8000

using namespace ci;
using namespace ci::app;
using namespace std;

//extern std::vector<std::string> openFileDialog();

class IlluminateApp : public AppNative {
  public:
    struct CaptureInfo {
        Capture::DeviceRef deviceRef;
        int listIdx;
        int width;
        int height;
    };
    
    const float ZOOM = 300, ML2R = -675, MT2B = -500, SKEW = 0, FEEDBACK = 0.9, HUE_ROT_SPD_FACTOR = 0.01f, NEW_FRAME_MIX = 0.f;
    const int FRAME_SKIP = 0;
    
    // setup our functions/methods
    void prepareSettings(Settings *settings);
    void setup();
    //void resize();
    void update();
    void draw();
    void keyDown(KeyEvent event);
    
    nocte::XmlSettings  mSettings;
    
    // Camera and image variables
    vector<CaptureInfo> mCaptureInfos;
    CaptureInfo         mCaptureInfo;
    std::string         camName;
    int                 camWidth;
    int                 camHeight;
    
    bool                mCameraActive;
    CaptureRef	        mCapture;
    Surface             mImgSurface;
    Surface             mDisplaySurface;
    gl::Texture         imgTexture;
    
    params::InterfaceGl	mParams;
    
    // CAMERA Controls
    CameraPersp         mCamPrep;
    Quatf				mSceneRotation;
    float				mCameraDistance;
    Vec3f				mEye, mCenter, mUp;
    
    Vec2f               mTrans;
    float               mMoveL2R;
    float               mMoveT2B;
    
    float               mSkew;
    
    float               mFeedback;
    int                 mFrameSkip;
    int                 mSkippedFrames;
    bool                mBlurOn;
    
    bool                mHueModOn;
    float               mHueRotSpeed;
    float               mHuePosition;
    float               mHueCenter;
    float               mHueWidth;
    bool                mHueDirection;
    
    bool                mFlipHorz;
    bool                mFlipVert;
    
    float               mNewestFrameMix;
    
    osc::Listener       listener;
    
    cinder::Area        mDrawArea;
    Rectf               mDrawAreaScreen;
    
    void selectCamera(int idx, bool resetZoom);
    // camera button callbacks
    void selectCamera0();
    void selectCamera1();
    void selectCamera2();
    void selectCamera3();
    void selectCamera4();
    void selectCamera5();
    void selectCamera6();
    void selectCamera7();
    
    void setupSettings();
    void loadSettings();
    void saveSettings();
};

void IlluminateApp::setupSettings() {
    mSettings.addParam("zoom", &mCameraDistance);
    mSettings.addParam("movel2r", &mMoveL2R);
    mSettings.addParam("movet2b", &mMoveT2B);
    mSettings.addParam("skew", &mSkew);
    mSettings.addParam("feedback", &mFeedback);
    mSettings.addParam("frameskip", &mFrameSkip);
    mSettings.addParam("bluron", &mBlurOn);
    mSettings.addParam("huemodon", &mHueModOn);
    mSettings.addParam("huecenter", &mHueCenter);
    mSettings.addParam("huewidth", &mHueWidth);
    mSettings.addParam("huerotspeed", &mHueRotSpeed);
    mSettings.addParam("fliphorz", &mFlipHorz);
    mSettings.addParam("flipvert", &mFlipVert);
    mSettings.addParam("newestframemix", &mNewestFrameMix);
    
    mSettings.addParam("camname", &camName);
    mSettings.addParam("camwidth", &camWidth);
    mSettings.addParam("camheight", &camHeight);
}

void IlluminateApp::saveSettings() {
    string filename = saveFileDialog(NULL);
    if (!filename.empty()) {
        mSettings.save(filename);
        console() << "saved settings from file" << endl;
    } else {
        console() << "error saving settings from file" << endl;
    }
}

void IlluminateApp::loadSettings() {
    string filename = openFileDialog(NULL);
    
    mSettings.load(filename);
    console() << "loaded settings from file " << filename << endl;
    if (!camName.empty() && camWidth > 0 && camHeight > 0) {
        if (mCaptureInfo.deviceRef != NULL
                && mCaptureInfo.deviceRef->getName().compare(camName) == 0) {
            console() << "same camera, no need to change camera on settings load" << endl;
        } else {
            for( vector<CaptureInfo>::const_iterator it = mCaptureInfos.begin(); it != mCaptureInfos.end(); ++it ) {
                CaptureInfo info = *it;
                if (info.deviceRef->getName().compare(camName) == 0) {
                    // start camera
                    mCaptureInfo = info;
                    try {
                        if (mCapture) {
                            mCapture->stop();
                            mCapture = NULL;
                        }
                        mCapture = Capture::create(mCaptureInfo.width, mCaptureInfo.height, mCaptureInfo.deviceRef);
                        mCapture->start();
                        console() << "Started capture: " << mCaptureInfo.deviceRef->getName() << ", " << mCaptureInfo.width << "x" << mCaptureInfo.height << endl;
                    }
                    catch( CaptureExc & ) {
                        console() << "Error starting capture " << mCaptureInfo.deviceRef->getName() << ", " << mCaptureInfo.width << "x" << mCaptureInfo.height << endl;
                    }
                }
            }
        }
    }
}

void IlluminateApp::prepareSettings( Settings *settings ) {
    settings->setFrameRate(60);
    //settings->enableSecondaryDisplayBlanking( false );
    settings->setWindowSize(800, 600); // TODO : change this
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
    setupSettings();
    
    int numCaptureResolutions = 7;
    int captureResolutionsWidth[] = {640, 800, 1024, 1280, 1920, 2048, 2560};
    int captureResolutionsHeight[] = {480, 600, 768, 720, 1080, 1152, 1440};
    
    mCaptureInfos = vector<CaptureInfo>();
    
    mCaptureInfo = CaptureInfo();
    mCaptureInfo.deviceRef = NULL;
    mCaptureInfo.listIdx = -1;
    mCaptureInfo.width = 0;
    mCaptureInfo.height = 0;
    
    vector<Capture::DeviceRef> devices( Capture::getDevices() );
    int c = 0;
    for( vector<Capture::DeviceRef>::const_iterator deviceIt = devices.begin(); deviceIt != devices.end(); ++deviceIt ) {
        Capture::DeviceRef device = *deviceIt;
        console() << "Found Device " << device->getName() << " ID: " << device->getUniqueId() << std::endl;
        for (int i = 0 ; i < numCaptureResolutions ; i++) {
            CaptureInfo info = CaptureInfo();
            info.deviceRef = device;
            info.listIdx = c;
            info.width = captureResolutionsWidth[i];
            info.height = captureResolutionsHeight[i];
            mCaptureInfos.push_back(info);
            c++;
        }
    }
    
    mCameraActive = false;
    
    mCameraDistance = ZOOM;
    mEye			= Vec3f( 0.0f, 0.0f, mCameraDistance );
    mCenter			= Vec3f::zero();
    mUp				= Vec3f::yAxis();
    mCamPrep.setPerspective(  75.0f, getWindowAspectRatio(), 5.0f, 2000.0f );
    
    mMoveL2R = ML2R;
    mMoveT2B = MT2B;
    mSkew = SKEW;
    
    mFeedback = FEEDBACK;
    mFrameSkip = FRAME_SKIP;
    mSkippedFrames = 0;
    
    mFlipHorz = true;
    mFlipVert = false;
    
    mHueModOn = false;
    mHueRotSpeed = 0.f;
    mHuePosition = 0.f;
    mHueCenter = 0.f;
    mHueWidth = 1.f;
    mHueDirection = true; //forward
    
    mNewestFrameMix = NEW_FRAME_MIX;
    
    gl::enableAlphaBlending();
    
    // SETUP PARAMS
    mParams = params::InterfaceGl( "Setup Controls", Vec2i( 200, 160 ) );
    mParams.addParam( "Zoom", &mCameraDistance, "min=50.0 max=1500.0 step=5.0 keyIncr=q keyDecr=a" );
    mParams.addParam( "Move Left to Right", &mMoveL2R, "min=-1500.0 max=1500.0 step=5.0 keyIncr=w keyDecr=s" );
    mParams.addParam( "Move Top to Bottom", &mMoveT2B, "min=-1500.0 max=1500.0 step=5.0 keyIncr=e keyDecr=d" );
    mParams.addParam( "Skew", &mSkew, "min=-85.0 max=85.0 step=1.0 keyIncr=r keyDecr=f" );
    mParams.addParam( "Feedback", &mFeedback, "min=0.000001 max=1.0 step=0.001 keyIncr=t keyDecr=g" );
    mParams.addParam( "Frame Skip", &mFrameSkip, "min=0 max=20 step=1 keyIncr=y keyDecr=h" );
    mParams.addParam( "Blur active", &mBlurOn, "" );
    mParams.addParam( "Hue rotation active", &mHueModOn, "" );
    mParams.addParam( "Hue rotation center", &mHueCenter, "min=0.00 max=1.0 step=0.01" );
    mParams.addParam( "Hue rotation width", &mHueWidth, "min=0.00 max=1.0 step=0.01" );
    mParams.addParam( "Hue rotation spd", &mHueRotSpeed, "min=0.00 max=1.0 step=0.01 keyIncr=y keyDecr=h" );
    mParams.addParam( "Flip horz", &mFlipHorz, "" );
    mParams.addParam( "Flip vert", &mFlipVert, "" );
    mParams.addParam( "Newest Frame Mix", &mNewestFrameMix, "min=0.00 max=1.0 step=0.01 keyIncr=u keyDecr=j" );
    mParams.addSeparator();
    mParams.addButton("Save settings", [&]{saveSettings();});
    mParams.addButton("Load settings", [&]{loadSettings();});
    mParams.addSeparator();
    if (mCaptureInfos.size() > 0) {
        // captures
        int c = 0;
        for( vector<CaptureInfo>::const_iterator it = mCaptureInfos.begin(); it != mCaptureInfos.end(); ++it ) {
            CaptureInfo info = *it;
            std::string infoStr = info.deviceRef->getName() + " " + std::to_string(info.width) + "x" + std::to_string(info.height);
            switch(c++) {
                case 0:
                    mParams.addButton(infoStr, [&]{selectCamera0();});
                    break;
                case 1:
                    mParams.addButton(infoStr, [&]{selectCamera1();});
                    break;
                case 2:
                    mParams.addButton(infoStr, [&]{selectCamera2();});
                    break;
                case 3:
                    mParams.addButton(infoStr, [&]{selectCamera3();});
                    break;
                case 4:
                    mParams.addButton(infoStr, [&]{selectCamera4();});
                    break;
                case 5:
                    mParams.addButton(infoStr, [&]{selectCamera5();});
                    break;
                case 6:
                    mParams.addButton(infoStr, [&]{selectCamera6();});
                    break;
                case 7:
                    mParams.addButton(infoStr, [&]{selectCamera7();});
                    break;
            }
            
        }
    } else {
        mParams.addText("No cameras detected");
    }
    
    // Start OSC listener
    listener.setup(OSC_PORT);
}

void IlluminateApp::selectCamera(int idx, bool resetZoom)
{
    mCameraActive = false;
    if (mCaptureInfos.size() > 0 && idx < mCaptureInfos.size()) {
        int c = 0;
        for( vector<CaptureInfo>::const_iterator it = mCaptureInfos.begin(); it != mCaptureInfos.end(); ++it ) {
            if (c++ == idx) {
                CaptureInfo info = *it;
                // start camera
                mCaptureInfo = info;
                try {
                    if (mCapture) {
                        mCapture->stop();
                        mCapture = NULL;
                    }
                    if (resetZoom) {
                        mCameraDistance = 1100;
                    }
                    mCapture = Capture::create(mCaptureInfo.width, mCaptureInfo.height, mCaptureInfo.deviceRef);
                    mCapture->start();
                    camName = mCaptureInfo.deviceRef->getName();
                    camWidth = mCaptureInfo.width;
                    camHeight = mCaptureInfo.height;
                    console() << "Started capture: " << mCaptureInfo.deviceRef->getName() << ", " << mCaptureInfo.width << "x" << mCaptureInfo.height << endl;
                }
                catch( CaptureExc & ) {
                    console() << "Error starting capture " << mCaptureInfo.deviceRef->getName() << ", " << mCaptureInfo.width << "x" << mCaptureInfo.height << endl;
                }
                return;
            }
        }
    }
    console() << "Could not start capture at list idx " << idx << endl;
}

void IlluminateApp::selectCamera0()
{
    selectCamera(0, true);
}

void IlluminateApp::selectCamera1()
{
    selectCamera(1, true);
}

void IlluminateApp::selectCamera2()
{
    selectCamera(2, true);
}

void IlluminateApp::selectCamera3()
{
    selectCamera(3, true);
}

void IlluminateApp::selectCamera4()
{
    selectCamera(4, true);
}

void IlluminateApp::selectCamera5()
{
    selectCamera(5, true);
}

void IlluminateApp::selectCamera6()
{
    selectCamera(6, true);
}

void IlluminateApp::selectCamera7()
{
    selectCamera(7, true);
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
            } else if (message.getAddress().compare("/1/vert_flip") == 0) {
                mFlipVert = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/frame_skip") == 0) {
                mFrameSkip = message.getArgAsInt32(0);
            } else if (message.getAddress().compare("/1/blur_switch") == 0) {
                mBlurOn = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/blur_amt") == 0) {
                mFeedback = message.getArgAsFloat(0);
            } else if (message.getAddress().compare("/1/col_rot_switch") == 0) {
                mHueModOn = message.getArgAsFloat(0) != 0.f;
            } else if (message.getAddress().compare("/1/col_rot_speed") == 0) {
                mHueRotSpeed = message.getArgAsFloat(0);
            } else if (message.getAddress().compare("/1/new_frame_mix") == 0) {
                mNewestFrameMix = 1.f - message.getArgAsFloat(0);
            } else if (message.getAddress().compare("/1/save") == 0) {
                saveSettings();
            } else if (message.getAddress().compare("/1/load") == 0) {
                loadSettings();
            } else {
                console() << "Didn't understand OSC mesage" << std::endl;
                console() << "- Num args: " << message.getNumArgs() << std::endl;
                for (int i = 0 ; i < message.getNumArgs() ; i++) {
                    console() << "--- Type of " << i << ": " << message.getArgType(i) << std::endl;
                }
            }
        }
    }
    
    if (mCapture && mCapture->isCapturing()) {
        mHuePosition += (mHueRotSpeed * HUE_ROT_SPD_FACTOR * (mHueDirection ? 1.f : -1.f));
        float upperBound = mHueCenter + (mHueWidth / 2.f);
        float lowerBound = mHueCenter - (mHueWidth / 2.f);
        if (mHuePosition > upperBound) {
            mHuePosition = upperBound;
            mHueDirection = false;
        } else if (mHuePosition < lowerBound) {
            mHuePosition = lowerBound;
            mHueDirection = true;
        }
        
        // UPDATE CAMERA
        mTrans.set(mMoveL2R, mMoveT2B);
        mEye = Vec3f( 0.0f, 0.0f, mCameraDistance );
        mCamPrep.lookAt( mEye, mCenter, mUp );
        gl::setMatrices( mCamPrep );
        gl::rotate(180);
        gl::rotate(Vec3f(mSkew, mFlipHorz ? 180 : 0, mFlipVert ? 180 : 0));
        gl::translate(mTrans);
    }
    
    if (mCapture && mCapture->checkNewFrame()) {
        mCameraActive = true;
        if (++mSkippedFrames >= mFrameSkip) {
            mSkippedFrames = 0;
        }
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
                        float boundedHuePosition = mHuePosition;
                        if (boundedHuePosition < 0.f) {
                            boundedHuePosition += 1.f;
                        } else if (boundedHuePosition > 1.f) {
                            boundedHuePosition -= 1.f;
                        }
                        hsvVec.x += mHuePosition;
                        if (hsvVec.x > 1.f) {
                            hsvVec.x -= 1.f;
                        } else if (hsvVec.x < 0.f) {
                            hsvVec.x += 1.f;
                        }
                        Colorf pix = hsvToRGB(hsvVec);
                        iterNewSurface.r() = pix.r;
                        iterNewSurface.g() = pix.g;
                        iterNewSurface.b() = pix.b;
                    }
                    if (mBlurOn) {
                        if (mSkippedFrames == 0) {
                            // feedback, using curve as applied to input number
                            float feedback = powf(mFeedback, 1.f / 3.f); // cube root, more values closer to 1.f
                            iterOldSurface.r() = (int)(((float)iterOldSurface.r()) * feedback);
                            iterOldSurface.g() = (int)(((float)iterOldSurface.g()) * feedback);
                            iterOldSurface.b() = (int)(((float)iterOldSurface.b()) * feedback);
                        } else {
                            // 100% feedback on skipped frames
                            //iterOldSurface.r() = iterOldSurface.r();
                            //iterOldSurface.g() = iterOldSurface.g();
                            //iterOldSurface.b() = iterOldSurface.b();
                        }
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
    
    if (mCaptureInfo.width > 0 && mCaptureInfo.height > 0) {
        mDrawArea.set(0, 0, mCaptureInfo.width, mCaptureInfo.height);
        // update draw area
        float multiplier = getWindowBounds().getWidth() / ((float)mCaptureInfo.width);
        mDrawAreaScreen = Rectf(0, 0, ((float)mCaptureInfo.width) * multiplier, ((float)mCaptureInfo.height) * multiplier);
        if (mDrawArea.getHeight() < getWindowBounds().getHeight()) {
            multiplier = getWindowBounds().getHeight() / ((float)mCaptureInfo.height);
            mDrawAreaScreen = Rectf(0, 0, mDrawArea.getX2() * multiplier, mDrawArea.getY2() * multiplier);
        }
    }
}

void IlluminateApp::draw()
{
    // clear out the window with black
    gl::clear( Color( 0, 0, 0.0f ), true );
    gl::enableDepthRead();
    gl::enableDepthWrite();
    
    if(mCameraActive) {
        gl::draw(imgTexture, mDrawArea, mDrawAreaScreen);
    } else if (mCapture) {
        gl::drawStringCentered("Waiting for camera...\n\nIf this takes a long time\nthere is a problem", getWindowCenter());
    } else {
        gl::drawStringCentered("Please select a camera from the menu", getWindowCenter());
    }
    
    // Don't draw, replaced with OSC control
    //params::InterfaceGl::draw();
    mParams.draw();
    
}

CINDER_APP_NATIVE( IlluminateApp, RendererGl )
