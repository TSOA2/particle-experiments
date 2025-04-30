#include <main.hpp>

void GPUNewtonApp::loadDevice()
{
	const auto desiredShaderFormats =
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_MSL;

#ifdef NDEBUG
	const bool debugMode = false;
#else
	const bool debugMode = true;
#endif

	gpuDevice = SDL_CreateGPUDevice(desiredShaderFormats, debugMode, nullptr);
	if (!gpuDevice)
		throw SDLError("failed to create GPU device");

#ifndef NDEBUG
	log(SDL_LOG_PRIORITY_INFO, "using GPU device driver: %s", SDL_GetGPUDeviceDriver(gpuDevice));
#endif

	if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window))
		throw SDLError("unable to set GPU device window");
}

SDL_GPUShader *GPUNewtonApp::loadShader(
	std::string_view fname,
	std::uint32_t numSamplers,
	std::uint32_t numStorageTextures,
	std::uint32_t numStorageBuffers,
	std::uint32_t numUniformBuffers)
{
	std::string fileName(fname);
	SDL_GPUShaderStage stage;
	if (SDL_strstr(fileName.data(), ".vert"))
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	else if (SDL_strstr(fileName.data(), ".frag"))
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	else
		throw std::runtime_error("invalid shader file extension (vert/frag)");

	SDL_GPUShaderFormat fmt = SDL_GetGPUShaderFormats(gpuDevice);
	const char *entryPoint = "main";
	if (fmt & SDL_GPU_SHADERFORMAT_MSL) {
		fileName.append(".msl");
		entryPoint = "main0";
		fmt = SDL_GPU_SHADERFORMAT_MSL;
	} else if (fmt & SDL_GPU_SHADERFORMAT_SPIRV) {
		fileName.append(".spv");
		fmt = SDL_GPU_SHADERFORMAT_SPIRV;
	} else
		throw std::runtime_error("shader formats not supported on this machine");

	std::size_t codeSize;
	void *data = SDL_LoadFile(fileName.data(), &codeSize);
	if (!data)
		throw SDLError("failed to read shader file");

	SDL_GPUShaderCreateInfo createInfo{};
	createInfo.code_size = codeSize;
	createInfo.code = static_cast<const std::uint8_t *>(data);
	createInfo.entrypoint = entryPoint;
	createInfo.format = fmt;
	createInfo.stage = stage;
	createInfo.num_samplers = numSamplers;
	createInfo.num_storage_textures = numStorageTextures;
	createInfo.num_storage_buffers = numStorageBuffers;
	createInfo.num_uniform_buffers = numUniformBuffers;
	createInfo.props = 0;

	SDL_GPUShader *shader = SDL_CreateGPUShader(gpuDevice, &createInfo);
	if (!shader)
		throw SDLError("failed to create shader");

	SDL_free(data);
	return shader;
}

void GPUNewtonApp::loadPipeline()
{
	SDL_GPUShader *vertShader = loadShader(VERT_SHADER_FNAME, 0, 0, 0, 1);
	SDL_GPUShader *fragShader = loadShader(FRAG_SHADER_FNAME, 0, 0, 0, 0);

	SDL_GPUColorTargetDescription colorDesc{};
	colorDesc.format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window),
	colorDesc.blend_state = {
		.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
		.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA,
		.color_blend_op = SDL_GPU_BLENDOP_ADD,
		.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR,
		.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR,
		.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
		.enable_blend = true
	};

	SDL_GPUGraphicsPipelineCreateInfo createInfo{};
	createInfo.vertex_shader = vertShader;
	createInfo.fragment_shader = fragShader;
	createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	createInfo.rasterizer_state = SDL_GPURasterizerState {
		.fill_mode = SDL_GPU_FILLMODE_FILL
	};
	createInfo.target_info = SDL_GPUGraphicsPipelineTargetInfo {
		.color_target_descriptions = &colorDesc,
		.num_color_targets = 1
	};
	gpuPipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &createInfo);
	SDL_ReleaseGPUShader(gpuDevice, vertShader);
	SDL_ReleaseGPUShader(gpuDevice, fragShader);
	if (!gpuPipeline)
		throw SDLError("failed to load graphics pipeline");
}

void GPUNewtonApp::updateProjMatrix(int width, int height)
{
	float aspect =
		static_cast<float>(width) /
		static_cast<float>(height);
	projMatrix = glm::perspective(glm::radians(FOV), aspect, NEAR, FAR);
}

bool GPUNewtonApp::keyPressed(SDL_Keycode key, [[maybe_unused]] bool wait)
{
	return keyboard[SDL_GetScancodeFromKey(key, nullptr)];
}

void GPUNewtonApp::updateCamera(const glm::vec2 &mouse, const Keyboard &keyboard)
{
	/* Where the camera is looking */
	{
		static float yaw;
		static float pitch;

		yaw += glm::radians(mouse.x * MOUSE_SENSITIVITY);
		pitch -= glm::radians(mouse.y * MOUSE_SENSITIVITY);

		const float rightAngle = glm::pi<float>() / 2;
		const float topLimit = rightAngle - 0.1;
		if (pitch > topLimit)
			pitch = topLimit;
		else if (pitch < -topLimit)
			pitch = -topLimit;

		const float x = glm::cos(yaw) * glm::cos(pitch);
		const float y = glm::sin(pitch);
		const float z = glm::sin(yaw) * glm::cos(pitch);

		camLookat = glm::vec3(x, y, z);
	}

	/* Where the camera is */
	{
		if (keyPressed(SDLK_W, false))
			camPosition += camLookat * static_cast<float>(delta / std::chrono::seconds(1));

		if (keyPressed(SDLK_S, false))
			camPosition -= camLookat * static_cast<float>(delta / std::chrono::seconds(1));
	}
}


void GPUNewtonApp::createCombined(glm::mat4 &combined)
{
	glm::mat4 viewMatrix = glm::lookAt(camPosition, camPosition + camLookat, glm::vec3(0.0, 1.0, 0.0));
	combined = projMatrix * viewMatrix;
}

GPUNewtonApp::GPUNewtonApp(std::string_view title, int width, int height)
{
	const SDL_InitFlags initFlags = SDL_INIT_VIDEO;
	const SDL_WindowFlags windowFlags = SDL_WINDOW_HIDDEN;

	if (!SDL_Init(initFlags))
		throw SDLError("failed to init SDL");

	window = SDL_CreateWindow(title.data(), width, height, windowFlags);
	if (!window)
		throw SDLError("failed to create window");

	log(SDL_LOG_PRIORITY_INFO, "loading GPU device and pipeline...");
	loadDevice();
	loadPipeline();

	SDL_ShowWindow(window);

	updateProjMatrix(width, height);
	keyboard.fill(false);
}

GPUNewtonApp::TimePoint GPUNewtonApp::currentTime()
{
	return std::chrono::steady_clock::now();
}

void GPUNewtonApp::debugDelta()
{
#if !defined(NDEBUG) && PRINT_FPS
	static TimePoint last;

	const auto fps = std::chrono::seconds(1) / delta;
	if ((currentTime() - last) >= std::chrono::seconds(1)) {
		std::cout << "Delta time: " << delta << ", FPS: " << fps << std::endl;
		last = currentTime();
	}
#endif
}

void GPUNewtonApp::toggleMouseGrab()
{
	static bool enabled;
	enabled = !enabled;
	SDL_SetWindowRelativeMouseMode(window, enabled);
}

void GPUNewtonApp::handleEvents()
{
	SDL_Event event;

	mouse = glm::vec2(0, 0);
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				updateProjMatrix(event.window.data1, event.window.data2);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				mouse.x = event.motion.xrel;
				mouse.y = event.motion.yrel;
				break;
			case SDL_EVENT_KEY_DOWN:
				keyboard[event.key.scancode] = true;
				break;
			case SDL_EVENT_KEY_UP:
				keyboard[event.key.scancode] = false;
				break;
		}
	}

	if (keyPressed(SDLK_ESCAPE, true))
		toggleMouseGrab();
}

void GPUNewtonApp::loop()
{
	running = true;

	TimePoint lastTime = currentTime();
	while (running) {
		handleEvents();

		TimePoint thisTime = currentTime();
		delta = thisTime - lastTime;
		debugDelta();

		/* Simulation code here */
		{
			updateCamera(mouse, keyboard);
		}
		/* End of simulation code */

		lastTime = thisTime;

		cmdBuffer = SDL_AcquireGPUCommandBuffer(gpuDevice);
		if (!cmdBuffer) {
			log(SDL_LOG_PRIORITY_ERROR, "failed to acquire command buffer: %s", SDL_GetError());
			continue;
		}

		SDL_GPUTexture *swapTexture;
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuffer, window, &swapTexture, nullptr, nullptr))
			continue;

		if (swapTexture) {
			glm::mat4 combined;
			createCombined(combined);

			SDL_GPUColorTargetInfo colorTargetInfo{};
			colorTargetInfo.texture = swapTexture;
			colorTargetInfo.clear_color = SDL_FColor(0.1f, 0.0f, 0.2f, 1.0f);
			colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmdBuffer, &colorTargetInfo, 1, nullptr);
			if (!renderPass) {
				log(SDL_LOG_PRIORITY_ERROR, "failed to acquire render pass: %s", SDL_GetError());
				continue;
			}

			SDL_BindGPUGraphicsPipeline(renderPass, gpuPipeline);

			SDL_PushGPUVertexUniformData(cmdBuffer, 0, glm::value_ptr(combined), sizeof(glm::mat4));
			SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(renderPass);
		} else {
			log(SDL_LOG_PRIORITY_INFO, "swapchain skip frame");
		}

		SDL_SubmitGPUCommandBuffer(cmdBuffer);
	}
}

GPUNewtonApp::~GPUNewtonApp()
{
	if (gpuPipeline)
		SDL_ReleaseGPUGraphicsPipeline(gpuDevice, gpuPipeline);

	if (gpuDevice)
		SDL_DestroyGPUDevice(gpuDevice);

	if (window)
		SDL_DestroyWindow(window);

	SDL_Quit();
}

int main()
{
	try {
		GPUNewtonApp app("GPU Newton", 800, 500);
		app.loop();
	} catch (std::exception &e) {
		log(SDL_LOG_PRIORITY_CRITICAL, "%s", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
