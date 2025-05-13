#ifndef _GPU_NEWTON_MAIN_HEADER_FILE
#define _GPU_NEWTON_MAIN_HEADER_FILE

#include <SDL3/SDL.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <array>
#include <chrono>
#include <random>

#define MASS_LOW (1e2)
#define MASS_HIGH (1e4)

#define PARTICLE_BOX_X (1.0)
#define PARTICLE_BOX_Y (1.0)
#define PARTICLE_BOX_Z (1.0)

#define NUM_PARTICLES (1e2)

#define PRINT_FPS (true)

/*
 * In degrees
 */
#define FOV (45.0f)

#define NEAR (0.1f)
#define FAR (1000.0f)

#define CAMERA_SPEED (1.0)
#define MOUSE_SENSITIVITY (0.2)

#define COMP_SHADER_FNAME "shaders/bin/dot_cs.comp"
#define VERT_SHADER_FNAME "shaders/bin/dot_vs.vert"
#define FRAG_SHADER_FNAME "shaders/bin/dot_fs.frag"

void log([[maybe_unused]] SDL_LogPriority pri, [[maybe_unused]] const char *fmt, ...)
{
#ifndef NDEBUG
	std::va_list list;
	va_start(list, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, pri, fmt, list);
	va_end(list);
#endif
}

std::ostream &operator<<(std::ostream &os, const glm::vec3 &vec)
{
	os << "VEC3(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
	return os;
}

struct SDLError final : std::exception {
	mutable std::string msg;
	template <class T>
	constexpr SDLError(T msg):
		msg(msg) {}

	virtual const char *what() const noexcept {
		msg.append(" | ");
		msg.append(SDL_GetError());
		return msg.data();
	}
};

class ParticleSet {
	private:
		struct __attribute__((packed)) Particle {
			glm::vec3 position;
			float mass;
			float padding;
			glm::vec3 velocity;
		};

		SDL_GPUDevice *gpuDevice;
		std::vector<Particle> particles;
		SDL_GPUBuffer *particleBuffer = nullptr;
		SDL_GPUTransferBuffer *transferBuffer = nullptr;

	public:
		void init(SDL_GPUDevice *gpuDevice, std::size_t numParticles);
		void upload();
		SDL_GPUBuffer *getBuffer() const;
		std::size_t getNum() const;
		void deinit();
};

struct CamInfo {
	glm::mat4 combined;
};

class GPUNewtonApp {
	private:
		SDL_Window *window                   = nullptr;
		SDL_GPUDevice *gpuDevice             = nullptr;
		SDL_GPUGraphicsPipeline *gpuPipeline = nullptr;
		SDL_GPUComputePipeline *compPipeline = nullptr;
		SDL_GPUCommandBuffer *cmdBuffer      = nullptr;

		ParticleSet particleSet;

		using Keyboard = std::array<bool, SDL_SCANCODE_COUNT>;
		Keyboard keyboard;

		glm::mat4 projMatrix = glm::mat4(1.0);
		glm::vec3 camPosition = glm::vec3(0.0, 0.0, -2);
		glm::vec3 camLookat = glm::vec3(0.0);

		using TimePoint = std::chrono::steady_clock::time_point;
		using Duration = std::chrono::duration<double, std::milli>;
		Duration delta;
		bool running = false;

		void loadDevice();

		template <class T>
		void loadShaderData(std::string &&name, T &shaderData);
		SDL_GPUShader *loadShader(
			std::string_view fname,
			std::uint32_t numSamplers,
			std::uint32_t numStorageTextures,
			std::uint32_t numStorageBuffers,
			std::uint32_t numUniformBuffers
		);
		void loadPipeline();
		void loadComputePipeline();

		TimePoint currentTime();
		void debugDelta();
		void handleEvents();

		void toggleMouseGrab();
		bool keyPressed(SDL_Keycode key, bool wait);
		void updateProjMatrix(int width, int height);
		void updateCameraMouse(float xrel, float yrel);
		void updateCameraPos(const Keyboard &keyboard);

		bool isWindowFocused();

		void createCombined(glm::mat4 &combined);


	public:
		GPUNewtonApp(std::string_view title, int width, int height);

		GPUNewtonApp() = delete;
		GPUNewtonApp(const GPUNewtonApp &) = delete;
		GPUNewtonApp(GPUNewtonApp &&) = delete;
		GPUNewtonApp &operator=(const GPUNewtonApp &) = delete;

		void loop();

		~GPUNewtonApp();
};

#endif //_GPU_NEWTON_MAIN_HEADER_FILE
