#include <main.hpp>

SDL_GPUShaderFormat GPUNewtonApp::getAvailableShaderFormats()
{
	SDL_GPUShaderFormat ret = 0;
	constexpr SDL_GPUShaderFormat formats[] = {
		SDL_GPU_SHADERFORMAT_PRIVATE,
		SDL_GPU_SHADERFORMAT_SPIRV,
		SDL_GPU_SHADERFORMAT_DXBC,
		SDL_GPU_SHADERFORMAT_DXIL,
		SDL_GPU_SHADERFORMAT_MSL,
		SDL_GPU_SHADERFORMAT_METALLIB
	};

	for (const auto &fmt : formats) {
		if (SDL_GPUSupportsShaderFormats(fmt, nullptr))
			ret |= fmt;
	}
	
	return ret;
}

void GPUNewtonApp::loadDevice()
{
	const auto desiredShaderFormats = SHADER_FORMAT;

	auto shaderFormats = getAvailableShaderFormats();
	if (!(shaderFormats & desiredShaderFormats))
		throw SDLError("desired shader formats are unavailable");

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
	SDL_GPUShaderFormat fmt,
	std::uint32_t numSamplers,
	std::uint32_t numStorageTextures,
	std::uint32_t numStorageBuffers,
	std::uint32_t numUniformBuffers)
{
	SDL_GPUShaderStage stage;
	if (SDL_strstr(fname.data(), ".vert"))
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	else if (SDL_strstr(fname.data(), ".frag"))
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	else
		throw std::runtime_error("invalid shader file extension (vert/frag");

	std::size_t codeSize;
	void *data = SDL_LoadFile(fname.data(), &codeSize);
	if (!data)
		throw SDLError("failed to read shader file");

	SDL_GPUShaderCreateInfo createInfo{};
	createInfo.code_size = codeSize;
	createInfo.code = static_cast<const std::uint8_t *>(data);
	createInfo.entrypoint = "main0"; /* This is only for MSL! */
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
	SDL_GPUShader *vertShader = loadShader(
		VERT_SHADER_FNAME, SHADER_FORMAT, 0, 0, 0, 0
	);
	SDL_GPUShader *fragShader = loadShader(
		FRAG_SHADER_FNAME, SHADER_FORMAT, 0, 0, 0, 0
	);

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
	createInfo.target_info = {
		.num_color_targets = 1,
		.color_target_descriptions = &colorDesc
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

bool GPUNewtonApp::keyPressed(const Keyboard &keyboard, SDL_Keycode key)
{
	return keyboard[SDL_GetScancodeFromKey(key, nullptr)];
}

void GPUNewtonApp::updateCamera(const glm::vec2 &mouse, const Keyboard &keyboard)
{
	/* Where the camera is looking */
	{
		static float yaw;
		static float pitch;

		yaw += mouse.x * MOUSE_SENSITIVITY;
		pitch -= mouse.x * MOUSE_SENSITIVITY;

		if (pitch > 89)
			pitch = 89;
		else if (pitch < -89)
			pitch = -89;

		camLookat = glm::vec3(
			glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)),
			glm::sin(glm::radians(pitch)),
			glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch))
		);
	}

	/* Where the camera is */
	{
		if (keyPressed(keyboard, SDLK_W))
			camPosition += camLookat;

		if (keyPressed(keyboard, SDLK_S))
			camPosition -= camLookat;
	}
}

void GPUNewtonApp::createCombined(glm::mat4 &combined)
{
	//const glm::mat4 viewMatrix = glm::lookAt(camPosition, camLookat, glm::vec3(0.0, 1.0, 0.0));
	const glm::mat4 viewMatrix = glm::lookAt(glm::vec3(0.0,0.0,-1.0), glm::vec3(0.0,0.0,1.0), glm::vec3(0.0, 1.0, 0.0));
	const glm::mat4 modelMatrix = glm::mat4(1.0);
	combined = projMatrix * viewMatrix * modelMatrix;
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
}

void GPUNewtonApp::handleEvents()
{
	SDL_Event event;
	Keyboard keyboard;
	glm::vec2 mouse;
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

	updateCamera(mouse, keyboard);
}

void GPUNewtonApp::loop()
{
	running = true;

	while (running) {
		handleEvents();
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
