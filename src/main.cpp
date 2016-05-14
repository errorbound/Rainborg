#include "main.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <time.h>
#include "Simulation.h"
#include "FluidSimpleGravityForce.h"
#include "FluidBoundingBox.h"
#include "FluidBrick.h"
#include "GridGPUFluid.h"
#include "NaiveGPUFluid.h"
#include "MathDefs.h"
#include "StringUtilities.h"
#include "SceneXMLParser.h"
#include <tclap/CmdLine.h>
#include <stdlib.h>
#include <openglframework.h>
#include "Simulation.h"
#include "TimingUtilities.h"
#include "YImage.h"

#ifdef GPU_ENABLED
#include <cuda.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <helper_cuda.h>
#include <helper_cuda_gl.h>
#endif

using namespace openglframework;

#ifdef GPU_ENABLED

#define GPU_CHECKERROR(err) (gpuCheckError(err, __FILE__, __LINE__))
static void gpuCheckError(cudaError_t err, const char *file, int line){
    if(err != cudaSuccess){
        fprintf(stderr, "%s in %s at line %d\n", cudaGetErrorString(err), file, line);
    }   
}

#endif


// callback functions for GLFW
void idle();
void display(int width, int height);
void reshape(GLFWwindow* window, int width, int height);
void mouseButton(GLFWwindow* window, int button, int action, int mods);
void mouseMotion(GLFWwindow* window, double x, double y);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
void errorCallback(int error, const char* description);

// other functions
void initializeOpenGLandGLFW();
void headlessSimLoop();
void stepSystem();
void dumpPNG(const std::string &filename);

// Global variables
Simulation* g_simulation;
openglframework::GLFWViewer* g_viewer;
openglframework::Color g_bgcolor(0.0, 0.0, 0.0, 1.0);

bool g_rendering_enabled = true;
double g_sec_per_frame;
double g_last_time = timingutils::seconds();
int g_num_steps = 0;
scalar g_dt = 0.0;

// Timing
double totalTime = 0;
double avgTime = 0;
struct timeval startTime, endTime;
int startStep;

// Simulation state
bool g_paused = true;
int g_current_step = 0;
bool g_simulation_ran_to_completion = false;

// Parser state
std::string g_xml_scene_file;
std::string g_description;

// gpu mode? vs. serial
bool g_gpu_mode;


void printTimingResults(){
    gettimeofday(&endTime, 0);
	    
    totalTime = (1000000.0*(endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec - startTime.tv_usec))/1000.0; // milliseconds
    avgTime = totalTime / (g_current_step - startStep); 
	if(g_current_step == startStep) // didn't actually do anything
		return;
    std::cout << "Total time: " << totalTime << " ms" << std::endl;
    std::cout << "Avg. frame time: " << avgTime << " ms "
              << "(" << 1000.0f/avgTime << ") fps" << std::endl;
    std::cout << "start time: " << startTime.tv_sec << std::endl;
    std::cout << "end time: " << endTime.tv_sec << std::endl;
}


// Parse the command line to get options
void parseCommandLine(int argc, char **argv) {

    try
    {
      TCLAP::CmdLine cmd("Position-Based Fluid Sim");

      // XML scene file to load
      TCLAP::ValueArg<std::string> scene("s", "scene", "Simulation to run; an xml scene file", true, "", "string", cmd);

      // Begin the scene paused or running
      TCLAP::ValueArg<bool> paused("p", "paused", "Begin the simulation paused if 1, running if 0", false, true, "boolean", cmd);

      // Run the simulation with rendering enabled or disabled
      TCLAP::ValueArg<bool> display("d", "display", "Run the simulation with display enabled if 1, without if 0", false, true, "boolean", cmd);

      // gpu mode
      TCLAP::ValueArg<bool> gpu_mode("g", "gpu-mode", "Simulate using GPU if 1, without if 0", false, false, "boolean", cmd);

      cmd.parse(argc, argv);

      g_xml_scene_file = scene.getValue();
      g_paused = paused.getValue();
      g_rendering_enabled = display.getValue();

      g_gpu_mode = gpu_mode.getValue();

      #ifndef GPU_ENABLED
        if (g_gpu_mode) {
            std::cerr << "CUDA not available! Compile with flag GPU_ENABLED (cmake .. -DGPU_ENABLED=ON)" << std::endl;
            exit(1);
        }
      #endif

    }
    catch (TCLAP::ArgException& e)
    {
      std::cerr << "error: " << e.what() << std::endl;
      exit(1);
    }

}

void loadScene( const std::string& file_name) {

    // Maximum time in the simulation to run for. This has nothing to do with run time, cpu time, etc. This is time in the 'virtual world'.
    scalar max_time;
    // Maximum frequency, in wall clock time, to execute the simulation for. This serves as a cap for simulations that run too fast to see a solution.
    scalar steps_per_sec_cap = 100000.0;

    // Load the simulation and pieces of rendring and UI startgreen
    assert( g_simulation == NULL );
    // XML parse scene here
    SceneXMLParser xml_scene_parser;
    xml_scene_parser.loadSimulation( file_name, g_rendering_enabled, g_gpu_mode, g_viewer, &g_simulation,
                                    g_dt, max_time, steps_per_sec_cap, g_bgcolor, g_description);

    g_simulation->load();

    assert( g_simulation != NULL );

    // To cap the framerate, compute the minimum time a single timestep should take
    g_sec_per_frame = 1.0/steps_per_sec_cap;
    // Integer number of timesteps to take
    g_num_steps = ceil(max_time/g_dt);
    // We begin at the 0th timestep
    g_current_step = 0;
}

bool loaded = false;

int main(int args, char **argv)
{
    srand(time(NULL));
    #ifdef GPU_ENABLED
    if(g_gpu_mode){
        //initGPUFluid();
    }
    #endif

    parseCommandLine(args, argv);

    if (g_rendering_enabled)
        initializeOpenGLandGLFW();

    loadScene(g_xml_scene_file);

    #ifdef GPU_ENABLED
    if(g_gpu_mode && g_rendering_enabled){
        GPU_CHECKERROR(cudaGLSetGLDevice( gpuGetMaxGflopsDeviceId() ));
    }
    #endif

    if(g_rendering_enabled)
        g_simulation->prepareForRender();
 
    std::cout << outputmod::startblue << "Scene: " << outputmod::endblue << g_xml_scene_file << std::endl;
    std::cout << outputmod::startblue << "Description: " << outputmod::endblue << g_description << std::endl;

    if (!g_rendering_enabled)
        gettimeofday(&startTime, 0);
    startStep = 0;

    if (g_rendering_enabled)
        g_viewer->mainLoop();
    else
        headlessSimLoop();

    printTimingResults();

    delete g_simulation;
    return 0;
}

// Step the system, calling the simulation's own stepSystem
void stepSystem() {

    // Determine if the simulation is complete
    if( g_current_step >= g_num_steps )
    {
        std::cout << outputmod::startpink << "PBF message: " << outputmod::endpink << "Simulation complete at time " << g_current_step*g_dt << ". Exiting." << std::endl;
        g_simulation_ran_to_completion = true;
        #ifdef GPU_ENABLED
        // cleanUpGPUFluid();
        #endif
        printTimingResults();
 
        exit(0);
   }

      // Step the system forward in time
    if(g_gpu_mode){
        #ifdef GPU_ENABLED
        // stepSystemGPUFluid(g_dt);
        #endif
        g_simulation->stepSystem(g_dt);
    }
    else{
        g_simulation->stepSystem(g_dt);
    }

    //if (g_rendering_enabled) {
        // std::cout << outputmod::startgreen << "Time step: " << outputmod::endgreen
                  // << (g_current_step*g_dt) << std::endl;
    //}
    
    g_current_step++;

    if (g_rendering_enabled) {
#ifdef PNGOUT
		// Save frame to png
        std::stringstream oss;
        oss << "pngs/frame" << std::setw(5) << std::setfill('0') << g_current_step << ".png";
        dumpPNG(oss.str());
#endif
    }

}

void headlessSimLoop() {

    scalar nextpercent = 0.02;
    std::cout << outputmod::startpink << "Progress: " << outputmod::endpink;
    for( int i = 0; i < 50; ++i ) std::cout << "-";
    std::cout << std::endl;
    std::cout << "          ";
    while( true )
    {
      scalar percent_done = ((double)g_current_step)/((double)g_num_steps);
      if( percent_done >= nextpercent )
      {
        nextpercent += 0.02;
        std::cout << "." << std::flush;
      }
      stepSystem();
    }
}

// idle function from GLFW
void idle() {

    // Trigger the next timestep
    double current_time = timingutils::seconds();
    if( !g_paused && current_time-g_last_time >= g_sec_per_frame )
    {
      g_last_time = current_time;
      stepSystem();
    }
}

// Initialization
void initializeOpenGLandGLFW() {

    g_viewer = new openglframework::GLFWViewer();
    openglframework::Vector2 windowsSize = openglframework::Vector2(600, 400);
    bool initOK = g_viewer->init("OpenGL Framework Demo", windowsSize);
    if (!initOK) {
        std::cerr << "Error initializing GLFW viewer" << std::endl;
        exit(1);
        return;
    }

    g_viewer->bindReshapeCallback(reshape);
    g_viewer->bindKeyboardCallback(keyboard);
    g_viewer->bindMouseButtonCallback(mouseButton);
    g_viewer->bindMouseMotionCallback(mouseMotion);
    g_viewer->bindDisplayCallback(display);
    g_viewer->bindIdleCallback(idle);

    g_viewer->setScenePosition(openglframework::Vector3(5.0, 5.0, 0.0), 20);   
    // Define the background color (black)
    glClearColor(g_bgcolor.r, g_bgcolor.g, g_bgcolor.b, 1.0);
}

// Reshape function
void reshape(GLFWwindow* window, int width, int height) {
    g_viewer->reshape(width, height);
}

// Called when a mouse button event occurs
void mouseButton(GLFWwindow* window, int button, int action, int mods) {
    g_viewer->mouseButtonEvent(button, action, mods);
}

// Called when a mouse motion event occurs
void mouseMotion(GLFWwindow* window, double x, double y) {
    g_viewer->mouseMotionEvent(x, y);
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
    g_viewer->keyboard(key, scancode, action, mods);

    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        stepSystem();
    }
    else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        g_paused = !g_paused;
        if (!g_paused) {
            gettimeofday(&startTime, 0);
            startStep = g_current_step;
        }
    }
}

// Display the scene
void display(int width, int height) {
    g_simulation->display(g_viewer, width, height);
}

void errorCallback(int error, const char* description)
{
    fputs(description, stderr);
}

void dumpPNG(const std::string &filename)
{
  #ifdef PNGOUT
    YImage image;
    image.resize(g_viewer->getWindowWidth(), g_viewer->getWindowHeight());

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glReadBuffer(GL_BACK);

    glFinish();
    glReadPixels(0, 0, g_viewer->getWindowWidth(), g_viewer->getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    image.flip();

    image.save(filename.c_str());
  #endif
}
