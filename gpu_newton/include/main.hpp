#ifndef _GPU_NEWTON_MAIN_HEADER_FILE
#define _GPU_NEWTON_MAIN_HEADER_FILE

#include <SDL3/SDL.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdarg>

/*
 * In degrees
 */
#define FOV (45.0f)

#define NEAR (0.1f)
#define FAR (1000.0f)

#define MOUSE_SENSITIVITY (2.0)

#define VERT_SHADER_FNAME "shaders/bin/dot_vs.vert.msl"
#define FRAG_SHADER_FNAME "shaders/bin/dot_fs.frag.msl"

#define SHADER_FORMAT (SDL_GPU_SHADERFORMAT_MSL)

void log([[maybe_unused]] SDL_LogPriority pri, [[maybe_unused]] const char *fmt, ...)
{
#ifndef NDEBUG
	std::va_list list;
	va_start(list, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, pri, fmt, list);
	va_end(list);
#endif
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

struct ParticleVertexData {
	unsigned int index;
};

class GPUNewtonApp {
	private:
		SDL_Window *window                   = nullptr;
		SDL_GPUDevice *gpuDevice             = nullptr;
		SDL_GPUGraphicsPipeline *gpuPipeline = nullptr;
		SDL_GPUCommandBuffer *cmdBuffer      = nullptr;

		using Keyboard = std::array<bool, SDL_SCANCODE_COUNT>;
		glm::mat4 projMatrix = glm::mat4(1.0);
		glm::vec3 camPosition = glm::vec3(0.0);
		glm::vec3 camLookat = glm::vec3(0.0);

		bool running;

		SDL_GPUShaderFormat getAvailableShaderFormats();

		void loadDevice();
		SDL_GPUShader *loadShader(
			std::string_view fname,
			SDL_GPUShaderFormat fmt,
			std::uint32_t numSamplers,
			std::uint32_t numStorageTextures,
			std::uint32_t numStorageBuffers,
			std::uint32_t numUniformBuffers
		);
		void loadPipeline();

		bool keyPressed(const Keyboard &keyboard, SDL_Keycode key);
		void updateProjMatrix(int width, int height);
		void updateCamera(const glm::vec2 &mouse, const Keyboard &keyboard);

		void createCombined(glm::mat4 &combined);

		void handleEvents();

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
