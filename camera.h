#ifndef CAMERA_H
#define CAMERA_H

#include <string>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>
#include "libs/glm/glm.hpp"
#include "libs/glm/geometric.hpp"
#include "libs/glm/gtc/matrix_transform.hpp"

// define movement directions as named constants to abstract from window-system specific input handling
enum Camera_Movement
{
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

// default camera values
const float PITCH = 0.0f;
const float YAW = -90.0f;
const float MAXSPEED = 50.0f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;
float const ACCELERATION = 10.0f;
float const DECELERATION = 30.0f;
glm::vec3 const STARTPOS = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 const WORLDUP = glm::vec3(0.0f, 1.0f, 0.0f);

// default camera start position and angle for Terrain Viewer mode (position, pitch, yaw)
// clang-format off
const std::unordered_map<std::string, std::tuple<glm::vec3, float, float>> terrainSpawnPos = {
    {"pvp_forsaken_shrine",     {{-125, 85, 160},   -50.0f,   0.0f}},
    {"pvp_garrison_quarter",    {{95, 70, 250},     -35.0f,  -50.0f}},
    {"pvp_mephitis_backwoods",  {{40, 30, 110},     -35.0f, -100.0f}},
    {"pvp_merciless_ring",      {{-40, 50, 400},    -30.0f,  -40.0f}},
    {"pvp_arena_of_courage",    {{110, 130, 230},   -30.0f,  -40.0f}},
    {"pvp_the_lost_city",       {{-280, 70, -330},  -15.0f, -125.0f}},
    {"1_relic's_key",           {{-222, 42, 214},     0.0f,  -25.0f}},
    {"2_knahswahs_prison",      {{-545, 15, -2325},   0.0f,  -90.0f}},
    {"3_young_deity's_realm",   {{-115, -8, 150},     0.0f,   90.0f}},
    {"4_sailen_the_lower_city", {{1528, 12, -1080},  10.0f,  140.0f}},
    {"6_eidolon's_horizon",     {{-156, 53, -1600},   5.0f, -100.0f}},
    {"tanned_land",             {{-2600, 120, 195}, -30.0f, -130.0f}},
    {"sandbox",                 {{1110, 10, 700},     0.0f,   70.0f}},
    {"human_selection",         {{1200, 120, 40},   -20.0f,   45.0f}},
    {"amusement_park1",         {{-975, 50, -160},   10.0f, -280.0f}},
    {"amusement_park2",         {{1214, 7, 351},      5.0f, -370.0f}},
    {"ghost_island",            {{-625, 87, -590},    0.0f,  685.0f}},
    {"flare_island",            {{-1373, 45, -430},   0.0f,  -50.0f}},
	{"hanging_gardens",         {{1509, 265, 805},    0.0f, -110.0f}},
    {"polynia",                 {{901, 3, -217},      6.0f, -150.0f}},
    {"greenmont",               {{-183, 60, -20},   -21.0f,   65.0f}}
};

class Camera
{
  public:
	// camera attributes
	glm::vec3 inputDir; // camera movement direction in world space based on current keyboard input
	glm::vec3 moveDir;	// camera accumulated movement direction in world space
	glm::vec3 Position; // camera position in world space
	glm::vec3 WorldUp;	// positive y-axis in world space
	glm::vec3 Front;	// negative z-axis (camera view direction in view space)
	glm::vec3 Up;		// positive y-axis
	glm::vec3 Right;	// positive x-axis

	// Euler angles
	float Pitch; // vertical rotation (look up / down)
	float Yaw;	 // horizontal rotation (look left / right)

	// camera options
	float MovementSpeed;
	float MouseSensitivity;
	float Zoom;

	// constructor that initializes camera orientation vectors
	Camera()
	{
		Position = STARTPOS;
		WorldUp = WORLDUP;
		Pitch = PITCH;
		Yaw = YAW;
		MouseSensitivity = SENSITIVITY;
		Zoom = ZOOM;
		updateCameraVectors();
	}

	//! Returns the LookAt view matrix calculated using Euler Angles.
	glm::mat4 GetViewMatrix()
	{
		return glm::lookAt(Position, Position + Front, Up);
	}

	//! Processes input received from a mouse scroll-wheel event; only requires input on the vertical wheel-axis.
	void ProcessMouseScroll(float yoffset)
	{
		Zoom -= (float)yoffset;

		if (Zoom < 1.0f)
			Zoom = 1.0f;
		if (Zoom > 45.0f)
			Zoom = 45.0f;
	}

	//! Processes input received from a mouse input system; expects the offset value in both the x and y direction.
	void ProcessMouseMovement(float xoffset, float yoffset)
	{
		xoffset *= MouseSensitivity;
		yoffset *= MouseSensitivity;

		Yaw += xoffset;
		Pitch += yoffset;

		if (Pitch > 89.0f)
			Pitch = 89.0f;
		if (Pitch < -89.0f)
			Pitch = -89.0f;

		updateCameraVectors();
	}

	//! Processes input received from any keyboard-like input system; accepts input parameter in the form of camera defined enum (to abstract it from windowing systems) and only updates inputDir vector. The actual movement logic is handled separately, allowing the camera to continue moving even when no keys are pressed.
	void ProcessKeyboard(Camera_Movement dir)
	{
		switch (dir)
		{
		case FORWARD:
			inputDir += Front;
			break;
		case BACKWARD:
			inputDir -= Front;
			break;
		case LEFT:
			inputDir -= Right;
			break;
		case RIGHT:
			inputDir += Right;
			break;
		}
	}

	//! Performs movement. moveDir vector is updated only if at least one key is pressed in the current frame; otherwise, it remains the same and the camera decelerates to a zero.
	void UpdatePosition(float dt)
	{
		if (inputDir != glm::vec3(0.0f))
		{
			moveDir = glm::normalize(inputDir);									   // normalization applied to ensure moveDir unit length, so that movement speed stays constant regardless of how many direction inputs (keys pressed) summed into inputDir
			MovementSpeed = std::min(MovementSpeed + ACCELERATION * dt, MAXSPEED); // input → accelerate
		}
		else
			MovementSpeed = std::max(MovementSpeed - DECELERATION * dt, 0.0f); // no input → decelerate

		Position += moveDir * MovementSpeed * dt;

		inputDir = glm::vec3(0.0f);
	}

	//! Calculates the new Front vector from the camera's updated Euler Angles, and also updates Right and Up vectors (private helper function, not for external use).
	void updateCameraVectors()
	{
		glm::vec3 front;
		front.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
		front.y = std::sin(glm::radians(Pitch));
		front.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
		Front = glm::normalize(front);

		Right = glm::normalize(glm::cross(Front, WorldUp));
		Up = glm::normalize(glm::cross(Right, Front));
	}
};

#endif
