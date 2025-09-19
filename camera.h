#ifndef CAMERA_H
#define CAMERA_H

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
		front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
		front.y = sin(glm::radians(Pitch));
		front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
		Front = glm::normalize(front);

		Right = glm::normalize(glm::cross(Front, WorldUp));
		Up = glm::normalize(glm::cross(Right, Front));
	}
};

#endif
